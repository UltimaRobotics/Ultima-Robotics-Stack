#include "urwt/strategies/pipe_scan_strategy.hpp"
#include "urwt/parsers/iw_scan_parser.hpp"

namespace urwt {

PipeScanStrategy::PipeScanStrategy(std::shared_ptr<ProcessExecutor> executor)
    : executor_(executor ? executor : std::make_shared<ProcessExecutor>())
    , parser_(std::make_shared<IwScanParser>()) {}

Result<ScanResult, std::string> PipeScanStrategy::execute(const WifiInterface& interface) {
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

void PipeScanStrategy::cancel() {
    cancelled_ = true;
}

}
