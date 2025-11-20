
#!/bin/bash

# Test for build-time encryption key feature
# This test verifies that licenses are encrypted using the BUILTIN_ENCRYPTION_KEY

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BINARY="$PROJECT_ROOT/build/ur-licence-mann"
TEST_OUTPUT="$SCRIPT_DIR/output"
LICENSE_FILE="$TEST_OUTPUT/builtin_encrypted_license.lic"
ENCRYPTED_LICENSE="$TEST_OUTPUT/builtin_encrypted_license.lic.enc"

# Create test output directory
mkdir -p "$TEST_OUTPUT"

echo "Testing Build-time Encryption Key Feature..."
echo "=============================================="

# Generate a license (should automatically encrypt with BUILTIN_ENCRYPTION_KEY)
echo "Step 1: Generating license with automatic encryption..."
cat > "$TEST_OUTPUT/builtin_encrypt_config.json" << EOF
{
  "operation": "generate",
  "parameters": {
    "output": "$LICENSE_FILE",
    "license_id": "BUILTIN-TEST-001",
    "customer_name": "Builtin Test User",
    "customer_email": "builtin@test.com"
  }
}
EOF

if ! "$BINARY" --config "$TEST_OUTPUT/builtin_encrypt_config.json" --package-config "$PROJECT_ROOT/config/package_config.json" --verbose; then
    echo "ERROR: Failed to generate license with builtin encryption"
    exit 1
fi

# Check if encrypted file was created
if [ ! -f "$ENCRYPTED_LICENSE" ]; then
    echo "ERROR: Encrypted license file not created: $ENCRYPTED_LICENSE"
    exit 1
fi

echo "✓ License encrypted successfully with BUILTIN_ENCRYPTION_KEY"

# Verify the encrypted license
echo ""
echo "Step 2: Verifying encrypted license..."
cat > "$TEST_OUTPUT/builtin_verify_config.json" << EOF
{
  "operation": "verify",
  "parameters": {
    "license_file": "$ENCRYPTED_LICENSE",
    "check_expiry": "false"
  }
}
EOF

if ! "$BINARY" --config "$TEST_OUTPUT/builtin_verify_config.json" --package-config "$PROJECT_ROOT/config/package_config.json" --verbose; then
    echo "ERROR: Failed to verify encrypted license"
    exit 1
fi

echo "✓ Encrypted license verified successfully"

# Extract license info from encrypted file
echo ""
echo "Step 3: Extracting info from encrypted license..."
cat > "$TEST_OUTPUT/builtin_info_config.json" << EOF
{
  "operation": "get_license_info",
  "parameters": {
    "license_file": "$ENCRYPTED_LICENSE"
  }
}
EOF

if ! "$BINARY" --config "$TEST_OUTPUT/builtin_info_config.json" --package-config "$PROJECT_ROOT/config/package_config.json" --verbose; then
    echo "ERROR: Failed to extract license info"
    exit 1
fi

echo "✓ License info extracted successfully"

echo ""
echo "All build-time encryption tests passed!"
exit 0
