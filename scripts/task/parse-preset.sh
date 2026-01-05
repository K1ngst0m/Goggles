#!/usr/bin/env bash
# Common preset argument parser for pixi tasks
# Usage: source this script, then use $PRESET and remaining args in "$@"

set -euo pipefail

PRESET="debug"

while [[ $# -gt 0 ]]; do
  case "$1" in
    -p|--preset)
      [[ $# -ge 2 ]] || { echo "Error: $1 requires a preset name" >&2; exit 1; }
      PRESET="$2"
      shift 2
      ;;
    --preset=*)
      PRESET="${1#*=}"
      shift
      ;;
    --)
      shift
      break
      ;;
    -*)
      echo "Error: Unknown option: $1" >&2
      exit 1
      ;;
    *)
      break
      ;;
  esac
done

export PRESET
