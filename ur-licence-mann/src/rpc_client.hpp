#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <ThreadManager.hpp>

class RpcClient {
public:
	// Construct with path to RPC config JSON and optional clientId override
	RpcClient(const std::string& configPath, const std::string& clientId);
	~RpcClient();

	// Start the RPC client in a separate thread
	bool start();
	void stop();

	bool isRunning() const;

	// Message handler hook for application-specific processing
	void setMessageHandler(std::function<void(const std::string&, const std::string&)> handler);

	// Send response back to requester
	void sendResponse(const std::string& topic, const std::string& response);

private:
	std::string configPath_;
	std::string clientId_;
	std::atomic<bool> running_{false};
	std::function<void(const std::string&, const std::string&)> messageHandler_;

	// ThreadManager for managing the RPC client thread
	std::unique_ptr<ThreadMgr::ThreadManager> threadManager_;
	unsigned int rpcThreadId_{0};

	// Thread function for RPC client
	void rpcClientThreadFunc();

	static void staticMessageHandler(const char* topic, const char* payload, size_t payload_len, void* user_data);
};