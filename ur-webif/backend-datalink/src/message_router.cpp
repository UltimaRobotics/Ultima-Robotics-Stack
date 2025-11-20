#include "../include/message_router.hpp"
#include <algorithm>
#include <regex>
#include <string>
#include "../thirdparty/controlled-log/include/controlled_log.h"

namespace BackendDatalink {

MessageRouter::MessageRouter() {
    logInfo("MessageRouter created");
}

void MessageRouter::registerHandler(const std::string& topic, MessageHandler handler) {
    std::lock_guard<std::mutex> lock(handlersMutex_);
    
    exactHandlers_[topic] = std::move(handler);
    logInfo("Registered handler for topic: " + topic);
}

void MessageRouter::registerPatternHandler(const std::string& pattern, MessageHandler handler) {
    std::lock_guard<std::mutex> lock(handlersMutex_);
    
    patternHandlers_.emplace_back(pattern, std::move(handler));
    logInfo("Registered pattern handler for pattern: " + pattern);
}

void MessageRouter::routeMessage(const std::string& topic, const std::string& payload) {
    std::lock_guard<std::mutex> lock(handlersMutex_);
    
    // First try exact match
    auto it = exactHandlers_.find(topic);
    if (it != exactHandlers_.end()) {
        try {
            logDebug("Routing message to exact handler for topic: " + topic);
            it->second(topic, payload);
            return;
        } catch (const std::exception& e) {
            logError("Exception in exact handler for topic " + topic + ": " + std::string(e.what()));
            return;
        }
    }
    
    // Then try pattern matches
    for (const auto& patternHandler : patternHandlers_) {
        if (matchesPattern(topic, patternHandler.first)) {
            try {
                logDebug("Routing message to pattern handler for topic: " + topic + 
                        " (pattern: " + patternHandler.first + ")");
                patternHandler.second(topic, payload);
                return;
            } catch (const std::exception& e) {
                logError("Exception in pattern handler for topic " + topic + ": " + std::string(e.what()));
                return;
            }
        }
    }
    
    // No handler found
    logDebug("No handler found for topic: " + topic);
}

void MessageRouter::removeHandler(const std::string& topic) {
    std::lock_guard<std::mutex> lock(handlersMutex_);
    
    auto it = exactHandlers_.find(topic);
    if (it != exactHandlers_.end()) {
        exactHandlers_.erase(it);
        logInfo("Removed handler for topic: " + topic);
        return;
    }
    
    // Remove from pattern handlers
    patternHandlers_.erase(
        std::remove_if(patternHandlers_.begin(), patternHandlers_.end(),
            [&topic](const auto& patternHandler) {
                return patternHandler.first == topic;
            }),
        patternHandlers_.end()
    );
    
    logInfo("Removed pattern handler for pattern: " + topic);
}

size_t MessageRouter::getHandlerCount() const {
    std::lock_guard<std::mutex> lock(handlersMutex_);
    return exactHandlers_.size() + patternHandlers_.size();
}

std::vector<std::string> MessageRouter::getRegisteredTopics() const {
    std::lock_guard<std::mutex> lock(handlersMutex_);
    
    std::vector<std::string> topics;
    
    // Add exact topics
    for (const auto& handler : exactHandlers_) {
        topics.push_back(handler.first);
    }
    
    // Add pattern topics
    for (const auto& patternHandler : patternHandlers_) {
        topics.push_back(patternHandler.first);
    }
    
    return topics;
}

void MessageRouter::clearHandlers() {
    std::lock_guard<std::mutex> lock(handlersMutex_);
    
    exactHandlers_.clear();
    patternHandlers_.clear();
    
    logInfo("Cleared all handlers");
}

bool MessageRouter::matchesPattern(const std::string& topic, const std::string& pattern) const {
    try {
        std::string regexPattern = patternToRegex(pattern);
        std::regex regex(regexPattern);
        return std::regex_match(topic, regex);
    } catch (const std::exception& e) {
        logError("Pattern matching error for pattern '" + pattern + "': " + std::string(e.what()));
        return false;
    }
}

std::string MessageRouter::patternToRegex(const std::string& pattern) const {
    std::string regex;
    
    // Escape special regex characters and convert wildcards
    for (char c : pattern) {
        switch (c) {
            case '*':
                regex += ".*";
                break;
            case '?':
                regex += ".";
                break;
            case '.':
            case '^':
            case '$':
            case '+':
            case '(':
            case ')':
            case '[':
            case ']':
            case '{':
            case '}':
            case '|':
            case '\\':
                regex += "\\";
                regex += c;
                break;
            default:
                regex += c;
                break;
        }
    }
    
    // Add start and end anchors
    return "^" + regex + "$";
}

void MessageRouter::logDebug(const std::string& message) const {
    LOG_RPC_DEBUG("[MessageRouter] " + message);
}

void MessageRouter::logError(const std::string& message) const {
    LOG_RPC_ERROR("[MessageRouter] " + message);
}

void MessageRouter::logInfo(const std::string& message) const {
    LOG_RPC_INFO("[MessageRouter] " + message);
}

} // namespace BackendDatalink
