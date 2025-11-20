#!/bin/bash

# Test get license info operation

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BINARY="$PROJECT_ROOT/build/ur-licence-mann"
TEST_CONFIG="$SCRIPT_DIR/test_data/test_package_config.json"
TEST_LICENSE="$SCRIPT_DIR/output/test_license.lic"

echo "Testing get license info..."

# Ensure we have a license
if [ ! -f "$TEST_LICENSE" ]; then
    bash "$SCRIPT_DIR/test_generate.sh" > /dev/null 2>&1
fi

# Get license info using package config
OUTPUT=$($BINARY --package-config "$TEST_CONFIG" \
    -o get_license_info \
    -p license_file="$TEST_LICENSE" \
    -v 2>&1)
INFO_EXIT_CODE=$?

# Check for expected fields
REQUIRED_FIELDS=("license_id" "product" "customer_name" "customer_email")
ALL_FOUND=true
MISSING_FIELDS=()

for field in "${REQUIRED_FIELDS[@]}"; do
    if echo "$OUTPUT" | grep -q "\"$field\""; then
        echo "✓ Found field: $field"
    else
        echo "✗ Missing field: $field"
        ALL_FOUND=false
        MISSING_FIELDS+=("$field")
    fi
done

if [ "$ALL_FOUND" = true ]; then
    echo "✓ Get license info test passed"
    exit 0
else
    echo "✗ Some fields missing"
    echo "=========================================="
    echo "VERBOSE FAILURE LOG:"
    echo "=========================================="
    echo "Operation: Get license info"
    echo "Parameters:"
    echo "  - license_file: $TEST_LICENSE"
    echo "Exit Code: $INFO_EXIT_CODE"
    echo ""
    echo "Expected Result:"
    echo "  - All required fields present: ${REQUIRED_FIELDS[*]}"
    echo ""
    echo "Actual Result:"
    echo "  - Missing fields: ${MISSING_FIELDS[*]}"
    echo ""
    echo "Command Output:"
    echo "$OUTPUT"
    echo "=========================================="
    exit 1
fi