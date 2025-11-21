#pragma once
#include <string>
#include <memory>
#include <functional>

// Forward declaration for the actual RpcClient
class RpcClient;

namespace RpcMechanisms {

/**
 * @brief Minimal RPC Client interface for HTTP server
 * 
 * This provides a clean interface that the HTTP server can use without
 * needing to include all the heavy ur-rpc-template dependencies.
 */
class RpcClientInterface {
public:
    virtual ~RpcClientInterface() = default;
    
    /**
     * @brief Start the RPC client
     * @return true if successful, false otherwise
     */
    virtual bool start() = 0;
    
    /**
     * @brief Stop the RPC client
     */
    virtual void stop() = 0;
    
    /**
     * @brief Check if RPC client is running
     * @return true if running, false otherwise
     */
    virtual bool isRunning() const = 0;
    
    /**
     * @brief Send RPC request
     * @param service Target service
     * @param method RPC method
     * @param paramsJson JSON parameters
     * @return Transaction ID used for the request
     */
    virtual std::string sendRpcRequest(const std::string& service, const std::string& method, const std::string& paramsJson) = 0;
    
    /**
     * @brief Send response
     * @param topic Response topic
     * @param response Response payload
     */
    virtual void sendResponse(const std::string& topic, const std::string& response) = 0;
    
    /**
     * @brief Set message handler
     * @param handler Message handler function
     */
    virtual void setMessageHandler(std::function<void(const std::string&, const std::string&)> handler) = 0;
};

/**
 * @brief RPC Client implementation wrapper
 * 
 * This class implements the interface and delegates to the actual RpcClient
 * while hiding the ur-rpc-template dependencies from the HTTP module.
 */
class RpcClientWrapper : public RpcClientInterface {
public:
    /**
     * @brief Constructor
     * @param configPath Path to RPC configuration
     * @param clientId Client ID
     */
    RpcClientWrapper(const std::string& configPath, const std::string& clientId);
    
    /**
     * @brief Destructor
     */
    ~RpcClientWrapper() override;
    
    // Interface implementation
    bool start() override;
    void stop() override;
    bool isRunning() const override;
    std::string sendRpcRequest(const std::string& service, const std::string& method, const std::string& paramsJson) override;
    void sendResponse(const std::string& topic, const std::string& response) override;
    void setMessageHandler(std::function<void(const std::string&, const std::string&)> handler) override;
    
    /**
     * @brief Get access to the underlying RPC client
     * @return Pointer to actual RPC client
     */
    RpcClient* getUnderlyingClient() { return rpcClient_.get(); }

private:
    std::unique_ptr<RpcClient> rpcClient_;
};

} // namespace RpcMechanisms
