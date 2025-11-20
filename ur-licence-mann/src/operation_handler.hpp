#pragma once

#include <string>
#include <map>
#include <nlohmann/json.hpp>
#include "package_config.hpp"
#include "operation_types.hpp"

// Type alias for backwards compatibility
using OperationType = urlic::OperationType;
using OperationConfig = urlic::OperationRequest;

class OperationHandler {
public:
    static int execute(const OperationConfig& op_config, const PackageConfig& pkg_config, bool verbose);
    
private:
    static int handle_generate(const std::map<std::string, std::string>& params, 
                              const PackageConfig& pkg_config, bool verbose);
    static int handle_verify(const std::map<std::string, std::string>& params, 
                            const PackageConfig& pkg_config, bool verbose);
    static int handle_update(const std::map<std::string, std::string>& params, 
                            const PackageConfig& pkg_config, bool verbose);
    static int handle_get_license_info(const std::map<std::string, std::string>& params, 
                                      const PackageConfig& pkg_config, bool verbose);
    static int handle_get_license_plan(const std::map<std::string, std::string>& params, 
                                      const PackageConfig& pkg_config, bool verbose);
    static int handle_get_license_definitions(const std::map<std::string, std::string>& params, 
                                             const PackageConfig& pkg_config, bool verbose);
    static int handle_update_license_definitions(const std::map<std::string, std::string>& params, 
                                                 const PackageConfig& pkg_config, bool verbose);
};
