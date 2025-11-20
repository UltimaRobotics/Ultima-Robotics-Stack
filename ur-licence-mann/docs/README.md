# ur-licence-mann - Advanced License Management Tool

A comprehensive license management system with encryption, hardware fingerprinting, and secure signature verification.

## Features

- **Automatic Key Generation**: RSA key pairs and encryption keys are automatically generated on first run
- **Hardware Binding**: Licenses can be bound to specific hardware with fingerprinting
- **Encrypted Storage**: License definitions and licenses are automatically encrypted
- **Signature Verification**: All licenses include hardware-bound signatures for authenticity
- **Flexible Operations**: Support for both JSON config files and command-line parameters
- **GET Operations**: Retrieve license information, plans, and definitions programmatically

## New Operation-Based Interface

### Command Line Flags

- `--package-config <file>`: Path to package configuration JSON file
- `-o, --operation <op>`: Operation to perform
- `--operation-config <file>`: Path to operation configuration JSON file  
- `-v, --verbose`: Enable verbose output

### Supported Operations

1. **generate** - Generate a new license
2. **verify** - Verify an existing license
3. **update** - Update/renew an existing license
4. **get_license_info** - Retrieve all license information
5. **get_license_plan** - Retrieve license plan details
6. **get_license_definitions** - Retrieve license type definitions

## Quick Start

### 1. Initialize the System

This automatically generates encryption keys and RSA key pairs:

```bash
./ur-licence-mann --package-config example_package_config.json --verbose
```

This will create:
- `./keys/private_key.pem` - RSA private key (2048-bit)
- `./keys/public_key.pem` - RSA public key
- `./keys/encryption_keys.json` - Master encryption key
- Required directories (keys/, config/, licenses/)

### 2. Generate a License

Using operation config file:

```bash
./ur-licence-mann --package-config example_package_config.json \
                  -o generate \
                  --operation-config example_operation_generate.json
```

Using command-line parameters:

```bash
./ur-licence-mann --package-config example_package_config.json \
                  -o generate \
                  output=./licenses/license.lic \
                  user_name="John Doe" \
                  user_email="john@example.com" \
                  expiry_date="2026-12-31"
```

### 3. Verify a License

```bash
./ur-licence-mann --package-config example_package_config.json \
                  -o verify \
                  license_file=./licenses/license.lic
```

### 4. Get License Information

```bash
./ur-licence-mann --package-config example_package_config.json \
                  -o get_license_info \
                  license_file=./licenses/license.lic
```

### 5. Get License Plan

```bash
./ur-licence-mann --package-config example_package_config.json \
                  -o get_license_plan \
                  license_file=./licenses/license.lic
```

### 6. Get License Definitions

```bash
./ur-licence-mann --package-config example_package_config.json \
                  -o get_license_definitions
```

## Configuration Files

### Package Configuration (example_package_config.json)

```json
{
  "keys_directory": "./keys",
  "config_directory": "./config",
  "licenses_directory": "./licenses",
  "license_definitions_file": "./config/license_definitions.json",
  "encrypted_license_definitions_file": "./config/license_definitions.enc",
  "encryption_keys_file": "./keys/encryption_keys.json",
  "private_key_file": "./keys/private_key.pem",
  "public_key_file": "./keys/public_key.pem",
  "master_encryption_key": "",
  "auto_encrypt_definitions": true,
  "auto_encrypt_licenses": true,
  "require_hardware_binding": true,
  "require_signature": true
}
```

### Operation Configuration Examples

#### Generate Operation (example_operation_generate.json)

```json
{
  "operation": "generate",
  "parameters": {
    "output": "./licenses/license.lic",
    "user_name": "John Doe",
    "user_email": "john@example.com",
    "expiry_date": "2026-12-31",
    "product_name": "MyApp",
    "product_version": "1.0.0"
  }
}
```

#### Verify Operation (example_operation_verify.json)

```json
{
  "operation": "verify",
  "parameters": {
    "license_file": "./licenses/license.lic"
  }
}
```

## Security Features

1. **Automatic Encryption**: 
   - Encryption keys are auto-generated using cryptographically secure random number generation
   - All license files are encrypted with AES-256
   - License definitions are encrypted when `auto_encrypt_definitions=true`

2. **Hardware Binding**:
   - Licenses include hardware fingerprints
   - Signatures contain hardware identification
   - Only ur-licence-mann can generate and update licenses with valid signatures

3. **RSA Signatures**:
   - 2048-bit RSA key pairs for signing
   - All licenses are cryptographically signed
   - Signature verification ensures license authenticity

## Workflow Integration

The build workflow automatically compiles the project:

```bash
cd ur-licence-mann/build && make
```

The compiled binary is located at: `ur-licence-mann/build/ur-licence-mann`

## Building from Source

1. Ensure you have GCC with C++20 support, CMake, and OpenSSL installed
2. Configure and build:

```bash
cd ur-licence-mann
mkdir -p build && cd build
cmake ..
make -j4
```

## Architecture

- **C++20** with GCC compiler
- **OpenSSL 3.4.1** for cryptographic operations
- **nlohmann/json** for JSON handling
- **CLI11** for command-line parsing
- **licensecxx** library for license operations

## License Management Features

- **Encrypted storage**: All sensitive data is encrypted at rest
- **Hardware-bound licenses**: Prevent license transfer between machines
- **Expiration management**: Built-in expiry date verification
- **Custom fields**: Add arbitrary metadata to licenses
- **Feature flags**: Define per-license-type feature access
- **Automatic init**: Keys and directories created on first run
- **JSON-based config**: Easy integration with CI/CD and automation

## Notes

- All licenses automatically include hardware fingerprints when `require_hardware_binding=true`
- License definitions are automatically encrypted on initialization
- Encryption keys are securely stored and loaded automatically
- The system initializes on every startup to ensure keys and directories exist
