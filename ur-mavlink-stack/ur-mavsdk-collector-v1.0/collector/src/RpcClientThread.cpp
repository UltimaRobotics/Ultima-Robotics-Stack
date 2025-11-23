#include "RpcClientThread.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <sstream>

static RpcClientThread* g_instance = nullptr;

RpcClientThread::RpcClientThread(ThreadMgr::ThreadManager* threadManager, 
                               const std::string& rpcConfigPath,
                               const std::string& clientId)
    : threadManager_(threadManager), rpcConfigPath_(rpcConfigPath), clientId_(clientId), rpcThreadId_(0) {
    g_instance = this;
}

RpcClientThread::~RpcClientThread() {
    stop();
    g_instance = nullptr;
}

bool RpcClientThread::start() {
    if (running_.load()) {
        std::cout << "RPC client thread already running" << std::endl;
        return true;
    }

    try {
        std::cout << "Starting RPC client thread initialization..." << std::endl;
        
        running_.store(true);
        
        auto rpc_thread_id = threadManager_->createThread([this]() {
            this->rpcClientThreadFunc();
        });
        
        rpcThreadId_ = rpc_thread_id;
        
        std::cout << "RPC client thread started successfully with ID: " << rpcThreadId_ << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Failed to start RPC client thread: " << e.what() << std::endl;
        running_.store(false);
        return false;
    }
}

void RpcClientThread::stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);
    
    if (rpcThreadId_ != 0) {
        try {
            bool completed = threadManager_->joinThread(rpcThreadId_, std::chrono::seconds(5));
            if (!completed) {
                std::cout << "RPC client thread did not complete in time, stopping forcefully" << std::endl;
                threadManager_->stopThread(rpcThreadId_);
                threadManager_->joinThread(rpcThreadId_, std::chrono::seconds(2));
            }
            
        } catch (const std::exception& e) {
            std::cerr << "Failed to join RPC client thread: " << e.what() << std::endl;
        }
        rpcThreadId_ = 0;
    }

    std::cout << "RPC client thread stopped" << std::endl;
}

bool RpcClientThread::isRunning() const {
    return running_.load();
}

void RpcClientThread::setMessageHandler(std::function<void(const std::string&, const std::string&)> handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    messageHandler_ = handler;
}

void RpcClientThread::sendResponse(const std::string& topic, const std::string& response) {
    if (!urpcClient_ || !running_.load()) {
        std::cerr << "RPC client not available for sending response" << std::endl;
        return;
    }

    try {
        urpcClient_->publishMessage(topic, response);
        std::cout << "Sent response to topic: " << topic << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to send response: " << e.what() << std::endl;
    }
}

void RpcClientThread::sendRpcRequest(const std::string& service, const std::string& method, const std::string& paramsJson) {
    if (!urpcClient_ || !running_.load()) {
        std::cerr << "RPC client not available for sending requests" << std::endl;
        return;
    }

    try {
        std::string transactionId = generateTransactionId();
        
        std::ostringstream requestJson;
        requestJson << "{";
        requestJson << "\"jsonrpc\":\"2.0\",";
        requestJson << "\"id\":\"" << transactionId << "\",";
        requestJson << "\"method\":\"" << method << "\",";
        requestJson << "\"params\":" << paramsJson;
        requestJson << "}";
        
        std::string topic = "direct_messaging/" + service + "/requests";
        urpcClient_->publishMessage(topic, requestJson.str());
        
        std::cout << "Sent RPC request to " << service << "." << method << " on topic: " << topic << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to send RPC request: " << e.what() << std::endl;
    }
}

void RpcClientThread::rpcClientThreadFunc() {
    std::cout << "RPC client thread started successfully" << std::endl;
    
    try {
        clientConfig_ = std::make_unique<UrRpc::ClientConfig>();
        clientConfig_->loadFromFile(rpcConfigPath_);
        
        topicConfig_ = std::make_unique<UrRpc::TopicConfig>();
        
        urpcClient_ = std::make_unique<UrRpc::Client>(*clientConfig_, *topicConfig_);
        
        urpcClient_->setMessageHandler([this](const std::string& topic, const std::string& payload) {
            if (g_instance) {
                g_instance->messageCallbackWrapper(topic, payload);
            }
        });
        
        urpcClient_->connect();
        
        std::cout << "RPC client connected to broker successfully" << std::endl;
        
        int heartbeatCounter = 0;
        while (running_.load()) {
            try {
                heartbeatCounter++;
                if (heartbeatCounter % 30 == 0) {
                    std::cout << "RPC client heartbeat #" << heartbeatCounter << std::endl;
                }
                
                urpcClient_->start();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                
            } catch (const std::exception& e) {
                std::cerr << "Exception in RPC client thread: " << e.what() << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize RPC client: " << e.what() << std::endl;
    }
    
    if (urpcClient_) {
        try {
            urpcClient_->disconnect();
        } catch (const std::exception& e) {
            std::cerr << "Error disconnecting RPC client: " << e.what() << std::endl;
        }
    }
    
    std::cout << "RPC client thread stopped" << std::endl;
}

std::string RpcClientThread::generateTransactionId() {
    static std::atomic<int> counter{0};
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    std::ostringstream id;
    id << "tx_" << timestamp << "_" << counter.fetch_add(1);
    return id.str();
}

void RpcClientThread::messageCallbackWrapper(const std::string& topic, const std::string& payload) {
    try {
        std::cout << "Received message on topic: " << topic << std::endl;
        
        if (g_instance && g_instance->messageHandler_) {
            g_instance->messageHandler_(topic, payload);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error in message callback: " << e.what() << std::endl;
    }
}
