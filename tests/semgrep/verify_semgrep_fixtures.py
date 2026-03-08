#!/usr/bin/env python3

from __future__ import annotations

import json
import shutil
import subprocess
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
FIXTURES_ROOT = REPO_ROOT / "tests/semgrep/fixtures"
SEMGREP_BIN = REPO_ROOT / ".pixi/envs/default/bin/semgrep"

EXPECTED_FINDINGS = {
    "goggles-no-std-stream-logging": {
        "positive": ["src/app/positive_logging.cpp"],
        "negative": ["src/app/negative_logging.cpp"],
    },
    "goggles-no-direct-raw-new": {
        "positive": ["src/render/positive_raw_new.cpp"],
        "negative": [
            "src/render/negative_raw_new.cpp",
            "src/render/chain/api/c/negative_raw_new_exception.cpp",
        ],
    },
    "goggles-no-direct-delete": {
        "positive": ["src/render/positive_delete.cpp"],
        "negative": [
            "src/render/negative_delete.cpp",
            "src/render/chain/api/c/negative_delete_exception.cpp",
        ],
    },
    "goggles-no-raw-vulkan-handles": {
        "positive": ["src/render/chain/positive_raw_vk_handle.cpp"],
        "negative": [
            "src/render/chain/negative_raw_vk_handle.cpp",
            "src/capture/vk_layer/negative_raw_vk_handle_exception.cpp",
        ],
    },
    "goggles-no-discarded-vulkan-result": {
        "positive": ["src/render/positive_discarded_wait_idle.cpp"],
        "negative": ["src/render/negative_discarded_wait_idle.cpp"],
    },
    "goggles-no-render-std-thread": {
        "positive": ["src/render/positive_render_thread.cpp"],
        "negative": ["src/render/negative_render_thread.cpp"],
    },
    "goggles-no-vulkan-unique-or-raii": {
        "positive": ["src/render/positive_vulkan_unique.cpp"],
        "negative": ["src/render/negative_vulkan_unique.cpp"],
    },
    "goggles-no-using-namespace-headers": {
        "positive": ["tests/positive_using_namespace.hpp"],
        "negative": ["tests/negative_using_namespace.hpp"],
    },
}


def verification_target(relative_fixture_path: str) -> Path:
    fixture_path = Path(relative_fixture_path)
    return fixture_path.parent / "__semgrep_verify__" / fixture_path.name


def materialize_fixture(relative_fixture_path: str) -> Path:
    source = FIXTURES_ROOT / relative_fixture_path
    target = REPO_ROOT / verification_target(relative_fixture_path)
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)
    subprocess.run(
        ["git", "add", str(target.relative_to(REPO_ROOT))], cwd=REPO_ROOT, check=True
    )
    return target


def cleanup_fixture(target: Path) -> None:
    relative_target = target.relative_to(REPO_ROOT)
    subprocess.run(
        ["git", "rm", "--cached", "-f", str(relative_target)],
        cwd=REPO_ROOT,
        check=False,
        capture_output=True,
        text=True,
    )
    target.unlink(missing_ok=True)
    parent = target.parent
    while parent != REPO_ROOT and parent.name == "__semgrep_verify__":
        try:
            parent.rmdir()
        except OSError:
            break
        parent = parent.parent


def scan_fixture(relative_fixture_path: str) -> set[str]:
    target = materialize_fixture(relative_fixture_path)
    try:
        relative_target = target.relative_to(REPO_ROOT).as_posix()
        target_dir = target.parent.relative_to(REPO_ROOT).as_posix()
        command = [
            str(SEMGREP_BIN),
            "scan",
            "--json",
            "--metrics=off",
            "--config",
            ".semgrep.yml",
            "--config",
            ".semgrep/rules",
            target_dir,
        ]
        completed = subprocess.run(
            command, cwd=REPO_ROOT, capture_output=True, text=True, check=False
        )
        if not completed.stdout:
            raise RuntimeError(
                f"semgrep produced no JSON output for {relative_fixture_path}\nSTDERR:\n{completed.stderr}"
            )
        results = json.loads(completed.stdout)
        rule_ids: set[str] = set()
        for entry in results.get("results", []):
            if entry["path"] == relative_target:
                rule_ids.add(entry["check_id"].split(".")[-1])
        return rule_ids
    finally:
        cleanup_fixture(target)


def validate() -> dict:
    missing: list[dict[str, str]] = []
    unexpected: list[dict[str, str]] = []
    actual_positive_total = 0

    for rule_id, expectation in EXPECTED_FINDINGS.items():
        for relative_path in expectation["positive"]:
            rule_ids = scan_fixture(relative_path)
            if rule_id in rule_ids:
                actual_positive_total += 1
            else:
                missing.append({"rule_id": rule_id, "path": relative_path})
        for relative_path in expectation["negative"]:
            rule_ids = scan_fixture(relative_path)
            if rule_id in rule_ids:
                unexpected.append({"rule_id": rule_id, "path": relative_path})

    expected_positive_total = sum(
        len(expectation["positive"]) for expectation in EXPECTED_FINDINGS.values()
    )
    return {
        "expected_positive_total": expected_positive_total,
        "actual_positive_total": actual_positive_total,
        "missing": missing,
        "unexpected": unexpected,
    }


def main() -> int:
    if not SEMGREP_BIN.exists():
        print(
            json.dumps(
                {
                    "status": "error",
                    "reason": f"missing Pixi Semgrep binary at {SEMGREP_BIN}",
                },
                indent=2,
            )
        )
        return 1

    summary = validate()
    status = "pass" if not summary["missing"] and not summary["unexpected"] else "fail"
    output = {
        "status": status,
        "rule_count": len(EXPECTED_FINDINGS),
        "expected_positive_total": summary["expected_positive_total"],
        "actual_positive_total": summary["actual_positive_total"],
        "missing": summary["missing"],
        "unexpected": summary["unexpected"],
    }
    print(json.dumps(output, indent=2, sort_keys=True))
    return 0 if status == "pass" else 1


if __name__ == "__main__":
    sys.exit(main())
