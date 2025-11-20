
#!/bin/bash

# Test for automatic license definitions encryption
# This test verifies that definitions are automatically encrypted when auto_encrypt_definitions is enabled

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BINARY="$PROJECT_ROOT/build/ur-licence-mann"
TEST_OUTPUT="$SCRIPT_DIR/output"
DEFINITIONS_FILE="$TEST_OUTPUT/test_definitions.json"
ENCRYPTED_DEFS_FILE="$TEST_OUTPUT/test_definitions.enc"

# Create test output directory
mkdir -p "$TEST_OUTPUT"

echo "Testing Automatic Definitions Encryption Feature..."
echo "===================================================="

# Create test package config with auto_encrypt_definitions enabled
cat > "$TEST_OUTPUT/auto_encrypt_pkg_config.json" << EOF
{
  "keys_directory": "$PROJECT_ROOT/keys",
  "config_directory": "$PROJECT_ROOT/config",
  "licenses_directory": "$PROJECT_ROOT/licenses",
  "license_definitions_file": "$DEFINITIONS_FILE",
  "encrypted_license_definitions_file": "$ENCRYPTED_DEFS_FILE",
  "private_key_file": "$PROJECT_ROOT/keys/private_key.pem",
  "public_key_file": "$PROJECT_ROOT/keys/public_key.pem",
  "auto_encrypt_definitions": true,
  "auto_encrypt_licenses": true,
  "require_hardware_binding": true,
  "require_signature": true
}
EOF

# Create test definitions JSON
cat > "$DEFINITIONS_FILE" << EOF
{
  "licenses": [
    {
      "licence_type": "TestBasic",
      "features": [
        {
          "feature_name": "test_feature_1",
          "feature_status": "UNLIMITED_ACCESS"
        },
        {
          "feature_name": "test_feature_2",
          "feature_status": "LIMITED_ACCESS"
        }
      ]
    },
    {
      "licence_type": "TestPremium",
      "features": [
        {
          "feature_name": "test_feature_1",
          "feature_status": "UNLIMITED_ACCESS"
        },
        {
          "feature_name": "test_feature_2",
          "feature_status": "UNLIMITED_ACCESS"
        },
        {
          "feature_name": "test_feature_3",
          "feature_status": "UNLIMITED_ACCESS"
        }
      ]
    }
  ]
}
EOF

# Step 1: Update definitions (should automatically encrypt)
echo "Step 1: Updating definitions (should auto-encrypt)..."
cat > "$TEST_OUTPUT/update_defs_config.json" << EOF
{
  "operation": "update_license_definitions",
  "parameters": {
    "definitions_file": "$DEFINITIONS_FILE"
  }
}
EOF

if ! "$BINARY" --config "$TEST_OUTPUT/update_defs_config.json" --package-config "$TEST_OUTPUT/auto_encrypt_pkg_config.json" --verbose; then
    echo "ERROR: Failed to update and encrypt definitions"
    exit 1
fi

# Check if encrypted definitions file was created
if [ ! -f "$ENCRYPTED_DEFS_FILE" ]; then
    echo "ERROR: Encrypted definitions file not created: $ENCRYPTED_DEFS_FILE"
    exit 1
fi

echo "✓ Definitions encrypted successfully"

# Step 2: Read encrypted definitions
echo ""
echo "Step 2: Reading encrypted definitions..."
cat > "$TEST_OUTPUT/get_defs_config.json" << EOF
{
  "operation": "get_license_definitions",
  "parameters": {}
}
EOF

if ! "$BINARY" --config "$TEST_OUTPUT/get_defs_config.json" --package-config "$TEST_OUTPUT/auto_encrypt_pkg_config.json" --verbose; then
    echo "ERROR: Failed to read encrypted definitions"
    exit 1
fi

echo "✓ Encrypted definitions read successfully"

# Step 3: Update definitions via JSON parameter
echo ""
echo "Step 3: Updating definitions via JSON parameter..."
cat > "$TEST_OUTPUT/update_defs_json_config.json" << EOF
{
  "operation": "update_license_definitions",
  "parameters": {
    "definitions_json": "{\"licenses\":[{\"licence_type\":\"UpdatedType\",\"features\":[{\"feature_name\":\"new_feature\",\"feature_status\":\"UNLIMITED_ACCESS\"}]}]}"
  }
}
EOF

if ! "$BINARY" --config "$TEST_OUTPUT/update_defs_json_config.json" --package-config "$TEST_OUTPUT/auto_encrypt_pkg_config.json" --verbose; then
    echo "ERROR: Failed to update definitions via JSON"
    exit 1
fi

echo "✓ Definitions updated via JSON successfully"

# Verify the updated content
echo ""
echo "Step 4: Verifying updated definitions..."
OUTPUT=$("$BINARY" --config "$TEST_OUTPUT/get_defs_config.json" --package-config "$TEST_OUTPUT/auto_encrypt_pkg_config.json" 2>&1)

if echo "$OUTPUT" | grep -q "UpdatedType"; then
    echo "✓ Definitions update verified - found UpdatedType"
else
    echo "ERROR: Updated definitions not found"
    exit 1
fi

echo ""
echo "All automatic definitions encryption tests passed!"
exit 0
