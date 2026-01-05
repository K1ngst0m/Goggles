#!/usr/bin/env bash
# Comprehensive CLI test script for pixi tasks
# Run: bash scripts/test-cli.sh

set -uo pipefail
set +o pipefail  # Disable for grep tests on error outputs

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

PASS=0
FAIL=0

pass() { echo -e "${GREEN}PASS${NC}: $1"; ((PASS++)); }
fail() { echo -e "${RED}FAIL${NC}: $1"; ((FAIL++)); }
section() { echo -e "\n${YELLOW}## $1${NC}"; }

echo "======================================"
echo "Pixi Task CLI Test Suite"
echo "======================================"

# ============================================
# HELP COMMAND
# ============================================
section "Help Command"

if pixi run help 2>&1 | grep -q "Pixi Tasks"; then
  pass "help shows usage"
else
  fail "help shows usage"
fi

if pixi run help 2>&1 | grep -q "\-p, --preset"; then
  pass "help shows -p flag"
else
  fail "help shows -p flag"
fi

# ============================================
# PRESET VALIDATION (Security)
# ============================================
section "Preset Validation - Security"

# Path traversal attacks
for bad_preset in "../.." "../../etc" "/etc" "/tmp" "a/b" "./foo" "foo/../bar"; do
  if pixi run clean -p "$bad_preset" 2>&1 | grep -q "Invalid preset"; then
    pass "blocks '$bad_preset'"
  else
    fail "blocks '$bad_preset'"
  fi
done

# Command injection attempts
for bad_preset in 'foo;ls' 'foo&&ls' 'foo|ls' '$(whoami)' '`whoami`'; do
  if pixi run clean -p "$bad_preset" 2>&1 | grep -q "Invalid preset"; then
    pass "blocks '$bad_preset'"
  else
    fail "blocks '$bad_preset'"
  fi
done

section "Preset Validation - Valid"

# Valid presets
for good_preset in "debug" "release" "Debug" "Release" "my-preset" "my_preset" "preset123"; do
  # Just check it doesn't say "Invalid preset" (may fail for other reasons like preset not existing)
  if pixi run clean -p "$good_preset" 2>&1 | grep -q "Invalid preset"; then
    fail "accepts '$good_preset'"
  else
    pass "accepts '$good_preset'"
  fi
done

# ============================================
# START COMMAND - Basic
# ============================================
section "Start Command - Help & Usage"

if pixi run start --help 2>&1 | grep -q "Usage:"; then
  pass "start --help shows usage"
else
  fail "start --help shows usage"
fi

if pixi run start -h 2>&1 | grep -q "Usage:"; then
  pass "start -h shows usage"
else
  fail "start -h shows usage"
fi

if pixi run start 2>&1 | grep -q "missing <app>"; then
  pass "start without app shows error"
else
  fail "start without app shows error"
fi

# ============================================
# START COMMAND - PATH Apps (-- optional)
# ============================================
section "Start Command - PATH Apps"

# Both with and without -- should work
if pixi run start vkcube 2>&1 | grep -q "Ensuring preset"; then
  pass "start vkcube (no --)"
else
  fail "start vkcube (no --)"
fi

if pixi run start -- vkcube 2>&1 | grep -q "Ensuring preset"; then
  pass "start -- vkcube (with --)"
else
  fail "start -- vkcube (with --)"
fi

if pixi run start vulkaninfo 2>&1 | grep -q "Ensuring preset"; then
  pass "start vulkaninfo (no --)"
else
  fail "start vulkaninfo (no --)"
fi

# ============================================
# START COMMAND - Preset Flag
# ============================================
section "Start Command - Preset Flag"

# -p flag (use timeout since these actually try to build)
if timeout 3s pixi run start -p debug vkcube 2>&1 | grep -q "Invalid preset"; then
  fail "start -p debug works"
else
  pass "start -p debug works"
fi

if timeout 3s pixi run start --preset release vkcube 2>&1 | grep -q "Invalid preset"; then
  fail "start --preset release works"
else
  pass "start --preset release works"
fi

if timeout 3s pixi run start --preset=debug vkcube 2>&1 | grep -q "Invalid preset"; then
  fail "start --preset=debug works"
