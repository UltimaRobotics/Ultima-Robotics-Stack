#!/bin/bash

# Test license update

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BINARY="$PROJECT_ROOT/build/ur-licence-mann"
TEST_CONFIG="$SCRIPT_DIR/test_data/test_package_config.json"
TEST_LICENSE="$SCRIPT_DIR/output/test_license.lic"
UPDATED_LICENSE="$SCRIPT_DIR/output/updated_license.lic"

echo "Testing license update..."

# Ensure we have a license
if [ ! -f "$TEST_LICENSE" ]; then
    bash "$SCRIPT_DIR/test_generate.sh" > /dev/null 2>&1
fi

# Update license using package config
OUTPUT=$($BINARY --package-config "$TEST_CONFIG" \
    -o update \
    -p input_file="$TEST_LICENSE" \
    -p output_file="$UPDATED_LICENSE" \
    -p new_expiry="2027-12-31T23:59:59Z" \
    -v 2>&1)
UPDATE_EXIT_CODE=$?

# Check if updated license exists
if [ -f "$UPDATED_LICENSE" ]; then
    echo "✓ Updated license created"

    # Verify updated license is valid
    OUTPUT=$($BINARY --package-config "$TEST_CONFIG" -o verify \
        license_file="$UPDATED_LICENSE" \
        check_expiry="false" \
        -v 2>&1)
    VERIFY_EXIT_CODE=$?

    if echo "$OUTPUT" | grep -q "License is VALID"; then
        echo "✓ Updated license is valid"
    else
        echo "✗ Updated license verification failed"
        echo "=========================================="
        echo "VERBOSE FAILURE LOG:"
        echo "=========================================="
        echo "Operation: Verify updated license"
        echo "Parameters:"
        echo "  - input_file: $TEST_LICENSE"
        echo "  - output_file: $UPDATED_LICENSE"
        echo "  - new_expiry: 2027-12-31T23:59:59Z"
        echo "Verify Exit Code: $VERIFY_EXIT_CODE"
        echo ""
        echo "Expected Result:"
        echo "  - Updated license should be VALID"
        echo ""
        echo "Actual Result:"
        echo "  - Updated license verification: FAILED"
        echo ""
        echo "Update Command Output:"
        echo "$OUTPUT"
        echo ""
        echo "Verification Output:"
        echo "$OUTPUT"
        echo "=========================================="
        exit 1
    fi
else
    echo "✗ Updated license not created"
    echo "=========================================="
    echo "VERBOSE FAILURE LOG:"
    echo "=========================================="
    echo "Operation: Update license"
    echo "Parameters:"
    echo "  - input_file: $TEST_LICENSE"
    echo "  - output_file: $UPDATED_LICENSE"
    echo "  - new_expiry: 2027-12-31T23:59:59Z"
    echo "Exit Code: $UPDATE_EXIT_CODE"
    echo ""
    echo "Expected Result:"
    echo "  - Updated license at: $UPDATED_LICENSE"
    echo ""
    echo "Actual Result:"
    echo "  - File exists: NO"
    echo ""
    echo "Command Output:"
    echo "$OUTPUT"
    echo "=========================================="
    exit 1
fi

echo "✓ License update test passed"
exit 0