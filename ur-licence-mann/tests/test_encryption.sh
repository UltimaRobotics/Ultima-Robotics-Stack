
#!/bin/bash

# Test license encryption/decryption

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BINARY="$PROJECT_ROOT/build/ur-licence-mann"
TEST_CONFIG="$SCRIPT_DIR/test_data/test_package_config_encrypted.json"
TEST_OUTPUT="$SCRIPT_DIR/output/encrypted_license.lic"

echo "Testing license encryption..."

# Generate encrypted license
GEN_OUTPUT=$($BINARY --package-config "$TEST_CONFIG" -o generate \
    license_id="enc-test-12345" \
    product="TestProduct" \
    version="1.0.0" \
    customer_name="Encrypted User" \
    customer_email="encrypted@example.com" \
    license_tier="Enterprise" \
    licence_type="UltimaEnterpriseLicence" \
    valid_until="2026-12-31" \
    output="$TEST_OUTPUT" \
    -v 2>&1)
GEN_EXIT_CODE=$?

# Verify encrypted license can be read
OUTPUT=$($BINARY --package-config "$TEST_CONFIG" -o verify \
    license_file="$TEST_OUTPUT" \
    check_expiry="false" \
    -v 2>&1)
VERIFY_EXIT_CODE=$?

if echo "$OUTPUT" | grep -q "License is VALID"; then
    echo "✓ Encrypted license verified successfully"
else
    echo "✗ Encrypted license verification failed"
    echo "=========================================="
    echo "VERBOSE FAILURE LOG:"
    echo "=========================================="
    echo "Operation: Verify encrypted license"
    echo "Parameters:"
    echo "  - license_id: enc-test-12345"
    echo "  - product: TestProduct"
    echo "  - customer_name: Encrypted User"
    echo "  - license_tier: Enterprise"
    echo "  - licence_type: UltimaEnterpriseLicence"
    echo "  - output: $TEST_OUTPUT"
    echo "Generate Exit Code: $GEN_EXIT_CODE"
    echo "Verify Exit Code: $VERIFY_EXIT_CODE"
    echo ""
    echo "Expected Result:"
    echo "  - Encrypted license should be VALID"
    echo ""
    echo "Actual Result:"
    echo "  - Encrypted license verification: FAILED"
    echo ""
    echo "Generation Output:"
    echo "$GEN_OUTPUT"
    echo ""
    echo "Verification Output:"
    echo "$OUTPUT"
    echo "=========================================="
    exit 1
fi

echo "✓ Encryption test passed"
exit 0
