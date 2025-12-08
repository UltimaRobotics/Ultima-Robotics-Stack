#include "urwt/strategies/threaded_scan_strategy.hpp"
#include "urwt/parsers/iw_scan_parser.hpp"

namespace urwt {

ThreadedScanStrategy::ThreadedScanStrategy(std::shared_ptr<ProcessExecutor> executor)
    : executor_(executor ? executor : std::make_shared<ProcessExecutor>())
    , parser_(std::make_shared<IwScanParser>()) {}

ThreadedScanStrategy::~ThreadedScanStrategy() {
    if (scan_thread_.joinable()) {
        scan_thread_.join();
    }
}

Result<ScanResult, std::string> ThreadedScanStrategy::execute(const WifiInterface& interface) {
    auto start_time = std::chrono::steady_clock::now();
    
    auto result = executor_->execute("iw", {"dev", interface.name(), "scan"}, timeout_);
    
    if (result.isError()) {
        return Result<ScanResult, std::string>::error(result.error());
    }
    
    auto& proc_result = result.value();
    if (proc_result.exit_code != 0) {
        return Result<ScanResult, std::string>::error(
            "Scan command failed: " + proc_result.stderr_output
        );
    }
    
    auto networks_result = parser_->parse(proc_result.stdout_output);
    if (networks_result.isError()) {
        return Result<ScanResult, std::string>::error(networks_result.error());
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    ScanResult scan_result(interface, networks_result.value(), duration);
    return Result<ScanResult, std::string>::ok(scan_result);
}

void ThreadedScanStrategy::cancel() {
    cancelled_ = true;
    running_ = false;
}

}
