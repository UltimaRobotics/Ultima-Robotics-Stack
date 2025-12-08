#ifndef NETBENCH_OPERATION_HANDLER_HPP
#define NETBENCH_OPERATION_HANDLER_HPP

#include <string>
#include <map>
#include <vector>
#include "json.hpp"

using json = nlohmann::json;

class NetbenchOperationHandler {
public:
    // Main execution interface
    static int execute(const std::string& operationConfigJson, 
                      const std::string& packageConfigJson, 
                      bool verbose = false);
    
private:
    // Specific operation handlers
    static int handleServersStatus(const std::map<std::string, std::string>& params, 
                                  const json& packageConfig, bool verbose);
    
    static int handlePingTest(const std::map<std::string, std::string>& params, 
                             const json& packageConfig, bool verbose);
    
    static int handleTracerouteTest(const std::map<std::string, std::string>& params, 
                                   const json& packageConfig, bool verbose);
    
    static int handleIperfTest(const std::map<std::string, std::string>& params, 
                              const json& packageConfig, bool verbose);
    
    static int handleDnsLookup(const std::map<std::string, std::string>& params, 
                              const json& packageConfig, bool verbose);
    
    // Utility methods
    static std::map<std::string, std::string> parseParams(const json& paramsObj);
    static std::string executeCommand(const std::string& command, bool verbose);
    static std::string generateResultJson(int exitCode, const std::string& output, const std::string& operation);
    static void logOperation(const std::string& operation, const std::map<std::string, std::string>& params, bool verbose);
};

#endif // NETBENCH_OPERATION_HANDLER_HPP
