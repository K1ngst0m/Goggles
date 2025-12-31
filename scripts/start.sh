#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
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
  pixi run start [preset] <app> [app_args...]
  pixi run start --preset <preset> <app> [app_args...]

Examples:
  pixi run start vkcube --wsi xcb
  pixi run start release vkcube --wsi xcb
  pixi run start -- preset_named_like_app ./my_app --flag value
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
preset_explicit=false
positional_preset_allowed=true

while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help)
      usage
      exit 0
      ;;
    --preset)
      [[ $# -ge 2 ]] || die "--preset requires a value"
      PRESET="$2"
      preset_explicit=true
      shift 2
      ;;
    --preset=*)
      PRESET="${1#*=}"
      preset_explicit=true
      shift
      ;;
    --)
      positional_preset_allowed=false
      shift
      break
      ;;
    *)
      break
      ;;
  esac
done

if ! $preset_explicit && $positional_preset_allowed && [[ $# -gt 0 ]] && is_valid_preset "$1"; then
  PRESET="$1"
  preset_explicit=true
  shift
fi

if [[ $# -eq 0 ]]; then
  usage
  die "missing <app> argument"
fi

APP="$1"
shift
APP_ARGS=("$@")

validate_preset "$PRESET"

cd "$REPO_ROOT"

echo "Ensuring preset '$PRESET' is built..."
pixi run dev "$PRESET"

VIEWER_BIN="$REPO_ROOT/build/$PRESET/bin/goggles"
[[ -x "$VIEWER_BIN" ]] || die "viewer binary not found at $VIEWER_BIN"

cleanup() {
  if [[ -n "${VIEWER_PID:-}" ]] && kill -0 "$VIEWER_PID" 2>/dev/null; then
    kill "$VIEWER_PID" 2>/dev/null || true
    wait "$VIEWER_PID" 2>/dev/null || true
  fi
}

trap cleanup EXIT INT TERM

"$VIEWER_BIN" &
VIEWER_PID=$!

# Give the viewer a moment to initialize before attaching.
sleep 1

set +e
GOGGLES_CAPTURE=1 "$APP" "${APP_ARGS[@]}"
APP_STATUS=$?
set -e

exit "$APP_STATUS"
