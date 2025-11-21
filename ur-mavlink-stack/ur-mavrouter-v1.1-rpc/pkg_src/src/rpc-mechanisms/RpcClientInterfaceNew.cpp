#include "RpcClientInterfaceNew.hpp"
#include "RpcClient.hpp"
#include "../common/log.h"

namespace RpcMechanisms {

RpcClientWrapper::RpcClientWrapper(const std::string& configPath, const std::string& clientId) {
    try {
        rpcClient_ = std::make_unique<RpcClient>(configPath, clientId);
        log_info("RpcClientWrapper initialized successfully");
    } catch (const std::exception& e) {
        log_error("Failed to initialize RpcClientWrapper: %s", e.what());
        rpcClient_.reset();
    }
}

RpcClientWrapper::~RpcClientWrapper() {
    if (rpcClient_) {
        stop();
    }
    log_info("RpcClientWrapper destroyed");
}

bool RpcClientWrapper::start() {
    if (!rpcClient_) {
        log_error("RpcClient not initialized");
        return false;
    }
    
    try {
        return rpcClient_->start();
    } catch (const std::exception& e) {
        log_error("Failed to start RPC client: %s", e.what());
        return false;
    }
}

void RpcClientWrapper::stop() {
    if (rpcClient_) {
        try {
            rpcClient_->stop();
        } catch (const std::exception& e) {
            log_error("Failed to stop RPC client: %s", e.what());
        }
    }
}

bool RpcClientWrapper::isRunning() const {
    if (!rpcClient_) {
        return false;
    }
    
    try {
        return rpcClient_->isRunning();
    } catch (const std::exception& e) {
        log_error("Failed to check RPC client status: %s", e.what());
        return false;
    }
}

std::string RpcClientWrapper::sendRpcRequest(const std::string& service, const std::string& method, const std::string& paramsJson) {
    if (!rpcClient_) {
        log_error("RpcClient not initialized - cannot send request");
        return "";
    }
    
    try {
        return rpcClient_->sendRpcRequest(service, method, paramsJson);
    } catch (const std::exception& e) {
        log_error("Failed to send RPC request: %s", e.what());
        return "";
    }
}

void RpcClientWrapper::sendResponse(const std::string& topic, const std::string& response) {
    if (!rpcClient_) {
        log_error("RpcClient not initialized - cannot send response");
        return;
    }
    
    try {
        rpcClient_->sendResponse(topic, response);
    } catch (const std::exception& e) {
        log_error("Failed to send RPC response: %s", e.what());
    }
}

void RpcClientWrapper::setMessageHandler(std::function<void(const std::string&, const std::string&)> handler) {
    if (!rpcClient_) {
        log_error("RpcClient not initialized - cannot set message handler");
        return;
    }
    
    try {
        rpcClient_->setMessageHandler(handler);
    } catch (const std::exception& e) {
        log_error("Failed to set RPC message handler: %s", e.what());
    }
}

} // namespace RpcMechanisms
