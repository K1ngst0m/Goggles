#!/usr/bin/env bash
# Comprehensive test suite for pixi run start
# Tests argument parsing - checks error messages and exit codes
# Run: bash scripts/test-start.sh

set -uo pipefail
set +o pipefail  # Disable for tests

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

PASS=0
FAIL=0

pass() { echo -e "${GREEN}PASS${NC}: $1"; ((PASS++)); }
fail() { echo -e "${RED}FAIL${NC}: $1 -- got: $2"; ((FAIL++)); }
section() { echo -e "\n${YELLOW}## $1${NC}"; }

# Test that command produces expected output pattern
# Usage: expect_output "description" "pattern" command args...
expect_output() {
  local desc="$1"
  local pattern="$2"
  shift 2
  local output
  output=$(timeout 3s pixi run start "$@" 2>&1 || true)
  if echo "$output" | grep -q "$pattern"; then
    pass "$desc"
  else
    fail "$desc" "$(echo "$output" | head -3 | tr '\n' ' ')"
  fi
}

# Test that command does NOT produce a pattern (success case)
# Usage: expect_no_output "description" "pattern" command args...
expect_no_output() {
  local desc="$1"
  local pattern="$2"
  shift 2
  local output
  output=$(timeout 3s pixi run start "$@" 2>&1 || true)
  if echo "$output" | grep -q "$pattern"; then
    fail "$desc" "$(echo "$output" | head -3 | tr '\n' ' ')"
  else
    pass "$desc"
  fi
}

# Test that command exits with error (non-zero)
# Usage: expect_error "description" "pattern" command args...
expect_error() {
  local desc="$1"
  local pattern="$2"
  shift 2
  local output
  local exit_code
  output=$(timeout 3s pixi run start "$@" 2>&1) && exit_code=0 || exit_code=$?
  if [[ $exit_code -ne 0 ]] && echo "$output" | grep -q "$pattern"; then
    pass "$desc"
  else
    fail "$desc" "exit=$exit_code, output=$(echo "$output" | head -2 | tr '\n' ' ')"
  fi
}

# Test argument parsing succeeds (reaches "Ensuring preset" without errors before it)
# Usage: expect_parse_ok "description" command args...
expect_parse_ok() {
  local desc="$1"
  shift
  local output
  output=$(timeout 3s pixi run start "$@" 2>&1 || true)
  # Check we get to "Ensuring preset" without hitting parse errors first
  if echo "$output" | grep -q "Ensuring preset"; then
    # Make sure no Error: before Ensuring preset
    local before_ensuring
    before_ensuring=$(echo "$output" | sed -n '1,/Ensuring preset/p')
    if echo "$before_ensuring" | grep -q "^Error:"; then
      fail "$desc" "Error before build: $(echo "$before_ensuring" | grep "^Error:" | head -1)"
    else
      pass "$desc"
    fi
  else
    fail "$desc" "$(echo "$output" | head -3 | tr '\n' ' ')"
  fi
}

echo "======================================"
echo "pixi run start - Comprehensive Tests"
echo "======================================"

# ============================================
# 1. HELP & USAGE (3 tests)
# ============================================
section "1. Help & Usage"

expect_output "1. --help shows usage" "Usage:" --help
expect_output "2. -h shows usage" "Usage:" -h
expect_error "3. no args shows missing app" "missing <app>"

# ============================================
# 2. PRESET OPTIONS (8 tests)
# ============================================
section "2. Preset Options"

expect_parse_ok "4. -p debug vkcube" -p debug vkcube
expect_parse_ok "5. --preset debug vkcube" --preset debug vkcube
expect_parse_ok "6. --preset=debug vkcube" --preset=debug vkcube
expect_parse_ok "7. -p release vkcube" -p release vkcube
expect_parse_ok "8. vkcube (default preset)" vkcube
expect_error "9. -p (no value)" "requires" -p
expect_error "10. --preset (no value)" "requires" --preset
expect_error "11. -p invalid-xyz" "Unknown preset" -p invalid-preset-xyz vkcube

# ============================================
# 3. APP SPECIFICATION (6 tests)
# ============================================
section "3. App Specification"

expect_parse_ok "12. vkcube (no --)" vkcube
expect_parse_ok "13. -- vkcube (with --)" -- vkcube
expect_parse_ok "14. vulkaninfo" vulkaninfo
expect_no_output "15. ./path no error" "Error:" ./build/debug/bin/app
expect_no_output "16. /abs/path no error" "Error:" /usr/bin/vkcube
expect_no_output "17. ../path no error" "Error:" ../foo

