
#include "ServerStatusProgressSerializer.hpp"
#include <fstream>

namespace NetBench {
namespace Shared {

json ServerStatusProgressSerializer::serialize(const ServerStatusProgress& progress) {
    json j;
    j["total_servers"] = progress.total_servers;
    j["tested_servers"] = progress.tested_servers;
    j["percentage"] = progress.percentage;
    j["current_server_name"] = progress.current_server_name;
    j["current_server_host"] = progress.current_server_host;
    j["timestamp"] = progress.timestamp;
    j["status_message"] = "Progress: " + std::to_string(progress.percentage) + "% [" + 
                          std::to_string(progress.tested_servers) + "/" + 
                          std::to_string(progress.total_servers) + "] Testing: " + 
                          progress.current_server_name;
    return j;
}

ServerStatusProgress ServerStatusProgressSerializer::deserialize(const json& j) {
    ServerStatusProgress progress;
    progress.total_servers = j.value("total_servers", 0);
    progress.tested_servers = j.value("tested_servers", 0);
    progress.percentage = j.value("percentage", 0);
    progress.current_server_name = j.value("current_server_name", "");
    progress.current_server_host = j.value("current_server_host", "");
    progress.timestamp = j.value("timestamp", "");
    return progress;
}

void ServerStatusProgressSerializer::exportToFile(const ServerStatusProgress& progress, const std::string& filepath) {
    json j = serialize(progress);
    std::ofstream file(filepath);
    if (file.is_open()) {
        file << j.dump(2);
        file.close();
    }
}

} // namespace Shared
} // namespace NetBench
