
#ifndef SERVER_STATUS_PROGRESS_SERIALIZER_HPP
#define SERVER_STATUS_PROGRESS_SERIALIZER_HPP

#include "json.hpp"
#include <string>

using json = nlohmann::json;

namespace NetBench {
namespace Shared {

struct ServerStatusProgress {
    int total_servers;
    int tested_servers;
    int percentage;
    std::string current_server_name;
    std::string current_server_host;
    std::string timestamp;
    
    ServerStatusProgress() 
        : total_servers(0),
          tested_servers(0),
          percentage(0) {}
};

class ServerStatusProgressSerializer {
public:
    static json serialize(const ServerStatusProgress& progress);
    static ServerStatusProgress deserialize(const json& j);
    static void exportToFile(const ServerStatusProgress& progress, const std::string& filepath);
};

} // namespace Shared
} // namespace NetBench

#endif // SERVER_STATUS_PROGRESS_SERIALIZER_HPP
