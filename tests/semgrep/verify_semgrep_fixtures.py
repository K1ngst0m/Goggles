#!/usr/bin/env python3

from __future__ import annotations

import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
FIXTURES_ROOT = REPO_ROOT / "tests/semgrep/fixtures"
SEMGREP_BIN = REPO_ROOT / ".pixi/envs/default/bin/semgrep"

EXPECTED_FINDINGS = {
    "goggles-no-std-stream-logging": {
        "positive": [
            "src/app/positive_logging.cpp",
            "src/app/positive_logging_printf.cpp",
        ],
        "negative": [
            "src/app/negative_logging.cpp",
            "src/app/negative_logging_printf.cpp",
        ],
    },
    "goggles-no-direct-raw-new": {
        "positive": ["src/render/positive_raw_new.cpp"],
        "negative": [
            "src/render/negative_raw_new.cpp",
            "src/render/chain/api/c/negative_raw_new_exception.cpp",
        ],
    },
    "goggles-no-direct-delete": {
        "positive": [
            "src/render/positive_delete.cpp",
            "src/render/positive_delete_array.cpp",
        ],
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
        "positive": [
            "src/render/positive_discarded_wait_idle.cpp",
            "src/render/positive_discarded_wait_idle_arrow.cpp",
        ],
        "negative": ["src/render/negative_discarded_wait_idle.cpp"],
    },
    "goggles-no-render-std-thread": {
        "positive": ["src/render/positive_render_thread.cpp"],
        "negative": ["src/render/negative_render_thread.cpp"],
    },
    "goggles-no-vulkan-unique-or-raii": {
        "positive": ["src/render/positive_vulkan_unique.cpp"],
        "negative": [
            "src/render/negative_vulkan_unique.cpp",
            "src/capture/vk_layer/negative_vulkan_unique_exception.cpp",
        ],
    },
    "goggles-no-using-namespace-headers": {
        "positive": ["tests/positive_using_namespace.hpp"],
        "negative": ["tests/negative_using_namespace.hpp"],
    },
}


def verification_target(relative_fixture_path: str) -> Path:
    fixture_path = Path(relative_fixture_path)
    return fixture_path.parent / "__semgrep_verify__" / fixture_path.name


def materialize_fixture(workspace_root: Path, relative_fixture_path: str) -> Path:
    source = FIXTURES_ROOT / relative_fixture_path
    target = workspace_root / verification_target(relative_fixture_path)
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)
    return target


def initialize_workspace(workspace_root: Path) -> None:
    shutil.copy2(REPO_ROOT / ".semgrep.yml", workspace_root / ".semgrep.yml")
    shutil.copy2(REPO_ROOT / ".semgrepignore", workspace_root / ".semgrepignore")
    shutil.copytree(REPO_ROOT / ".semgrep", workspace_root / ".semgrep")


def scan_fixture(relative_fixture_path: str) -> set[str]:
    with tempfile.TemporaryDirectory(prefix="goggles-semgrep-") as temp_dir:
        workspace_root = Path(temp_dir)
        initialize_workspace(workspace_root)
        target = materialize_fixture(workspace_root, relative_fixture_path)
        relative_target = target.relative_to(workspace_root).as_posix()
        command = [
            str(SEMGREP_BIN),
            "scan",
            "--json",
            "--metrics=off",
            "--config",
            ".semgrep.yml",
            "--config",
            ".semgrep/rules",
            ".",
        ]
        completed = subprocess.run(
            command, cwd=workspace_root, capture_output=True, text=True, check=False
        )
        if not completed.stdout:
            raise RuntimeError(
                f"semgrep produced no JSON output for {relative_fixture_path}\nSTDERR:\n{completed.stderr}"
            )
        try:
            results = json.loads(completed.stdout)
        except json.JSONDecodeError as exc:
            raise RuntimeError(
                f"semgrep produced invalid JSON for {relative_fixture_path}\n"
                f"STDERR:\n{completed.stderr}\nSTDOUT:\n{completed.stdout}"
            ) from exc
        errors = results.get("errors", [])
        if completed.returncode != 0 or errors:
            raise RuntimeError(
                f"semgrep scan failed for {relative_fixture_path}\n"
                f"target={relative_target}\n"
                f"returncode={completed.returncode}\n"
                f"STDERR:\n{completed.stderr}\n"
                f"JSON errors:\n{json.dumps(errors, indent=2, sort_keys=True)}"
            )
        rule_ids: set[str] = set()
        for entry in results.get("results", []):
            if entry["path"] == relative_target:
                rule_ids.add(entry["check_id"].split(".")[-1])
        return rule_ids


def build_expected_rule_ids_by_fixture() -> dict[str, set[str]]:
    expected_rule_ids_by_fixture: dict[str, set[str]] = {}

    for rule_id, expectation in EXPECTED_FINDINGS.items():
        for relative_path in expectation["positive"]:
            expected_rule_ids_by_fixture.setdefault(relative_path, set()).add(rule_id)
        for relative_path in expectation["negative"]:
            expected_rule_ids_by_fixture.setdefault(relative_path, set())

    return expected_rule_ids_by_fixture


def validate() -> dict:
    missing: list[dict[str, str]] = []
    unexpected: list[dict[str, str]] = []
    actual_positive_total = 0
    expected_rule_ids_by_fixture = build_expected_rule_ids_by_fixture()

    for relative_path, expected_rule_ids in sorted(
        expected_rule_ids_by_fixture.items()
    ):
        actual_rule_ids = scan_fixture(relative_path)

        for rule_id in sorted(expected_rule_ids - actual_rule_ids):
            missing.append({"rule_id": rule_id, "path": relative_path})

        for rule_id in sorted(actual_rule_ids - expected_rule_ids):
            unexpected.append({"rule_id": rule_id, "path": relative_path})

        if actual_rule_ids == expected_rule_ids:
            actual_positive_total += len(expected_rule_ids)

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
