
# ur-licence-shared Library

This is a shared library for ur-licence-mann that provides common data structures and JSON serialization/deserialization for operations and results.

## Purpose

The `ur-licence-shared` library allows other binaries to:
- Generate JSON operation requests that ur-licence-mann can process
- Parse JSON results from ur-licence-mann operations
- Use standardized data structures for license operations

## Data Structures

### OperationRequest
Represents a license operation request.

**Fields:**
- `operation`: The type of operation (GENERATE, VERIFY, UPDATE, etc.)
- `parameters`: Key-value map of operation parameters

**Methods:**
- `to_json()`: Serialize to JSON
- `from_json(json)`: Deserialize from JSON

### OperationResult
Represents the result of an operation.

**Fields:**
- `success`: Boolean indicating success/failure
- `exit_code`: Exit code (0 for success)
- `message`: Human-readable message
- `data`: Key-value map of result data

**Methods:**
- `to_json()`: Serialize to JSON
- `from_json(json)`: Deserialize from JSON

### LicenseInfo
Contains detailed license information.

**Fields:**
- `license_id`, `user_name`, `user_email`
- `product_name`, `product_version`
- `device_hardware_id`, `device_model`, `device_mac`
- `issued_at`, `valid_until`
- `license_tier`, `license_type`
- `signature_algorithm`

**Methods:**
- `to_json()`: Serialize to JSON
- `from_json(json)`: Deserialize from JSON

### VerificationResult
Result of license verification.

**Fields:**
- `valid`: Boolean indicating if license is valid
- `error_message`: Error description if invalid
- `license_info`: Detailed license information

**Methods:**
- `to_json()`: Serialize to JSON
- `from_json(json)`: Deserialize from JSON

### LicensePlan
License plan information.

**Fields:**
- `license_type`, `license_tier`
- `product`, `version`, `expiry`

**Methods:**
- `to_json()`: Serialize to JSON
- `from_json(json)`: Deserialize from JSON

## Building

The library is built as part of the main ur-licence-mann project:

```bash
cd ur-licence-mann
mkdir -p build && cd build
cmake ..
make
```

The static library will be available at: `build/lib/libur-licence-shared.a`

## Usage Example

```cpp
#include <operation_types.hpp>
using namespace urlic;

// Create an operation request
OperationRequest req;
req.operation = OperationType::GENERATE;
req.parameters["license_id"] = "LIC-001";
req.parameters["customer_name"] = "John Doe";

// Serialize to JSON
nlohmann::json json = req.to_json();
std::cout << json.dump(2) << std::endl;

// Save to file for ur-licence-mann to process
std::ofstream file("operation.json");
file << json.dump(2);
file.close();

// Parse result
std::ifstream result_file("result.json");
nlohmann::json result_json;
result_file >> result_json;

OperationResult result = OperationResult::from_json(result_json);
if (result.success) {
    std::cout << "Operation succeeded: " << result.message << std::endl;
}
```

## Linking Against the Library

In your CMakeLists.txt:

```cmake
# Add the shared library directory
add_subdirectory(path/to/ur-licence-mann/shared-library)

# Include directories
include_directories(path/to/ur-licence-mann/shared-library/include)

# Link against the library
target_link_libraries(your_binary
    PRIVATE
        ur-licence-shared
)
```

## Operation Types

- `GENERATE`: Generate a new license
- `VERIFY`: Verify an existing license
- `UPDATE`: Update license fields
- `GET_LICENSE_INFO`: Extract license information
- `GET_LICENSE_PLAN`: Get license plan details
- `GET_LICENSE_DEFINITIONS`: Get license type definitions
- `UPDATE_LICENSE_DEFINITIONS`: Update license type definitions
- `INIT`: Initialize license system

## JSON Format Examples

### Generate Operation Request
```json
{
  "operation": "generate",
  "parameters": {
    "license_id": "LIC-12345",
    "customer_name": "John Doe",
    "customer_email": "john@example.com",
    "output": "./license.lic"
  }
}
```

### Operation Result
```json
{
  "success": true,
  "exit_code": 0,
  "message": "License generated successfully",
  "data": {
    "license_file": "./license.lic",
    "license_id": "LIC-12345"
  }
}
```

### Verification Result
```json
{
  "valid": true,
  "error_message": "",
  "license_info": {
    "license_id": "LIC-12345",
    "user_name": "John Doe",
    "user_email": "john@example.com",
    "product_name": "Ultima AIRLink",
    "license_tier": "Professional",
    "license_type": "UltimaOpenLicence"
  }
}
```
