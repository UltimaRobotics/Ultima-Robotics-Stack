
#!/bin/bash

# Test for forced encryption flags
# Verifies that FORCE_LICENSE_ENCRYPTION and FORCE_DEFINITIONS_ENCRYPTION work correctly

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BINARY="$PROJECT_ROOT/build/ur-licence-mann"
TEST_OUTPUT="$SCRIPT_DIR/output"
DEVICE_CONFIG="$PROJECT_ROOT/build/device_config.h"

# Create test output directory
mkdir -p "$TEST_OUTPUT"

echo "Testing Force Encryption Flags..."
echo "=================================="

# Step 1: Verify device_config.h has forced encryption enabled
echo "Step 1: Checking device_config.h for force encryption flags..."

if [ ! -f "$DEVICE_CONFIG" ]; then
    echo "ERROR: device_config.h not found at: $DEVICE_CONFIG"
    exit 1
fi

if grep -q "FORCE_LICENSE_ENCRYPTION true" "$DEVICE_CONFIG"; then
    echo "✓ FORCE_LICENSE_ENCRYPTION is enabled"
else
    echo "ERROR: FORCE_LICENSE_ENCRYPTION not enabled in device_config.h"
    exit 1
fi

if grep -q "FORCE_DEFINITIONS_ENCRYPTION true" "$DEVICE_CONFIG"; then
    echo "✓ FORCE_DEFINITIONS_ENCRYPTION is enabled"
else
    echo "ERROR: FORCE_DEFINITIONS_ENCRYPTION not enabled in device_config.h"
    exit 1
fi

if grep -q "BUILTIN_ENCRYPTION_KEY" "$DEVICE_CONFIG"; then
    echo "✓ BUILTIN_ENCRYPTION_KEY is defined"
else
    echo "ERROR: BUILTIN_ENCRYPTION_KEY not defined in device_config.h"
    exit 1
fi

# Step 2: Verify license generation always encrypts
echo ""
echo "Step 2: Verifying license generation always encrypts..."
LICENSE_FILE="$TEST_OUTPUT/force_encrypt_test.lic"
ENCRYPTED_FILE="$TEST_OUTPUT/force_encrypt_test.lic.enc"

cat > "$TEST_OUTPUT/force_gen_config.json" << EOF
{
  "operation": "generate",
  "parameters": {
    "output": "$LICENSE_FILE",
    "license_id": "FORCE-ENC-001",
    "customer_name": "Force Encrypt Test",
    "customer_email": "force@test.com"
  }
}
EOF

if ! "$BINARY" --config "$TEST_OUTPUT/force_gen_config.json" --package-config "$PROJECT_ROOT/config/package_config.json" --verbose; then
    echo "ERROR: License generation failed"
    exit 1
fi

if [ -f "$ENCRYPTED_FILE" ]; then
    echo "✓ License was automatically encrypted (encrypted file exists)"
else
    echo "WARNING: Encrypted file not found, checking if auto_encrypt_licenses is enabled in package_config"
fi

# Step 3: Verify that all operations use BUILTIN_ENCRYPTION_KEY
echo ""
echo "Step 3: Verifying all operations use builtin encryption key..."

# Generate license
"$BINARY" --config "$TEST_OUTPUT/force_gen_config.json" --package-config "$PROJECT_ROOT/config/package_config.json" --verbose > /dev/null 2>&1

# Verify license (should work with builtin key)
cat > "$TEST_OUTPUT/force_verify_config.json" << EOF
{
  "operation": "verify",
  "parameters": {
    "license_file": "$ENCRYPTED_FILE",
    "check_expiry": "false"
  }
}
EOF

if "$BINARY" --config "$TEST_OUTPUT/force_verify_config.json" --package-config "$PROJECT_ROOT/config/package_config.json" --verbose > /dev/null 2>&1; then
    echo "✓ Verification works with builtin encryption key"
else
    echo "ERROR: Verification failed with builtin key"
    exit 1
fi

# Get license info (should work with builtin key)
cat > "$TEST_OUTPUT/force_info_config.json" << EOF
{
  "operation": "get_license_info",
  "parameters": {
    "license_file": "$ENCRYPTED_FILE"
  }
}
EOF

if "$BINARY" --config "$TEST_OUTPUT/force_info_config.json" --package-config "$PROJECT_ROOT/config/package_config.json" --verbose > /dev/null 2>&1; then
    echo "✓ Get license info works with builtin encryption key"
else
    echo "ERROR: Get license info failed with builtin key"
    exit 1
fi

echo ""
echo "All force encryption tests passed!"
exit 0
