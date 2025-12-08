#ifndef URWT_RPC_SERVICE_HPP
#define URWT_RPC_SERVICE_HPP

#include "urwt/rpc/rpc_types.hpp"
#include "urwt/rpc/operation_tracker.hpp"
#include "urwt/rpc/request_dispatcher.hpp"
#include "urwt/config/config_manager.hpp"
#include "urwt/config/wireless_config_manager.hpp"
#include "urwt/api.hpp"
#include "urwt/mode/mode_controller.hpp"
#include "urwt/state/system_state_analyzer.hpp"
#include "urwt/network/saved_network_manager.hpp"
#include "urwt/config/startup_configurator.hpp"
#include "ThreadManager.hpp"
#include <direct_template.hpp>
#include <memory>
#include <atomic>
#include <string>
#include <optional>
#include <vector>

namespace urwt {
namespace rpc {

class RPCService {
public:
    RPCService();
    ~RPCService();

    std::optional<std::string> initialize(const std::string& rpcConfigPath,
                                          const std::string& wirelessConfigPath);
    std::optional<std::string> run();
    void stop();
    void shutdown();

    bool isRunning() const;
    bool isShutdownRequested() const;

    std::string getEffectiveInterface(const std::string& requestedInterface) const;
    uint64_t getNextWorkerNumber();

private:
    std::shared_ptr<config::ConfigManager> config_manager_;
    std::shared_ptr<config::WirelessConfigManager> wireless_config_manager_;
    std::shared_ptr<WirelessToolsAPI> wireless_api_;
    std::shared_ptr<mode::ModeController> mode_controller_;
    std::shared_ptr<state::SystemStateAnalyzer> state_analyzer_;
    std::shared_ptr<network::SavedNetworkManager> network_manager_;
    std::shared_ptr<ThreadMgr::ThreadManager> thread_manager_;
    std::unique_ptr<DirectTemplate::ClientThread> rpc_client_;
    std::shared_ptr<OperationTracker> operation_tracker_;
    std::shared_ptr<RequestDispatcher> request_dispatcher_;
    std::shared_ptr<config::StartupConfigurator> startup_configurator_;

    std::atomic<bool> running_;
    std::atomic<bool> shutdown_requested_;
    std::chrono::steady_clock::time_point start_time_;
    std::string config_path_;

    std::vector<WifiInterface> detected_interfaces_;
    std::string default_interface_name_;
    bool single_interface_mode_;
    std::atomic<uint64_t> worker_thread_counter_{0};

    void setupHandlers();
    void onMessage(const std::string& topic, const std::string& payload);
    void publishHeartbeat();

    std::optional<std::string> connectToMQTT();
    std::optional<std::string> subscribeToTopics();
    std::optional<std::string> loadAndApplyConfiguration();
};

} // namespace rpc
} // namespace urwt

#endif // URWT_RPC_SERVICE_HPP