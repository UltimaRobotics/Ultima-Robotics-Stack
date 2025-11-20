#!/bin/bash

# Test get license plan operation

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BINARY="$PROJECT_ROOT/build/ur-licence-mann"
TEST_CONFIG="$SCRIPT_DIR/test_data/test_package_config.json"
TEST_LICENSE="$SCRIPT_DIR/output/test_license.lic"

echo "Testing get license plan..."

# Ensure we have a license
if [ ! -f "$TEST_LICENSE" ]; then
    bash "$SCRIPT_DIR/test_generate.sh" > /dev/null 2>&1
fi

# Get license plan using package config
OUTPUT=$($BINARY --package-config "$TEST_CONFIG" \
    -o get_license_plan \
    -p license_file="$TEST_LICENSE" \
    -v 2>&1)
PLAN_EXIT_CODE=$?

# Check for expected fields
MISSING_FIELDS=()
if ! echo "$OUTPUT" | grep -q "\"license_type\""; then
    MISSING_FIELDS+=("license_type")
fi
if ! echo "$OUTPUT" | grep -q "\"license_tier\""; then
    MISSING_FIELDS+=("license_tier")
fi
if ! echo "$OUTPUT" | grep -q "\"product\""; then
    MISSING_FIELDS+=("product")
fi

if [ ${#MISSING_FIELDS[@]} -eq 0 ]; then
    echo "✓ License plan contains expected fields"
    echo "✓ Get license plan test passed"
    exit 0
else
    echo "✗ License plan missing expected fields"
    echo "=========================================="
    echo "VERBOSE FAILURE LOG:"
    echo "=========================================="
    echo "Operation: Get license plan"
    echo "Parameters:"
    echo "  - license_file: $TEST_LICENSE"
    echo "Exit Code: $PLAN_EXIT_CODE"
    echo ""
    echo "Expected Result:"
    echo "  - Output should contain: license_type, license_tier, product"
    echo ""
    echo "Actual Result:"
    echo "  - Missing fields: ${MISSING_FIELDS[*]}"
    echo ""
    echo "Command Output:"
    echo "$OUTPUT"
    echo "=========================================="
    exit 1
fi