#include "logger.h"

namespace UrWebManager {
    Logger* Logger::instance_ = nullptr;
    std::mutex Logger::mutex_;
}
