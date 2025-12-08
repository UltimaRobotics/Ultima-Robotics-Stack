#ifndef URWT_STRATEGIES_SCAN_STRATEGY_HPP
#define URWT_STRATEGIES_SCAN_STRATEGY_HPP

#include <memory>
#include <chrono>
#include "../models/wifi_interface.hpp"
#include "../models/scan_result.hpp"
#include "../utils/result.hpp"

namespace urwt {

class ScanStrategy {
public:
    virtual ~ScanStrategy() = default;

    virtual Result<ScanResult, std::string> execute(const WifiInterface& interface) = 0;
    virtual void cancel() = 0;
    virtual std::string name() const = 0;

    void setTimeout(std::chrono::milliseconds timeout) {
        timeout_ = timeout;
    }

    std::chrono::milliseconds timeout() const {
        return timeout_;
    }

protected:
    std::chrono::milliseconds timeout_{30000};
    bool cancelled_{false};
};

}

#endif
