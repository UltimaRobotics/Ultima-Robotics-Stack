
#include <iostream>
#include <fstream>
#include "operation_types.hpp"

using namespace urlic;

int main() {
    // Example 1: Create a generate license operation
    OperationRequest generate_req;
    generate_req.operation = OperationType::GENERATE;
    generate_req.parameters["license_id"] = "LIC-12345";
    generate_req.parameters["customer_name"] = "John Doe";
    generate_req.parameters["customer_email"] = "john@example.com";
    generate_req.parameters["output"] = "./test_license.lic";
    
    // Serialize to JSON
    nlohmann::json generate_json = generate_req.to_json();
    std::cout << "Generate Request JSON:\n" << generate_json.dump(2) << "\n\n";
    
    // Save to file
    std::ofstream generate_file("generate_operation.json");
    generate_file << generate_json.dump(2);
    generate_file.close();
    
    // Example 2: Create a verify license operation
    OperationRequest verify_req;
    verify_req.operation = OperationType::VERIFY;
    verify_req.parameters["license_file"] = "./test_license.lic";
    verify_req.parameters["check_expiry"] = "true";
    
    nlohmann::json verify_json = verify_req.to_json();
    std::cout << "Verify Request JSON:\n" << verify_json.dump(2) << "\n\n";
    
    // Example 3: Parse an operation result
    nlohmann::json result_json = {
        {"success", true},
        {"exit_code", 0},
        {"message", "License generated successfully"},
        {"data", {
            {"license_file", "./test_license.lic"},
            {"license_id", "LIC-12345"}
        }}
    };
    
    OperationResult result = OperationResult::from_json(result_json);
    std::cout << "Parsed Result:\n";
    std::cout << "  Success: " << (result.success ? "true" : "false") << "\n";
    std::cout << "  Exit Code: " << result.exit_code << "\n";
    std::cout << "  Message: " << result.message << "\n";
    
    // Example 4: Create and serialize license info
    LicenseInfo license_info;
    license_info.license_id = "LIC-12345";
    license_info.user_name = "John Doe";
    license_info.user_email = "john@example.com";
    license_info.product_name = "Ultima AIRLink";
    license_info.product_version = "1.0.0";
    license_info.license_tier = "Professional";
    license_info.license_type = "UltimaOpenLicence";
    
    nlohmann::json info_json = license_info.to_json();
    std::cout << "\nLicense Info JSON:\n" << info_json.dump(2) << "\n\n";
    
    // Example 5: Create verification result
    VerificationResult verif_result;
    verif_result.valid = true;
    verif_result.error_message = "";
    verif_result.license_info = license_info;
    
    nlohmann::json verif_json = verif_result.to_json();
    std::cout << "Verification Result JSON:\n" << verif_json.dump(2) << "\n";
    
    return 0;
}
