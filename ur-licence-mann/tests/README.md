
# ur-licence-mann Test Suite

This directory contains comprehensive tests for the ur-licence-mann binary.

## Test Structure

- `test_runner.sh` - Main test runner that executes all tests
- `test_init.sh` - Tests initialization and key generation
- `test_generate.sh` - Tests license generation
- `test_verify.sh` - Tests license verification
- `test_update.sh` - Tests license update functionality
- `test_get_info.sh` - Tests extracting license information
- `test_get_plan.sh` - Tests extracting license plan
- `test_get_definitions.sh` - Tests retrieving license definitions
- `test_encryption.sh` - Tests encryption/decryption features
- `test_hardware_binding.sh` - Tests hardware binding functionality

## Running Tests

### Run All Tests

```bash
cd ur-licence-mann/tests
chmod +x test_runner.sh
./test_runner.sh
```

### Run Individual Test

```bash
cd ur-licence-mann/tests
chmod +x test_init.sh
./test_init.sh
```

## Test Output

- Test results are logged to `output/test_results_YYYYMMDD_HHMMSS.log`
- Generated test files are stored in `output/`
- Test configurations are in `test_data/`

## Prerequisites

1. Build the project first:
```bash
cd ur-licence-mann/build
cmake -DPRODUCT_NAME="TestProduct" -DPRODUCT_VERSION="1.0.0" -DDEVICE_MODEL="TestDevice" ..
make
```

2. Ensure the binary exists at `ur-licence-mann/build/ur-licence-mann`

## Test Coverage

The test suite verifies:
- ✓ Key generation (RSA and AES)
- ✓ License generation with various tiers
- ✓ License verification (valid and invalid)
- ✓ License updating
- ✓ Information extraction
- ✓ Plan extraction
- ✓ Definition retrieval
- ✓ Encryption/decryption
- ✓ Hardware binding
