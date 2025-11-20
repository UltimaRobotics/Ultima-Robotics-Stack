#!/bin/bash

# Test hardware binding feature

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BINARY="$PROJECT_ROOT/build/ur-licence-mann"
TEST_CONFIG="$SCRIPT_DIR/test_data/test_package_config.json"
TEST_OUTPUT="$SCRIPT_DIR/output/hw_bound_license.lic"

echo "Testing hardware binding..."

# Generate license with hardware binding
GEN_OUTPUT=$($BINARY --package-config "$TEST_CONFIG" -o generate \
    license_id="hw-test-12345" \
    product="TestProduct" \
    version="1.0.0" \
    customer_name="HW Bound User" \
    customer_email="hwbound@example.com" \
    license_tier="Professional" \
    licence_type="UltimaProfessionalLicence" \
    valid_until="2026-12-31" \
    output="$TEST_OUTPUT" \
    -v 2>&1)
GEN_EXIT_CODE=$?

# Check license info for hardware ID
INFO_OUTPUT=$($BINARY --package-config "$TEST_CONFIG" -o get_license_info \
    license_file="$TEST_OUTPUT" \
    -v 2>&1)
INFO_EXIT_CODE=$?

if echo "$INFO_OUTPUT" | grep -q "device_hardware_id"; then
    echo "✓ Hardware binding information present in license"
else
    echo "✗ Hardware binding information missing"
    echo "=========================================="
    echo "VERBOSE FAILURE LOG:"
    echo "=========================================="
    echo "Operation: Check hardware binding in license"
    echo "Parameters:"
    echo "  - license_id: hw-test-12345"
    echo "  - product: TestProduct"
    echo "  - customer_name: HW Bound User"
    echo "Generate Exit Code: $GEN_EXIT_CODE"
    echo "Get Info Exit Code: $INFO_EXIT_CODE"
    echo ""
    echo "Expected Result:"
    echo "  - License should contain 'device_hardware_id' field"
    echo ""
    echo "Actual Result:"
    echo "  - device_hardware_id field: NOT FOUND"
    echo ""
    echo "Generation Output:"
    echo "$GEN_OUTPUT"
    echo ""
    echo "Get Info Output:"
    echo "$INFO_OUTPUT"
    echo "=========================================="
    exit 1
fi

echo "✓ Hardware binding test passed"
exit 0