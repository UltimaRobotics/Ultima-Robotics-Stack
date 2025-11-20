
#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include <string>
#include <functional>
#include <microhttpd.h>
#include "vpn_instance_manager.hpp"

namespace http_server {

class HTTPServer {
public:
    HTTPServer(const std::string& host, int port);
    ~HTTPServer();
    
    bool start();
    void stop();
    bool isRunning() const;
    
    void setVPNManager(vpn_manager::VPNInstanceManager* manager);
    bool isVerbose() const;
    
private:
    struct MHD_Daemon* daemon;
    std::string host;
    int port;
    bool running;
    vpn_manager::VPNInstanceManager* vpn_manager;
    
    static MHD_Result handleRequest(void* cls, struct MHD_Connection* connection,
                                   const char* url, const char* method,
                                   const char* version, const char* upload_data,
                                   size_t* upload_data_size, void** con_cls);
    
    std::string processOperation(const std::string& json_data);
    
    static MHD_Result sendResponse(struct MHD_Connection* connection, 
                                  const std::string& response_data,
                                  int status_code = 200);
};

} // namespace http_server

#endif // HTTP_SERVER_HPP
