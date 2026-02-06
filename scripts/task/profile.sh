#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
CMAKE_PRESETS_FILE="${REPO_ROOT}/CMakePresets.json"
DEFAULT_PRESET="profile"
VALID_PRESETS=()

if command -v python3 >/dev/null 2>&1 && [[ -f "${CMAKE_PRESETS_FILE}" ]]; then
  if mapfile -t VALID_PRESETS < <(python3 - "${CMAKE_PRESETS_FILE}" <<'PY'
import json
import sys

path = sys.argv[1]
with open(path, "r", encoding="utf-8") as handle:
    data = json.load(handle)

for preset in data.get("configurePresets", []):
    name = preset.get("name")
    if not name or name.startswith(".") or name.endswith("-i686"):
        continue
    print(name)
PY
  ); then
    :
  else
    VALID_PRESETS=()
  fi
fi

if [[ ${#VALID_PRESETS[@]} -eq 0 ]]; then
  VALID_PRESETS=(debug release relwithdebinfo asan ubsan asan-ubsan test quality profile)
fi

usage() {
  cat <<'EOF'
Usage:
  pixi run profile [options] [goggles_args...] -- <app> [app_args...]

Options:
  -p, --preset <name>   Build preset (default: profile)
  --viewer-port <port>  Tracy data port for goggles process (default: 18086)
  --layer-port <port>   Tracy data port for app-layer process (default: 18087)
  -h, --help            Show this help

Notes:
  - This command always profiles default launch mode (viewer + target app).
  - It writes session artifacts under build/<preset>/profiles/<timestamp-pid>/.
  - Output files:
      viewer.tracy
      layer.tracy
      merged.tracy
      session.json
  - Optional tool overrides:
      GOGGLES_PROFILE_TRACY_CAPTURE_BIN
      GOGGLES_PROFILE_TRACY_CSVEXPORT_BIN
      GOGGLES_PROFILE_TRACY_IMPORT_CHROME_BIN

Examples:
  pixi run profile -- vkcube
  pixi run profile -p profile --app-width 480 --app-height 240 -- vkcube
EOF
}

die() {
  echo "Error: $*" >&2
  exit 1
}

is_valid_preset() {
  local candidate="$1"
  local preset
  for preset in "${VALID_PRESETS[@]}"; do
    if [[ "${preset}" == "${candidate}" ]]; then
      return 0
    fi
  done
  return 1
}

validate_preset() {
  local preset="$1"
  if ! is_valid_preset "${preset}"; then
    printf 'Error: Unknown preset "%s"\nValid presets: %s\n' "${preset}" "${VALID_PRESETS[*]}" >&2
    exit 1
  fi
}

contains_arg() {
  local needle="$1"
  shift
  local arg
  for arg in "$@"; do
    if [[ "${arg}" == "${needle}" ]]; then
      return 0
    fi
  done
  return 1
}

wait_capture() {
  local label="$1"
  local pid="$2"
  local timeout_seconds="$3"
  local output_var="$4"
  local deadline=$((SECONDS + timeout_seconds))

  while kill -0 "${pid}" 2>/dev/null; do
    if (( SECONDS >= deadline )); then
      echo "Capture '${label}' still running; sending SIGINT..."
      kill -INT "${pid}" 2>/dev/null || true
      break
    fi
    sleep 1
  done

  local exit_code=0
  set +e
  wait "${pid}"
  exit_code=$?
  set -e
  printf -v "${output_var}" '%s' "${exit_code}"
}

PRESET="${DEFAULT_PRESET}"
VIEWER_TRACY_PORT="${GOGGLES_PROFILE_VIEWER_PORT:-18086}"
LAYER_TRACY_PORT="${GOGGLES_PROFILE_LAYER_PORT:-18087}"
GOGGLES_ARGS=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help)
      usage
      exit 0
      ;;
    -p)
      [[ $# -ge 2 ]] || die "-p requires a preset name"
      PRESET="$2"
      shift 2
      ;;
    --preset)
      [[ $# -ge 2 ]] || die "--preset requires a value"
      PRESET="$2"
      shift 2
      ;;
    --preset=*)
      PRESET="${1#*=}"
      shift
      ;;
    --viewer-port)
      [[ $# -ge 2 ]] || die "--viewer-port requires a value"
      VIEWER_TRACY_PORT="$2"
      shift 2
      ;;
    --layer-port)
      [[ $# -ge 2 ]] || die "--layer-port requires a value"
      LAYER_TRACY_PORT="$2"
      shift 2
      ;;
    *)
      GOGGLES_ARGS=("$@")
      break
      ;;
  esac
done

if [[ ${#GOGGLES_ARGS[@]} -eq 0 ]]; then
  usage
  die "missing goggles arguments and/or target app command"
fi

if contains_arg "--detach" "${GOGGLES_ARGS[@]}"; then
  die "--detach is not supported by pixi run profile"
fi

if ! contains_arg "--" "${GOGGLES_ARGS[@]}"; then
  die "missing '--' separator before target app command"
fi

if [[ ! "${VIEWER_TRACY_PORT}" =~ ^[0-9]+$ ]] || [[ ! "${LAYER_TRACY_PORT}" =~ ^[0-9]+$ ]]; then
  die "Tracy ports must be numeric"
fi

if [[ "${VIEWER_TRACY_PORT}" == "${LAYER_TRACY_PORT}" ]]; then
  die "viewer and layer Tracy ports must differ"
fi

validate_preset "${PRESET}"

cd "${REPO_ROOT}"

echo "Ensuring preset '${PRESET}' is built..."
pixi run dev -p "${PRESET}"

VIEWER_BIN="${REPO_ROOT}/build/${PRESET}/bin/goggles"
[[ -x "${VIEWER_BIN}" ]] || die "viewer binary not found at ${VIEWER_BIN}"

source <("${REPO_ROOT}/scripts/profiling/ensure_tracy_tools.sh")

TRACY_CAPTURE_BIN="${GOGGLES_PROFILE_TRACY_CAPTURE_BIN:-${TRACY_CAPTURE_BIN}}"
TRACY_CSVEXPORT_BIN="${GOGGLES_PROFILE_TRACY_CSVEXPORT_BIN:-${TRACY_CSVEXPORT_BIN}}"
TRACY_IMPORT_CHROME_BIN="${GOGGLES_PROFILE_TRACY_IMPORT_CHROME_BIN:-${TRACY_IMPORT_CHROME_BIN}}"

SESSION_ROOT="${REPO_ROOT}/build/${PRESET}/profiles"
SESSION_ID="$(date -u +%Y%m%dT%H%M%SZ)-$$"
SESSION_DIR="${SESSION_ROOT}/${SESSION_ID}"
mkdir -p "${SESSION_DIR}"

VIEWER_TRACE="${SESSION_DIR}/viewer.tracy"
LAYER_TRACE="${SESSION_DIR}/layer.tracy"
MERGED_TRACE="${SESSION_DIR}/merged.tracy"
MERGE_SUMMARY="${SESSION_DIR}/merge_summary.json"
MERGED_CHROME="${SESSION_DIR}/merged.chrome.json"
SESSION_MANIFEST="${SESSION_DIR}/session.json"
VIEWER_CAPTURE_LOG="${SESSION_DIR}/viewer_capture.log"
LAYER_CAPTURE_LOG="${SESSION_DIR}/layer_capture.log"

printf -v COMMAND_LINE '%q ' "${VIEWER_BIN}" "${GOGGLES_ARGS[@]}"
COMMAND_LINE="${COMMAND_LINE%" "}"
START_TIME_UTC="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

echo "Starting Tracy capture workers..."
"${TRACY_CAPTURE_BIN}" -a 127.0.0.1 -p "${VIEWER_TRACY_PORT}" -o "${VIEWER_TRACE}" -f \
  >"${VIEWER_CAPTURE_LOG}" 2>&1 &
viewer_capture_pid=$!
"${TRACY_CAPTURE_BIN}" -a 127.0.0.1 -p "${LAYER_TRACY_PORT}" -o "${LAYER_TRACE}" -f \
  >"${LAYER_CAPTURE_LOG}" 2>&1 &
layer_capture_pid=$!

echo "Launching Goggles profile session..."
viewer_exit_code=0
set +e
TRACY_PORT="${VIEWER_TRACY_PORT}" \
GOGGLES_TARGET_TRACY_PORT="${LAYER_TRACY_PORT}" \
"${VIEWER_BIN}" "${GOGGLES_ARGS[@]}"
viewer_exit_code=$?
set -e

echo "Finalizing captures..."
viewer_capture_exit_code=0
layer_capture_exit_code=0
wait_capture viewer "${viewer_capture_pid}" 30 viewer_capture_exit_code
wait_capture layer "${layer_capture_pid}" 30 layer_capture_exit_code

merge_exit_code=0
if [[ -s "${VIEWER_TRACE}" && -s "${LAYER_TRACE}" ]]; then
  echo "Merging traces..."
  set +e
  python3 "${REPO_ROOT}/scripts/profiling/merge_traces.py" \
    --viewer-trace "${VIEWER_TRACE}" \
    --layer-trace "${LAYER_TRACE}" \
    --output-trace "${MERGED_TRACE}" \
    --summary-out "${MERGE_SUMMARY}" \
    --csvexport-bin "${TRACY_CSVEXPORT_BIN}" \
    --import-chrome-bin "${TRACY_IMPORT_CHROME_BIN}" \
    --chrome-out "${MERGED_CHROME}"
  merge_exit_code=$?
  set -e
else
  merge_exit_code=1
fi

END_TIME_UTC="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

PROFILE_VIEWER_CAPTURE_EXIT="${viewer_capture_exit_code}" \
PROFILE_LAYER_CAPTURE_EXIT="${layer_capture_exit_code}" \
PROFILE_MERGE_EXIT="${merge_exit_code}" \
PROFILE_VIEWER_EXIT="${viewer_exit_code}" \
PROFILE_START_TIME_UTC="${START_TIME_UTC}" \
PROFILE_END_TIME_UTC="${END_TIME_UTC}" \
PROFILE_PRESET="${PRESET}" \
PROFILE_COMMAND_LINE="${COMMAND_LINE}" \
PROFILE_VIEWER_PORT="${VIEWER_TRACY_PORT}" \
PROFILE_LAYER_PORT="${LAYER_TRACY_PORT}" \
python3 - "${SESSION_MANIFEST}" "${MERGE_SUMMARY}" <<'PY'
import json
import os
import pathlib
import sys

manifest_path = pathlib.Path(sys.argv[1])
merge_summary_path = pathlib.Path(sys.argv[2])

warnings = []
merge = {}
if merge_summary_path.is_file():
    try:
        merge = json.loads(merge_summary_path.read_text(encoding="utf-8"))
        warnings.extend(merge.get("warnings", []))
    except json.JSONDecodeError:
        warnings.append("Failed to parse merge_summary.json")

viewer_capture_exit = int(os.environ["PROFILE_VIEWER_CAPTURE_EXIT"])
layer_capture_exit = int(os.environ["PROFILE_LAYER_CAPTURE_EXIT"])
merge_exit = int(os.environ["PROFILE_MERGE_EXIT"])
viewer_exit = int(os.environ["PROFILE_VIEWER_EXIT"])

if viewer_capture_exit != 0:
    warnings.append(f"Viewer capture exited with code {viewer_capture_exit}.")
if layer_capture_exit != 0:
    warnings.append(f"Layer capture exited with code {layer_capture_exit}.")
if merge_exit != 0:
    warnings.append(f"Merge pipeline exited with code {merge_exit}.")

status = "success"
if viewer_exit != 0 or viewer_capture_exit != 0 or layer_capture_exit != 0 or merge_exit != 0:
    status = "failed"

manifest = {
    "status": status,
    "timestamps": {
        "started_utc": os.environ["PROFILE_START_TIME_UTC"],
        "ended_utc": os.environ["PROFILE_END_TIME_UTC"],
    },
    "preset": os.environ["PROFILE_PRESET"],
    "command_line": os.environ["PROFILE_COMMAND_LINE"],
    "ports": {
        "viewer": int(os.environ["PROFILE_VIEWER_PORT"]),
        "layer": int(os.environ["PROFILE_LAYER_PORT"]),
    },
    "client_mapping": {
        "viewer": {
            "description": "goggles viewer/compositor process",
            "trace": "viewer.tracy",
            "capture_log": "viewer_capture.log",
        },
        "layer": {
            "description": "target app process with goggles Vulkan layer",
            "trace": "layer.tracy",
            "capture_log": "layer_capture.log",
        },
    },
    "artifacts": {
        "viewer_trace": "viewer.tracy",
        "layer_trace": "layer.tracy",
        "merged_trace": "merged.tracy",
        "merged_chrome_trace": "merged.chrome.json",
        "merge_summary": "merge_summary.json",
    },
    "exit_codes": {
        "viewer_process": viewer_exit,
        "viewer_capture": viewer_capture_exit,
        "layer_capture": layer_capture_exit,
        "merge_pipeline": merge_exit,
    },
    "merge": merge,
    "warnings": warnings,
}

manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
PY

echo "Profile session artifacts: ${SESSION_DIR}"
echo "  - ${VIEWER_TRACE}"
echo "  - ${LAYER_TRACE}"
if [[ -f "${MERGED_TRACE}" ]]; then
  echo "  - ${MERGED_TRACE}"
fi
echo "  - ${SESSION_MANIFEST}"

if [[ "${viewer_exit_code}" -ne 0 ]]; then
  die "goggles exited with code ${viewer_exit_code}; see ${SESSION_MANIFEST}"
fi
if [[ "${viewer_capture_exit_code}" -ne 0 ]]; then
  die "viewer capture failed with code ${viewer_capture_exit_code}; see ${VIEWER_CAPTURE_LOG}"
fi
if [[ "${layer_capture_exit_code}" -ne 0 ]]; then
  die "layer capture failed with code ${layer_capture_exit_code}; see ${LAYER_CAPTURE_LOG}"
fi
if [[ "${merge_exit_code}" -ne 0 ]]; then
  die "trace merge failed; see ${MERGE_SUMMARY}"
fi

exit 0
