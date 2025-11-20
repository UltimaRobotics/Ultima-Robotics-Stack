
#!/bin/bash

# Test for license update with automatic encryption
# Verifies that updated licenses are automatically encrypted

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BINARY="$PROJECT_ROOT/build/ur-licence-mann"
TEST_OUTPUT="$SCRIPT_DIR/output"
LICENSE_FILE="$TEST_OUTPUT/update_encrypt_license.lic"
ENCRYPTED_LICENSE="$TEST_OUTPUT/update_encrypt_license.lic.enc"
UPDATED_LICENSE="$TEST_OUTPUT/updated_encrypt_license.lic"
UPDATED_ENCRYPTED="$TEST_OUTPUT/updated_encrypt_license.lic.enc"

# Create test output directory
mkdir -p "$TEST_OUTPUT"

echo "Testing License Update with Automatic Encryption..."
echo "===================================================="

# Step 1: Generate initial license
echo "Step 1: Generating initial license..."
cat > "$TEST_OUTPUT/gen_for_update_config.json" << EOF
{
  "operation": "generate",
  "parameters": {
    "output": "$LICENSE_FILE",
    "license_id": "UPDATE-ENC-001",
    "customer_name": "Update Test User",
    "customer_email": "update@test.com"
  }
}
EOF

if ! "$BINARY" --config "$TEST_OUTPUT/gen_for_update_config.json" --package-config "$PROJECT_ROOT/config/package_config.json" --verbose; then
    echo "ERROR: Failed to generate initial license"
    exit 1
fi

if [ ! -f "$ENCRYPTED_LICENSE" ]; then
    echo "ERROR: Initial encrypted license not created"
    exit 1
fi

echo "✓ Initial license generated and encrypted"

# Step 2: Update the license (should automatically encrypt output)
echo ""
echo "Step 2: Updating license (should auto-encrypt)..."
cat > "$TEST_OUTPUT/update_encrypt_config.json" << EOF
{
  "operation": "update",
  "parameters": {
    "input_file": "$ENCRYPTED_LICENSE",
    "output_file": "$UPDATED_LICENSE",
    "customer_name": "Updated Test User"
  }
}
EOF

if ! "$BINARY" --config "$TEST_OUTPUT/update_encrypt_config.json" --package-config "$PROJECT_ROOT/config/package_config.json" --verbose; then
    echo "ERROR: Failed to update license"
    exit 1
fi

if [ ! -f "$UPDATED_ENCRYPTED" ]; then
    echo "ERROR: Updated encrypted license not created"
    exit 1
fi

echo "✓ License updated and re-encrypted successfully"

# Step 3: Verify updated encrypted license
echo ""
echo "Step 3: Verifying updated encrypted license..."
cat > "$TEST_OUTPUT/verify_updated_config.json" << EOF
{
  "operation": "verify",
  "parameters": {
    "license_file": "$UPDATED_ENCRYPTED",
    "check_expiry": "false"
  }
}
EOF

if ! "$BINARY" --config "$TEST_OUTPUT/verify_updated_config.json" --package-config "$PROJECT_ROOT/config/package_config.json" --verbose; then
    echo "ERROR: Failed to verify updated encrypted license"
    exit 1
fi

echo "✓ Updated encrypted license verified successfully"

# Step 4: Extract info and verify customer name was updated
echo ""
echo "Step 4: Verifying customer name was updated..."
cat > "$TEST_OUTPUT/info_updated_config.json" << EOF
{
  "operation": "get_license_info",
  "parameters": {
    "license_file": "$UPDATED_ENCRYPTED"
  }
}
EOF

OUTPUT=$("$BINARY" --config "$TEST_OUTPUT/info_updated_config.json" --package-config "$PROJECT_ROOT/config/package_config.json" 2>&1)

if echo "$OUTPUT" | grep -q "Updated Test User"; then
    echo "✓ Customer name update verified"
else
    echo "ERROR: Customer name not updated"
    exit 1
fi

# Step 5: Update license type
echo ""
echo "Step 5: Updating license type..."
cat > "$TEST_OUTPUT/update_type_config.json" << EOF
{
  "operation": "update",
  "parameters": {
    "input_file": "$UPDATED_ENCRYPTED",
    "output_file": "$UPDATED_LICENSE",
    "licence_type": "UltimaProfessionalLicence"
  }
}
EOF

if ! "$BINARY" --config "$TEST_OUTPUT/update_type_config.json" --package-config "$PROJECT_ROOT/config/package_config.json" --verbose; then
    echo "ERROR: Failed to update license type"
    exit 1
fi

echo "✓ License type updated successfully"

# Step 6: Verify license type was changed
echo ""
echo "Step 6: Verifying license type change..."
OUTPUT=$("$BINARY" --config "$TEST_OUTPUT/info_updated_config.json" --package-config "$PROJECT_ROOT/config/package_config.json" 2>&1)

if echo "$OUTPUT" | grep -q "UltimaProfessionalLicence"; then
    echo "✓ License type update verified"
else
    echo "ERROR: License type not updated"
    exit 1
fi

echo ""
echo "All license update with encryption tests passed!"
exit 0
