#!/usr/bin/env bash
# Install or repair the managed pre-commit formatting hook.
# Used by: pixi run init and scripts/ensure-init.sh auto-recovery.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
HOOK_SRC="${PROJECT_ROOT}/scripts/pre-commit-format.sh"
EXPECTED_HOOK_TARGET="$(readlink -f "$HOOK_SRC")"
MANAGED_MARKER="# goggles-managed-pre-commit-hook"
printf -v HOOK_SRC_SHELL %q "$HOOK_SRC"
EXPECTED_EXEC_LINE="exec ${HOOK_SRC_SHELL} \"\$@\""

if ! git -C "$PROJECT_ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  echo "[pre-commit] Not a git checkout; skipping hook install."
  exit 0
fi

HOOKS_DIR=""
HOOKS_PATH="$(git -C "$PROJECT_ROOT" config --get core.hooksPath || true)"
if [[ -n "$HOOKS_PATH" ]]; then
  if [[ "$HOOKS_PATH" = /* ]]; then
    HOOKS_DIR="$HOOKS_PATH"
  else
    HOOKS_DIR="${PROJECT_ROOT}/${HOOKS_PATH}"
  fi
else
  HOOKS_DIR_REL="$(git -C "$PROJECT_ROOT" rev-parse --git-path hooks)"
  if [[ "$HOOKS_DIR_REL" = /* ]]; then
    HOOKS_DIR="$HOOKS_DIR_REL"
  else
    HOOKS_DIR="${PROJECT_ROOT}/${HOOKS_DIR_REL}"
  fi
fi

HOOK_DST="${HOOKS_DIR}/pre-commit"

has_managed_marker() {
  [[ -f "$HOOK_DST" && ! -L "$HOOK_DST" ]] || return 1
  grep -Fq "$MANAGED_MARKER" "$HOOK_DST"
}

is_legacy_managed_hook() {
  [[ -L "$HOOK_DST" ]] || return 1

  local resolved_hook
  resolved_hook="$(readlink -f "$HOOK_DST" 2>/dev/null || true)"

  [[ -n "$resolved_hook" && "$resolved_hook" == "$EXPECTED_HOOK_TARGET" ]]
}

is_current_managed_wrapper() {
  has_managed_marker || return 1
  grep -Fqx "$EXPECTED_EXEC_LINE" "$HOOK_DST"
}

write_managed_hook() {
  local tmp_hook

  mkdir -p "$HOOKS_DIR"
  tmp_hook="$(mktemp "${HOOK_DST}.tmp.XXXXXX")"

  cat >"$tmp_hook" <<EOF
#!/usr/bin/env bash
$MANAGED_MARKER
$EXPECTED_EXEC_LINE
EOF

  chmod +x "$HOOK_SRC" "$tmp_hook"
  mv -f "$tmp_hook" "$HOOK_DST"
}

if is_current_managed_wrapper; then
  echo "[pre-commit] Managed hook already installed -> $HOOK_DST"
  exit 0
fi

action="Installed"

if [[ -L "$HOOK_DST" ]]; then
  if ! is_legacy_managed_hook; then
    echo "[pre-commit] Existing hook symlink is not managed; leave it unchanged: $HOOK_DST" >&2
    exit 1
  fi

  action="Migrated"
elif has_managed_marker; then
  action="Repaired"
elif [[ -e "$HOOK_DST" ]]; then
  echo "[pre-commit] Existing hook not managed; leave it unchanged: $HOOK_DST" >&2
  exit 1
fi

write_managed_hook
echo "[pre-commit] ${action} managed hook -> $HOOK_DST"
