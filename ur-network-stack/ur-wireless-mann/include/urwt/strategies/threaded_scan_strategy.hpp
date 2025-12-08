#ifndef URWT_STRATEGIES_THREADED_SCAN_STRATEGY_HPP
#define URWT_STRATEGIES_THREADED_SCAN_STRATEGY_HPP

#include "scan_strategy.hpp"
#include "../utils/process_executor.hpp"
#include <memory>
#include <thread>
#include <atomic>

namespace urwt {

class IwScanParser;

class ThreadedScanStrategy : public ScanStrategy {
public:
    explicit ThreadedScanStrategy(std::shared_ptr<ProcessExecutor> executor = nullptr);
    ~ThreadedScanStrategy() override;

    Result<ScanResult, std::string> execute(const WifiInterface& interface) override;
    void cancel() override;
    std::string name() const override { return "threaded"; }

private:
    std::shared_ptr<ProcessExecutor> executor_;
    std::shared_ptr<IwScanParser> parser_;
    std::atomic<bool> running_{false};
    std::thread scan_thread_;
};

}

#endif
