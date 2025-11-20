
<![CDATA[#!/bin/bash

# Test license type update

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BINARY="$PROJECT_ROOT/build/ur-licence-mann"
TEST_CONFIG="$SCRIPT_DIR/test_data/test_package_config.json"
TEST_LICENSE="$SCRIPT_DIR/output/test_license.lic"
UPDATED_LICENSE="$SCRIPT_DIR/output/updated_license_type.lic"

echo "Testing license type update..."

# Ensure we have a license
if [ ! -f "$TEST_LICENSE" ]; then
    bash "$SCRIPT_DIR/test_generate.sh" > /dev/null 2>&1
fi

# Update license type from UltimaOpenLicence to UltimaProfessionalLicence
UPDATE_OUTPUT=$($BINARY --package-config "$TEST_CONFIG" -o update \
    input_file="$TEST_LICENSE" \
    output_file="$UPDATED_LICENSE" \
    licence_type="UltimaProfessionalLicence" \
    -v 2>&1)
UPDATE_EXIT_CODE=$?

# Check if updated license exists
if [ -f "$UPDATED_LICENSE" ]; then
    echo "✓ Updated license created"
    
    # Get license info to verify the type changed
    INFO_OUTPUT=$($BINARY --package-config "$TEST_CONFIG" -o get_license_info \
        license_file="$UPDATED_LICENSE" \
        -v 2>&1)
    INFO_EXIT_CODE=$?
    
    if echo "$INFO_OUTPUT" | grep -q "UltimaProfessionalLicence"; then
        echo "✓ License type updated successfully to UltimaProfessionalLicence"
    else
        echo "✗ License type was not updated"
        echo "=========================================="
        echo "VERBOSE FAILURE LOG:"
        echo "=========================================="
        echo "Operation: Update license type"
        echo "Parameters:"
        echo "  - input_file: $TEST_LICENSE"
        echo "  - output_file: $UPDATED_LICENSE"
        echo "  - licence_type: UltimaProfessionalLicence"
        echo "Info Exit Code: $INFO_EXIT_CODE"
        echo ""
        echo "Expected Result:"
        echo "  - licence_type should be UltimaProfessionalLicence"
        echo ""
        echo "Actual Result:"
        echo "  - licence_type not found or incorrect"
        echo ""
        echo "Update Command Output:"
        echo "$UPDATE_OUTPUT"
        echo ""
        echo "Info Output:"
        echo "$INFO_OUTPUT"
        echo "=========================================="
        exit 1
    fi
else
    echo "✗ Updated license not created"
    echo "=========================================="
    echo "VERBOSE FAILURE LOG:"
    echo "=========================================="
    echo "Operation: Update license type"
    echo "Parameters:"
    echo "  - input_file: $TEST_LICENSE"
    echo "  - output_file: $UPDATED_LICENSE"
    echo "  - licence_type: UltimaProfessionalLicence"
    echo "Exit Code: $UPDATE_EXIT_CODE"
    echo ""
    echo "Expected Result:"
    echo "  - Updated license at: $UPDATED_LICENSE"
    echo ""
    echo "Actual Result:"
    echo "  - File exists: NO"
    echo ""
    echo "Command Output:"
    echo "$UPDATE_OUTPUT"
    echo "=========================================="
    exit 1
fi

echo "✓ License type update test passed"
exit 0
]]>
