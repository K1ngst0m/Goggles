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
    if not name or name.startswith("."):
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
  -h, --help            Show this help

Notes:
  - This command always profiles default launch mode (viewer + target app).
  - It writes session artifacts under build/<preset>/profiles/<timestamp-pid>/.
  - Output files:
      viewer.tracy
      viewer_capture.log
      session.json
  - Optional tool overrides:
      GOGGLES_PROFILE_TRACY_CAPTURE_BIN

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

if ! contains_arg "--" "${GOGGLES_ARGS[@]}"; then
  die "missing '--' separator before target app command"
fi

if [[ ! "${VIEWER_TRACY_PORT}" =~ ^[0-9]+$ ]]; then
  die "Tracy port must be numeric"
fi

validate_preset "${PRESET}"

cd "${REPO_ROOT}"

echo "Ensuring preset '${PRESET}' is built..."
pixi run build -p "${PRESET}"

VIEWER_BIN="${REPO_ROOT}/build/${PRESET}/bin/goggles"
[[ -x "${VIEWER_BIN}" ]] || die "viewer binary not found at ${VIEWER_BIN}"

source <("${REPO_ROOT}/scripts/profiling/ensure_tracy_tools.sh")

TRACY_CAPTURE_BIN="${GOGGLES_PROFILE_TRACY_CAPTURE_BIN:-${TRACY_CAPTURE_BIN}}"

SESSION_ROOT="${REPO_ROOT}/build/${PRESET}/profiles"
SESSION_ID="$(date -u +%Y%m%dT%H%M%SZ)-$$"
SESSION_DIR="${SESSION_ROOT}/${SESSION_ID}"
mkdir -p "${SESSION_DIR}"

VIEWER_TRACE="${SESSION_DIR}/viewer.tracy"
SESSION_MANIFEST="${SESSION_DIR}/session.json"
VIEWER_CAPTURE_LOG="${SESSION_DIR}/viewer_capture.log"

printf -v COMMAND_LINE '%q ' "${VIEWER_BIN}" "${GOGGLES_ARGS[@]}"
COMMAND_LINE="${COMMAND_LINE%" "}"
START_TIME_UTC="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

echo "Starting Tracy capture worker..."
"${TRACY_CAPTURE_BIN}" -a 127.0.0.1 -p "${VIEWER_TRACY_PORT}" -o "${VIEWER_TRACE}" -f \
  >"${VIEWER_CAPTURE_LOG}" 2>&1 &
viewer_capture_pid=$!

echo "Launching Goggles profile session..."
viewer_exit_code=0
set +e
TRACY_PORT="${VIEWER_TRACY_PORT}" \
"${VIEWER_BIN}" "${GOGGLES_ARGS[@]}"
viewer_exit_code=$?
set -e

echo "Finalizing capture..."
viewer_capture_exit_code=0
wait_capture viewer "${viewer_capture_pid}" 30 viewer_capture_exit_code

END_TIME_UTC="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

PROFILE_VIEWER_CAPTURE_EXIT="${viewer_capture_exit_code}" \
PROFILE_VIEWER_EXIT="${viewer_exit_code}" \
PROFILE_START_TIME_UTC="${START_TIME_UTC}" \
PROFILE_END_TIME_UTC="${END_TIME_UTC}" \
PROFILE_PRESET="${PRESET}" \
PROFILE_COMMAND_LINE="${COMMAND_LINE}" \
PROFILE_VIEWER_PORT="${VIEWER_TRACY_PORT}" \
python3 - "${SESSION_MANIFEST}" <<'PY'
import json
import os
import pathlib
import sys

manifest_path = pathlib.Path(sys.argv[1])

warnings = []

viewer_capture_exit = int(os.environ["PROFILE_VIEWER_CAPTURE_EXIT"])
viewer_exit = int(os.environ["PROFILE_VIEWER_EXIT"])

if viewer_capture_exit != 0:
    warnings.append(f"Viewer capture exited with code {viewer_capture_exit}.")

status = "success"
if viewer_exit != 0 or viewer_capture_exit != 0:
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
    },
    "client_mapping": {
        "viewer": {
            "description": "goggles viewer/compositor process",
            "trace": "viewer.tracy",
            "capture_log": "viewer_capture.log",
        },
    },
    "artifacts": {
        "viewer_trace": "viewer.tracy",
    },
    "exit_codes": {
        "viewer_process": viewer_exit,
        "viewer_capture": viewer_capture_exit,
    },
    "warnings": warnings,
}

manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
PY

echo "Profile session artifacts: ${SESSION_DIR}"
echo "  - ${VIEWER_TRACE}"
echo "  - ${SESSION_MANIFEST}"

if [[ "${viewer_exit_code}" -ne 0 ]]; then
  die "goggles exited with code ${viewer_exit_code}; see ${SESSION_MANIFEST}"
fi
if [[ "${viewer_capture_exit_code}" -ne 0 ]]; then
  die "viewer capture failed with code ${viewer_capture_exit_code}; see ${VIEWER_CAPTURE_LOG}"
fi

exit 0
