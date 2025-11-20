#!/bin/bash

# Test license verification

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BINARY="$PROJECT_ROOT/build/ur-licence-mann"
TEST_CONFIG="$SCRIPT_DIR/test_data/test_package_config.json"
TEST_LICENSE="$SCRIPT_DIR/output/test_license.lic"

echo "Testing license verification..."

# First ensure we have a license to verify
if [ ! -f "$TEST_LICENSE" ]; then
    echo "Generating test license first..."
    bash "$SCRIPT_DIR/test_generate.sh" > /dev/null 2>&1
fi

# Verify license using package config
OUTPUT=$($BINARY --package-config "$TEST_CONFIG" \
    -o verify \
    -p license_file="$TEST_LICENSE" \
    -v 2>&1)
VERIFY_EXIT_CODE=$?

if echo "$OUTPUT" | grep -q "License is VALID"; then
    echo "✓ License verification successful"
else
    echo "✗ License verification failed"
    echo "=========================================="
    echo "VERBOSE FAILURE LOG:"
    echo "=========================================="
    echo "Operation: Verify valid license"
    echo "Parameters:"
    echo "  - license_file: $TEST_LICENSE"
    echo "  - check_expiry: false"
    echo "Exit Code: $VERIFY_EXIT_CODE"
    echo ""
    echo "Expected Result:"
    echo "  - Output should contain: 'License is VALID'"
    echo ""
    echo "Actual Result:"
    echo "  - License verification: FAILED"
    echo ""
    echo "Command Output:"
    echo "$OUTPUT"
    echo "=========================================="
    exit 1
fi

# Test verification with invalid license
echo "Testing with invalid license..."
INVALID_LICENSE="$SCRIPT_DIR/output/invalid_license.lic"
echo "invalid data" > "$INVALID_LICENSE"

OUTPUT=$($BINARY --package-config "$TEST_CONFIG" -o verify \
    license_file="$INVALID_LICENSE" \
    -v 2>&1)
INVALID_EXIT_CODE=$?

if echo "$OUTPUT" | grep -q "License is INVALID"; then
    echo "✓ Invalid license correctly rejected"
else
    echo "✗ Invalid license should have been rejected"
    echo "=========================================="
    echo "VERBOSE FAILURE LOG:"
    echo "=========================================="
    echo "Operation: Verify invalid license (should reject)"
    echo "Parameters:"
    echo "  - license_file: $INVALID_LICENSE"
    echo "Exit Code: $INVALID_EXIT_CODE"
    echo ""
    echo "Expected Result:"
    echo "  - Output should contain: 'License is INVALID'"
    echo ""
    echo "Actual Result:"
    echo "  - Invalid license was NOT rejected properly"
    echo ""
    echo "Command Output:"
    echo "$OUTPUT"
    echo "=========================================="
    exit 1
fi

echo "✓ License verification test passed"
exit 0