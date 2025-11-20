#!/bin/bash

# Test license generation

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BINARY="$PROJECT_ROOT/build/ur-licence-mann"
TEST_CONFIG="$SCRIPT_DIR/test_data/test_package_config.json"
TEST_OUTPUT="$SCRIPT_DIR/output/test_license.lic"
TEST_OUTPUT_DIR="$SCRIPT_DIR/output"

echo "Testing license generation..."

# Generate test license using package config and operation parameters
OUTPUT=$($BINARY --package-config "$TEST_CONFIG" \
    -o generate \
    -p customer_name="Test User" \
    -p customer_email="test@example.com" \
    -p license_tier="Professional" \
    -p output="$TEST_OUTPUT" \
    -v 2>&1)
GEN_EXIT_CODE=$?

# Check if license file was created
if [ -f "$TEST_OUTPUT" ]; then
    echo "✓ License file created"

    # Check file is not empty
    if [ -s "$TEST_OUTPUT" ]; then
        echo "✓ License file has content"
    else
        echo "✗ License file is empty"
        echo "=========================================="
        echo "VERBOSE FAILURE LOG:"
        echo "=========================================="
        echo "Operation: Generate license with content"
        echo "Parameters:"
        echo "  - license_id: test-12345"
        echo "  - product: TestProduct"
        echo "  - version: 1.0.0"
        echo "  - customer_name: Test User"
        echo "  - license_tier: Professional"
        echo "  - licence_type: UltimaProfessionalLicence"
        echo "Exit Code: $GEN_EXIT_CODE"
        echo ""
        echo "Expected Result:"
        echo "  - Non-empty license file at: $TEST_OUTPUT"
        echo ""
        echo "Actual Result:"
        echo "  - File exists: YES"
        echo "  - File size: 0 bytes (EMPTY)"
        echo ""
        echo "Command Output:"
        echo "$OUTPUT"
        echo "=========================================="
        exit 1
    fi
else
    echo "✗ License file not created"
    echo "=========================================="
    echo "VERBOSE FAILURE LOG:"
    echo "=========================================="
    echo "Operation: Generate license file"
    echo "Parameters:"
    echo "  - license_id: test-12345"
    echo "  - product: TestProduct"
    echo "  - version: 1.0.0"
    echo "  - customer_name: Test User"
    echo "  - customer_email: test@example.com"
    echo "  - license_tier: Professional"
    echo "  - licence_type: UltimaProfessionalLicence"
    echo "  - valid_until: 2026-12-31"
    echo "  - output: $TEST_OUTPUT"
    echo "Exit Code: $GEN_EXIT_CODE"
    echo ""
    echo "Expected Result:"
    echo "  - License file at: $TEST_OUTPUT"
    echo ""
    echo "Actual Result:"
    echo "  - File exists: NO"
    echo ""
    echo "Command Output:"
    echo "$OUTPUT"
    echo "=========================================="
    exit 1
fi

echo "✓ License generation test passed"
exit 0