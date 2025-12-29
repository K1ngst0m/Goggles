#!/bin/bash
set -euo pipefail

SHADER_DIR="shaders/retroarch"
BUILD_DIR="build/debug"
OUTPUT="docs/shader_compatibility.md"
TEST_BIN="$BUILD_DIR/tests/goggles_tests"

if [[ ! -x "$TEST_BIN" ]]; then
    echo "Error: Test binary not found: $TEST_BIN" >&2
    echo "Run: cmake --build --preset debug" >&2
    exit 1
fi

mkdir -p docs

echo "Running full shader validation..."
RESULT=$("$TEST_BIN" "[shader][validation][batch]" -s 2>&1) || true

if [[ -z "$RESULT" ]]; then
    echo "Error: Test produced no output" >&2
    exit 1
fi

# Discover all shader categories from filesystem (same as test does)
CATEGORIES=()
for dir in "$SHADER_DIR"/*/; do
    name=$(basename "$dir")
    # Skip non-shader directories
    [[ "$name" == "include" || "$name" == "spec" || "$name" == "test" ]] && continue
    CATEGORIES+=("$name")
done
IFS=$'\n' CATEGORIES=($(sort <<<"${CATEGORIES[*]}")); unset IFS

cat > "$OUTPUT" << 'EOF'
# Shader Compatibility Report

RetroArch shader preset compatibility with Goggles filter chain.

> **Note:** This report only covers **compilation status** (parse + preprocess).
> Passing compilation does not guarantee visual correctness.

EOF

echo "**Last Updated:** $(date -u '+%Y-%m-%d %H:%M UTC')" >> "$OUTPUT"
echo "" >> "$OUTPUT"

# Parse all results first
declare -A CAT_PASS CAT_TOTAL
TOTAL_PASS=0
TOTAL_COUNT=0

for cat in "${CATEGORIES[@]}"; do
    LINE=$(echo "$RESULT" | grep -iP "^\s*${cat}:" | head -1)
    if [[ -n "$LINE" ]]; then
        PASS=$(echo "$LINE" | grep -oP '\d+(?=/)')
        TOTAL=$(echo "$LINE" | grep -oP '(?<=/)\d+')
        CAT_PASS[$cat]=$PASS
        CAT_TOTAL[$cat]=$TOTAL
        TOTAL_PASS=$((TOTAL_PASS + PASS))
        TOTAL_COUNT=$((TOTAL_COUNT + TOTAL))
    fi
done

# Overall summary
echo "## Overview" >> "$OUTPUT"
echo "" >> "$OUTPUT"
if [[ $TOTAL_COUNT -gt 0 ]]; then
    PERCENT=$((TOTAL_PASS * 100 / TOTAL_COUNT))
else
    PERCENT=0
fi
echo "**Total:** ${TOTAL_PASS}/${TOTAL_COUNT} presets compile (${PERCENT}%)" >> "$OUTPUT"
echo "" >> "$OUTPUT"

echo "## By Category" >> "$OUTPUT"
echo "" >> "$OUTPUT"
echo "| Category | Pass Rate | Status |" >> "$OUTPUT"
echo "|----------|-----------|--------|" >> "$OUTPUT"

for cat in "${CATEGORIES[@]}"; do
    PASS=${CAT_PASS[$cat]:-0}
    TOTAL=${CAT_TOTAL[$cat]:-0}
    [[ $TOTAL -eq 0 ]] && continue
    if [[ "$PASS" == "$TOTAL" ]]; then
        echo "| $cat | $PASS/$TOTAL | ✅ |" >> "$OUTPUT"
    else
        echo "| $cat | $PASS/$TOTAL | ⚠️ |" >> "$OUTPUT"
    fi
done

echo "" >> "$OUTPUT"

# Extract failed presets
FAILED=$(echo "$RESULT" | grep -oP "[\w-]+\.slangp(?=:)" | sort -u)

# Detailed sections
echo "## Details" >> "$OUTPUT"
echo "" >> "$OUTPUT"

for cat in "${CATEGORIES[@]}"; do
    DIR="$SHADER_DIR/$cat"
    [[ ! -d "$DIR" ]] && continue

    PASS=${CAT_PASS[$cat]:-}
    TOTAL=${CAT_TOTAL[$cat]:-}
    # Skip categories with no test results
    [[ -z "$TOTAL" || "$TOTAL" == "0" ]] && continue

    PRESETS=$(find "$DIR" -type f -name "*.slangp" 2>/dev/null | sort)

    echo "<details>" >> "$OUTPUT"
    echo "<summary><strong>$cat</strong> ($PASS/$TOTAL)</summary>" >> "$OUTPUT"
    echo "" >> "$OUTPUT"
    echo "| Preset | Status |" >> "$OUTPUT"
    echo "|--------|--------|" >> "$OUTPUT"

    while IFS= read -r preset; do
        [[ -z "$preset" ]] && continue
        name=$(basename "$preset")
        # Show relative path from category dir for nested presets
        rel_path="${preset#$DIR/}"
        if echo "$FAILED" | grep -qx "$name"; then
            echo "| \`$rel_path\` | ❌ |" >> "$OUTPUT"
        else
            echo "| \`$rel_path\` | ✅ |" >> "$OUTPUT"
        fi
    done <<< "$PRESETS"

    echo "" >> "$OUTPUT"
    echo "</details>" >> "$OUTPUT"
    echo "" >> "$OUTPUT"
done

echo "---" >> "$OUTPUT"
echo "**Run:** \`./build/debug/tests/goggles_tests \"[shader][validation][batch]\" -s\`" >> "$OUTPUT"

echo "Report saved to $OUTPUT"
grep -E "^\| " "$OUTPUT" | head -20