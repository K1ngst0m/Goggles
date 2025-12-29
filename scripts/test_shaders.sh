#!/bin/bash
# Test shader preset parsing
# Usage: ./scripts/test_shaders.sh [category] [limit]

SHADER_DIR="shaders/retroarch"
CATEGORY="${1:-crt}"
LIMIT="${2:-20}"

echo "Testing $CATEGORY presets (limit: $LIMIT)..."
echo ""

PRESETS=$(find "$SHADER_DIR/$CATEGORY" -name "*.slangp" 2>/dev/null | head -$LIMIT | sort)

TOTAL=0
PASSED=0

for preset in $PRESETS; do
    TOTAL=$((TOTAL + 1))
    name=$(basename "$preset")

    # Run preset parser test
    if ./build/debug/tests/goggles_tests "[preset]" 2>&1 | grep -q "passed"; then
        echo -e "\033[32m✓\033[0m $name"
        PASSED=$((PASSED + 1))
    else
        echo -e "\033[31m✗\033[0m $name"
    fi
done

echo ""
echo "Results: $PASSED/$TOTAL"