# ============================================
# 4. GOGGLES ARGS (6 tests)
# ============================================
section "4. Goggles Args"

expect_parse_ok "18. --input-forwarding vkcube" --input-forwarding vkcube
expect_parse_ok "19. --foo --bar vkcube" --foo --bar vkcube
expect_parse_ok "20. --unknown-flag vkcube" --unknown-flag vkcube
expect_parse_ok "21. --foo -- vkcube" --foo -- vkcube
expect_parse_ok "22. arg1 arg2 vkcube" arg1 arg2 vkcube
# After --, first arg is app name (even if looks like flag)
expect_output "23. -- --flag treats flag as app" "Ensuring preset" -- --flag-app

# ============================================
# 5. ENVIRONMENT VARIABLES (8 tests)
# ============================================
section "5. Environment Variables"

expect_parse_ok "24. --goggles-env FOO=bar vkcube" --goggles-env FOO=bar vkcube
expect_parse_ok "25. --goggles-env=FOO=bar vkcube" --goggles-env=FOO=bar vkcube
expect_parse_ok "26. --app-env FOO=bar vkcube" --app-env FOO=bar vkcube
expect_parse_ok "27. --app-env=FOO=bar vkcube" --app-env=FOO=bar vkcube
expect_parse_ok "28. both env vars" --goggles-env FOO=1 --app-env BAR=2 vkcube
expect_error "29. --goggles-env (no val)" "requires" --goggles-env
expect_error "30. --goggles-env invalid" "invalid format" --goggles-env invalid vkcube
expect_error "31. --goggles-env =val" "invalid format" --goggles-env =value vkcube

# ============================================
# 6. APP ARGUMENTS (4 tests)
# ============================================
section "6. App Arguments"

expect_parse_ok "32. vkcube --app-arg" vkcube --app-arg
expect_parse_ok "33. -- vkcube --width 800" -- vkcube --width 800
expect_parse_ok "34. -p debug vkcube arg1 arg2" -p debug vkcube arg1 arg2
expect_error "35. vkcube -- (trailing --)" "missing <app>" vkcube --

# ============================================
# 7. SEPARATOR EDGE CASES (5 tests)
# ============================================
section "7. Separator Edge Cases"

expect_parse_ok "36. -- vkcube" -- vkcube
expect_output "37. -- -- vkcube" "Ensuring preset" -- -- vkcube
expect_parse_ok "38. -p debug -- vkcube" -p debug -- vkcube
expect_parse_ok "39. --goggles-env X=1 -- vkcube" --goggles-env X=1 -- vkcube
expect_parse_ok "40. --foo -- vkcube --bar" --foo -- vkcube --bar

# ============================================
# 8. COMPLEX COMBINATIONS (5 tests)
# ============================================
section "8. Complex Combinations"

expect_parse_ok "41. full combo" -p release --goggles-env DISPLAY=:0 --input-forwarding vkcube
expect_parse_ok "42. both env same var" --app-env VAR=1 --goggles-env VAR=2 vkcube
expect_parse_ok "43. all options + --" -p debug --goggles-env X=1 --app-env Y=2 -- vkcube --arg
expect_parse_ok "44. multi flags + args" --input-forwarding --verbose vkcube --width 800
expect_parse_ok "45. release vulkaninfo" -p release vulkaninfo

# ============================================
# 9. ERROR CASES (7 tests)
# ============================================
section "9. Error Cases"

expect_error "46. no args" "missing <app>"
expect_error "47. -p no value" "requires" -p
expect_error "48. --preset= empty" "Unknown preset" --preset= vkcube
expect_error "49. --goggles-env no value" "requires" --goggles-env
expect_error "50. --app-env no =" "invalid format" --app-env noequals vkcube
expect_error "51. preset with space" "Unknown preset" -p "foo bar" vkcube
expect_error "52. only --" "missing <app>" --

# ============================================
# SUMMARY
# ============================================
echo -e "\n======================================"
TOTAL=$((PASS + FAIL))
echo -e "Results: ${GREEN}$PASS passed${NC}, ${RED}$FAIL failed${NC} (total: $TOTAL)"
echo "======================================"

[[ $FAIL -eq 0 ]]
