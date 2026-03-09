#!/usr/bin/env bash
# Ensure the managed pre-commit hook exists before guarded Pixi tasks.
# Auto-repair missing or stale managed hooks, but never replace unmanaged hooks.

set -euo pipefail

# Skip in CI - no pre-commit hook needed
if [[ "${CI:-}" == "true" ]]; then
  exit 0
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
HOOK_SRC="${PROJECT_ROOT}/scripts/pre-commit-format.sh"
EXPECTED_HOOK_TARGET="$(readlink -f "$HOOK_SRC")"
MANAGED_MARKER="# goggles-managed-pre-commit-hook"
printf -v HOOK_SRC_SHELL %q "$HOOK_SRC"
EXPECTED_EXEC_LINE="exec ${HOOK_SRC_SHELL} \"\$@\""

if ! git -C "$PROJECT_ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  echo -e "\033[31m[ERROR] Not a git checkout.\033[0m" >&2
  exit 1
fi

HOOKS_PATH="$(git -C "$PROJECT_ROOT" config --get core.hooksPath || true)"
if [[ -n "$HOOKS_PATH" ]]; then
  if [[ "$HOOKS_PATH" = /* ]]; then
    HOOK="$HOOKS_PATH/pre-commit"
  else
    HOOK="$PROJECT_ROOT/$HOOKS_PATH/pre-commit"
  fi
else
  HOOKS_DIR_REL="$(git -C "$PROJECT_ROOT" rev-parse --git-path hooks)"
  if [[ "$HOOKS_DIR_REL" = /* ]]; then
    HOOK="$HOOKS_DIR_REL/pre-commit"
  else
    HOOK="$PROJECT_ROOT/$HOOKS_DIR_REL/pre-commit"
  fi
fi

has_managed_marker() {
  [[ -f "$HOOK" && ! -L "$HOOK" ]] || return 1
  grep -Fq "$MANAGED_MARKER" "$HOOK"
}

is_legacy_managed_hook() {
  [[ -L "$HOOK" ]] || return 1

  local resolved_hook
  resolved_hook="$(readlink -f "$HOOK" 2>/dev/null || true)"

  [[ -n "$resolved_hook" && "$resolved_hook" == "$EXPECTED_HOOK_TARGET" ]]
}

is_current_managed_wrapper() {
  has_managed_marker || return 1
  grep -Fqx "$EXPECTED_EXEC_LINE" "$HOOK"
}

is_repairable_managed_hook() {
  has_managed_marker
}

is_managed_hook() {
  is_legacy_managed_hook || is_current_managed_wrapper
}

if is_managed_hook; then
  exit 0
fi

if [[ ! -e "$HOOK" ]]; then
  echo "[pre-commit] Managed hook missing; reinstalling."
elif is_repairable_managed_hook; then
  echo "[pre-commit] Managed hook is stale; reinstalling."
elif [[ -L "$HOOK" ]]; then
  echo -e "\033[31m[ERROR] Found an unmanaged pre-commit hook symlink at $HOOK. Move or remove it, then rerun your Pixi command.\033[0m" >&2
  exit 1
elif [[ -e "$HOOK" ]]; then
  echo -e "\033[31m[ERROR] Found an unmanaged pre-commit hook at $HOOK. Move or remove it, then rerun your Pixi command.\033[0m" >&2
  exit 1
fi

bash "${PROJECT_ROOT}/scripts/install-pre-commit-hook.sh"

if ! is_managed_hook; then
  echo -e "\033[31m[ERROR] Failed to install the managed pre-commit hook at $HOOK.\033[0m" >&2
  exit 1
fi