else
  pass "start --preset=debug works"
fi

# Missing preset value
if pixi run start -p 2>&1 | grep -q "requires"; then
  pass "start -p without value shows error"
else
  fail "start -p without value shows error"
fi

# ============================================
# START COMMAND - Path Apps (no -- needed)
# ============================================
section "Start Command - Path Apps"

# Apps with / don't need --
if pixi run start ./foo 2>&1 | grep -q "Use '--'"; then
  fail "start ./foo doesn't require --"
else
  pass "start ./foo doesn't require --"
fi

if pixi run start /usr/bin/foo 2>&1 | grep -q "Use '--'"; then
  fail "start /usr/bin/foo doesn't require --"
else
  pass "start /usr/bin/foo doesn't require --"
fi

if pixi run start ../foo 2>&1 | grep -q "Use '--'"; then
  fail "start ../foo doesn't require --"
else
  pass "start ../foo doesn't require --"
fi

# ============================================
# BUILD COMMAND
# ============================================
section "Build Command"

# Valid preset flags (use timeout since build actually runs)
if timeout 2s pixi run build -p debug 2>&1 | grep -q "Error:"; then
  fail "build -p debug"
else
  pass "build -p debug"
fi

if timeout 2s pixi run build --preset release 2>&1 | grep -q "Error:"; then
  fail "build --preset release"
else
  pass "build --preset release"
fi

if timeout 2s pixi run build --preset=debug 2>&1 | grep -q "Error:"; then
  fail "build --preset=debug"
else
  pass "build --preset=debug"
fi

# Default preset (no flag)
if timeout 2s pixi run build 2>&1 | grep -q "Error:"; then
  fail "build (default preset)"
else
  pass "build (default preset)"
fi

# ============================================
# CLEAN COMMAND
# ============================================
section "Clean Command"

if pixi run clean -p debug 2>&1 | grep -qE "(Removed|Nothing to clean|does not exist)"; then
  pass "clean -p debug"
else
  fail "clean -p debug"
fi

if pixi run clean --preset release 2>&1 | grep -qE "(Removed|Nothing to clean|does not exist)"; then
  pass "clean --preset release"
else
  fail "clean --preset release"
fi

# ============================================
# TEST COMMAND
# ============================================
section "Test Command"

# Should use ctest --preset (check it doesn't error on arg parsing)
if timeout 2s pixi run test -p debug 2>&1 | grep -q "Unknown option"; then
  fail "test -p debug"
else
  pass "test -p debug"
fi

# ============================================
# INVALID FLAGS
# ============================================
section "Invalid Flags"

if pixi run build --invalid-flag 2>&1 | grep -q "Unknown option"; then
  pass "build rejects --invalid-flag"
else
  fail "build rejects --invalid-flag"
fi

if pixi run clean --bad 2>&1 | grep -q "Unknown option"; then
  pass "clean rejects --bad"
else
  fail "clean rejects --bad"
fi

# start passes unknown flags to goggles (not an error)
if timeout 3s pixi run start --unknown-flag vkcube 2>&1 | grep -q "Ensuring preset"; then
  pass "start passes --unknown-flag to goggles"
else
  fail "start passes --unknown-flag to goggles"
fi

# ============================================
# EDGE CASES
# ============================================
section "Edge Cases"

# Empty preset
if pixi run build -p "" 2>&1 | grep -q "Invalid preset"; then
  pass "rejects empty preset"
else
  fail "rejects empty preset"
fi

# Preset with spaces (should fail validation)
if pixi run build -p "foo bar" 2>&1 | grep -q "Invalid preset"; then
  pass "rejects preset with spaces"
else
  fail "rejects preset with spaces"
fi

# Double -- handling
if pixi run start -- -- vkcube 2>&1 | grep -q "Use '--'"; then
  fail "handles double --"
else
  pass "handles double --"
fi

# ============================================
# SUMMARY
# ============================================
echo -e "\n======================================"
TOTAL=$((PASS + FAIL))
echo -e "Results: ${GREEN}$PASS passed${NC}, ${RED}$FAIL failed${NC} (total: $TOTAL)"
echo "======================================"

[[ $FAIL -eq 0 ]]
