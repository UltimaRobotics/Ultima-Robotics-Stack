#!/bin/bash

# Test Runner for ur-licence-mann
# This script runs all tests and generates a report

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BINARY="$PROJECT_ROOT/build/ur-licence-mann"
TEST_OUTPUT_DIR="$SCRIPT_DIR/output"

# Create output directory
mkdir -p "$TEST_OUTPUT_DIR"

# Log file
LOG_FILE="$TEST_OUTPUT_DIR/test_results_$(date +%Y%m%d_%H%M%S).log"

echo "==================================================" | tee "$LOG_FILE"
echo "ur-licence-mann Test Suite" | tee -a "$LOG_FILE"
echo "==================================================" | tee -a "$LOG_FILE"
echo "Test started at: $(date)" | tee -a "$LOG_FILE"
echo "" | tee -a "$LOG_FILE"

# Check if binary exists
if [ ! -f "$BINARY" ]; then
    echo -e "${RED}ERROR: Binary not found at $BINARY${NC}" | tee -a "$LOG_FILE"
    echo "Please build the project first: cd build && make" | tee -a "$LOG_FILE"
    exit 1
fi

echo -e "${GREEN}Binary found: $BINARY${NC}" | tee -a "$LOG_FILE"
echo "" | tee -a "$LOG_FILE"

# Function to run a test
run_test() {
    local test_name="$1"
    local test_script="$2"

    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    echo "---------------------------------------------------" | tee -a "$LOG_FILE"
    echo "Running: $test_name" | tee -a "$LOG_FILE"
    echo "---------------------------------------------------" | tee -a "$LOG_FILE"

    if bash "$test_script" >> "$LOG_FILE" 2>&1; then
        echo -e "${GREEN}✓ PASSED${NC}: $test_name" | tee -a "$LOG_FILE"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo -e "${RED}✗ FAILED${NC}: $test_name" | tee -a "$LOG_FILE"
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
    echo "" | tee -a "$LOG_FILE"
}

# Run all test scripts
run_test "License Generation Test" "$SCRIPT_DIR/test_generate.sh"
run_test "License Verification Test" "$SCRIPT_DIR/test_verify.sh"
run_test "Update Test" "$SCRIPT_DIR/test_update.sh"
run_test "Update License Type Test" "$SCRIPT_DIR/test_update_license_type.sh"
run_test "Get License Info Test" "$SCRIPT_DIR/test_get_info.sh"
run_test "Get License Plan Test" "$SCRIPT_DIR/test_get_plan.sh"
run_test "Encryption Test" "$SCRIPT_DIR/test_encryption.sh"
run_test "Hardware Binding Test" "$SCRIPT_DIR/test_hardware_binding.sh"
run_test "Builtin Encryption Key Test" "$SCRIPT_DIR/test_builtin_encryption.sh"
run_test "Auto Encrypt Definitions Test" "$SCRIPT_DIR/test_auto_encrypt_definitions.sh"
run_test "Update with Encryption Test" "$SCRIPT_DIR/test_update_with_encryption.sh"
run_test "Force Encryption Flags Test" "$SCRIPT_DIR/test_force_encryption.sh"

# Summary
echo "==================================================" | tee -a "$LOG_FILE"
echo "Test Summary" | tee -a "$LOG_FILE"
echo "==================================================" | tee -a "$LOG_FILE"
echo "Total Tests:  $TOTAL_TESTS" | tee -a "$LOG_FILE"
echo -e "${GREEN}Passed:       $PASSED_TESTS${NC}" | tee -a "$LOG_FILE"
echo -e "${RED}Failed:       $FAILED_TESTS${NC}" | tee -a "$LOG_FILE"
echo "" | tee -a "$LOG_FILE"
echo "Detailed log: $LOG_FILE" | tee -a "$LOG_FILE"
echo "Test completed at: $(date)" | tee -a "$LOG_FILE"

# Exit with appropriate code
if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed!${NC}"
    exit 1
fi