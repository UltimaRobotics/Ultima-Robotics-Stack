#ifndef URWT_COMMON_EXCEPTIONS_HPP
#define URWT_COMMON_EXCEPTIONS_HPP

#include <exception>
#include <string>
#include "../../json.hpp"

namespace urwt {

using json = nlohmann::json;

class WirelessToolsException : public std::exception {
public:
    explicit WirelessToolsException(const std::string& message)
        : message_(message) {}

    const char* what() const noexcept override {
        return message_.c_str();
    }

    json toJson() const {
        return json{
            {"error", message_},
            {"type", "WirelessToolsException"}
        };
    }

private:
    std::string message_;
};

class ScanException : public WirelessToolsException {
public:
    explicit ScanException(const std::string& message)
        : WirelessToolsException(message) {}

    json toJson() const {
        return json{
            {"error", what()},
            {"type", "ScanException"}
        };
    }
};

class InterfaceException : public WirelessToolsException {
public:
    explicit InterfaceException(const std::string& message)
        : WirelessToolsException(message) {}

    json toJson() const {
        return json{
            {"error", what()},
            {"type", "InterfaceException"}
        };
    }
};

class ConnectionException : public WirelessToolsException {
public:
    explicit ConnectionException(const std::string& message)
        : WirelessToolsException(message) {}

    json toJson() const {
        return json{
            {"error", what()},
            {"type", "ConnectionException"}
        };
    }
};

}

#endif
