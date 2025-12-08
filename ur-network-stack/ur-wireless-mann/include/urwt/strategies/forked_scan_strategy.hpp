#ifndef URWT_STRATEGIES_FORKED_SCAN_STRATEGY_HPP
#define URWT_STRATEGIES_FORKED_SCAN_STRATEGY_HPP

#include "scan_strategy.hpp"
#include "../utils/process_executor.hpp"
#include <memory>

namespace urwt {

class IwScanParser;

class ForkedScanStrategy : public ScanStrategy {
public:
    explicit ForkedScanStrategy(std::shared_ptr<ProcessExecutor> executor = nullptr);

    Result<ScanResult, std::string> execute(const WifiInterface& interface) override;
    void cancel() override;
    std::string name() const override { return "forked"; }

private:
    std::shared_ptr<ProcessExecutor> executor_;
    std::shared_ptr<IwScanParser> parser_;
};

}

#endif
