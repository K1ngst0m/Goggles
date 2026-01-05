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
  pixi run start [options] [goggles_args...] -- <app> [app_args...]
  pixi run start [options] [goggles_args...] <path/to/app> [app_args...]

Options:
  -p, --preset <name>     Build preset (default: debug)
  --goggles-env VAR=val   Set environment variable for goggles (repeatable)
  --app-env VAR=val       Set environment variable for app (repeatable)
  -h, --help              Show this help

Note: Use '--' to separate goggles args from the app when the app is in PATH.
      Apps specified as paths (containing '/') don't need '--'.

Examples:
  pixi run start -- vkcube
  pixi run start -p release -- vkcube
  pixi run start --input-forwarding -- vkcube
  pixi run start ./build/debug/bin/app
  pixi run start --goggles-env DISPLAY=:0 --app-env DISPLAY=:1 -- vkcube
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
seen_separator=false
GOGGLES_ARGS=()
GOGGLES_ENV=()
APP_ENV=()

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
    --app-env)
      [[ $# -ge 2 ]] || die "--app-env requires VAR=value"
      [[ "$2" == *=* && "${2%%=*}" != "" ]] || die "--app-env: invalid format '$2', expected VAR=value"
      APP_ENV+=("$2")
      shift 2
      ;;
    --app-env=*)
      [[ "${1#*=}" == *=* && "${1#*=}" != =* ]] || die "--app-env: invalid format '${1#*=}', expected VAR=value"
      APP_ENV+=("${1#*=}")
      shift
      ;;
    --goggles-env)
      [[ $# -ge 2 ]] || die "--goggles-env requires VAR=value"
      [[ "$2" == *=* && "${2%%=*}" != "" ]] || die "--goggles-env: invalid format '$2', expected VAR=value"
      GOGGLES_ENV+=("$2")
      shift 2
      ;;
    --goggles-env=*)
      [[ "${1#*=}" == *=* && "${1#*=}" != =* ]] || die "--goggles-env: invalid format '${1#*=}', expected VAR=value"
      GOGGLES_ENV+=("${1#*=}")
      shift
      ;;
    --)
      seen_separator=true
      shift
      break
      ;;
    *)
      break
      ;;
  esac
done

# Collect goggles args until -- or path
if ! $seen_separator; then
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --)
        seen_separator=true
        shift
        break
        ;;
      *)
        # If it looks like an app path (contains /), stop collecting viewer args.
        if [[ "$1" == */* ]]; then
          break
        fi
        GOGGLES_ARGS+=("$1")
        shift
        ;;
    esac
  done
fi

if [[ $# -eq 0 ]]; then
  # If we collected goggles args but have no app, user likely forgot --
  if ! $seen_separator && [[ ${#GOGGLES_ARGS[@]} -gt 0 ]]; then
    last_arg="${GOGGLES_ARGS[-1]}"
    die "Use '--' before apps in PATH (e.g., pixi run start -- $last_arg)"
  fi
  usage
  die "missing <app> argument"
fi

# Enforce -- for PATH apps (no /)
if ! $seen_separator && [[ "$1" != */* ]]; then
  die "Use '--' before apps in PATH (e.g., pixi run start -- $1)"
fi

APP="$1"
shift
APP_ARGS=("$@")

validate_preset "$PRESET"

cd "$REPO_ROOT"

echo "Ensuring preset '$PRESET' is built..."
pixi run dev -p "$PRESET"

VIEWER_BIN="$REPO_ROOT/build/$PRESET/bin/goggles"
[[ -x "$VIEWER_BIN" ]] || die "viewer binary not found at $VIEWER_BIN"

cleanup() {
  if [[ -n "${VIEWER_PID:-}" ]] && kill -0 "$VIEWER_PID" 2>/dev/null; then
    kill "$VIEWER_PID" 2>/dev/null || true
    wait "$VIEWER_PID" 2>/dev/null || true
  fi
}

trap cleanup EXIT INT TERM

if [[ ${#GOGGLES_ARGS[@]} -gt 0 ]]; then
  env "${GOGGLES_ENV[@]}" "$VIEWER_BIN" "${GOGGLES_ARGS[@]}" &
else
  env "${GOGGLES_ENV[@]}" "$VIEWER_BIN" &
fi
VIEWER_PID=$!

sleep 1

set +e
env GOGGLES_CAPTURE=1 "${APP_ENV[@]}" "$APP" "${APP_ARGS[@]}"
APP_STATUS=$?
set -e

exit "$APP_STATUS"
