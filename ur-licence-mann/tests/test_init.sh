#!/bin/bash

# Test initialization and key generation

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BINARY="$PROJECT_ROOT/build/ur-licence-mann"
TEST_CONFIG="$SCRIPT_DIR/test_data/test_package_config.json"
TEST_KEYS_DIR="$SCRIPT_DIR/output/test_keys"

# Clean up previous test
rm -rf "$TEST_KEYS_DIR"
mkdir -p "$TEST_KEYS_DIR"

echo "Testing initialization with default config..."

# Run initialization
INIT_OUTPUT=$($BINARY --package-config "$TEST_CONFIG" -v 2>&1)
INIT_EXIT_CODE=$?

# Check if keys were generated
if [ -f "$TEST_KEYS_DIR/private_key.pem" ] && [ -f "$TEST_KEYS_DIR/public_key.pem" ]; then
    echo "✓ RSA keys generated successfully"
else
    echo "✗ RSA keys not found"
    echo "=========================================="
    echo "VERBOSE FAILURE LOG:"
    echo "=========================================="
    echo "Operation: Initialize and generate RSA keys"
    echo "Command: $BINARY --package-config $TEST_CONFIG -v"
    echo "Exit Code: $INIT_EXIT_CODE"
    echo ""
    echo "Expected Result:"
    echo "  - RSA private key at: $TEST_KEYS_DIR/private_key.pem"
    echo "  - RSA public key at: $TEST_KEYS_DIR/public_key.pem"
    echo ""
    echo "Actual Result:"
    echo "  - Private key exists: $([ -f "$TEST_KEYS_DIR/private_key.pem" ] && echo "YES" || echo "NO")"
    echo "  - Public key exists: $([ -f "$TEST_KEYS_DIR/public_key.pem" ] && echo "YES" || echo "NO")"
    echo ""
    echo "Command Output:"
    echo "$INIT_OUTPUT"
    echo "=========================================="
    exit 1
fi

# Check if encryption keys were generated
if [ -f "$TEST_KEYS_DIR/encryption_keys.json" ]; then
    echo "✓ Encryption keys generated successfully"
else
    echo "✗ Encryption keys not found"
    echo "=========================================="
    echo "VERBOSE FAILURE LOG:"
    echo "=========================================="
    echo "Operation: Generate encryption keys"
    echo "Command: $BINARY --package-config $TEST_CONFIG -v"
    echo "Exit Code: $INIT_EXIT_CODE"
    echo ""
    echo "Expected Result:"
    echo "  - Encryption keys at: $TEST_KEYS_DIR/encryption_keys.json"
    echo ""
    echo "Actual Result:"
    echo "  - Encryption keys exist: NO"
    echo ""
    echo "Command Output:"
    echo "$INIT_OUTPUT"
    echo "=========================================="
    exit 1
fi

echo "✓ Initialization test passed"
exit 0