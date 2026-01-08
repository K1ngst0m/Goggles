#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
CMAKE_PRESETS_FILE="$REPO_ROOT/CMakePresets.json"
DEFAULT_PRESET="debug"
VALID_PRESETS=()

if command -v python3 >/dev/null 2>&1 && [[ -f "$CMAKE_PRESETS_FILE" ]]; then
  if mapfile -t VALID_PRESETS < <(python3 - "$CMAKE_PRESETS_FILE" <<'PY'
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
  pixi run start [options] --detach
  pixi run start [options] [goggles_args...] -- <app> [app_args...]

Options:
  -p, --preset <name>   Build preset (default: debug)
  -h, --help            Show this help

Notes:
  - Default mode (no --detach) launches the target app with capture + input forwarding enabled.
  - Use '--detach' to start viewer-only mode (manual app launch).
  - When launching an app, '--' is required to separate Goggles args from app args.

Examples:
  pixi run start -- vkcube
  pixi run start -p profile --app-width 480 --app-height 240 -- vkcube
  pixi run start --detach
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
    if [[ "$preset" == "$candidate" ]]; then
      return 0
    fi
  done
  return 1
}

validate_preset() {
  local preset="$1"
  if ! is_valid_preset "$preset"; then
    printf 'Error: Unknown preset "%s"\nValid presets: %s\n' "$preset" "${VALID_PRESETS[*]}" >&2
    exit 1
  fi
}

PRESET="$DEFAULT_PRESET"
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

validate_preset "$PRESET"

cd "$REPO_ROOT"

echo "Ensuring preset '$PRESET' is built..."
pixi run dev -p "$PRESET"

VIEWER_BIN="$REPO_ROOT/build/$PRESET/bin/goggles"
[[ -x "$VIEWER_BIN" ]] || die "viewer binary not found at $VIEWER_BIN"

exec "$VIEWER_BIN" "${GOGGLES_ARGS[@]}"
