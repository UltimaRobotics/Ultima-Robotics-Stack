#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace BackendDatalink {

/**
 * @brief Message router for handling different topic types
 * 
 * This class routes incoming RPC messages to appropriate handlers
 * based on topic patterns. It supports exact matches and pattern matching.
 */
class MessageRouter {
public:
    // Message handler function type
    using MessageHandler = std::function<void(const std::string& topic, const std::string& payload)>;
    
    /**
     * @brief Constructor
     */
    MessageRouter();
    
    /**
     * @brief Destructor
     */
    ~MessageRouter() = default;
    
    /**
     * @brief Register a handler for an exact topic match
     * @param topic The exact topic to match
     * @param handler The handler function
     */
    void registerHandler(const std::string& topic, MessageHandler handler);
    
    /**
     * @brief Register a handler for a topic pattern (supports wildcards)
     * @param pattern The topic pattern (supports * and ? wildcards)
     * @param handler The handler function
     */
    void registerPatternHandler(const std::string& pattern, MessageHandler handler);
    
    /**
     * @brief Route a message to the appropriate handler
     * @param topic The message topic
     * @param payload The message payload
     */
    void routeMessage(const std::string& topic, const std::string& payload);
    
    /**
     * @brief Remove a handler for a specific topic
     * @param topic The topic to remove handler for
     */
    void removeHandler(const std::string& topic);
    
    /**
     * @brief Get the number of registered handlers
     * @return Number of handlers
     */
    size_t getHandlerCount() const;
    
    /**
     * @brief Get list of all registered topics
     * @return Vector of topic strings
     */
    std::vector<std::string> getRegisteredTopics() const;
    
    /**
     * @brief Clear all handlers
     */
    void clearHandlers();
    
private:
    /**
     * @brief Check if a topic matches a pattern
     * @param topic The topic to check
     * @param pattern The pattern to match against
     * @return True if matches
     */
    bool matchesPattern(const std::string& topic, const std::string& pattern) const;
    
    /**
     * @brief Convert pattern to regex
     * @param pattern The pattern with wildcards
     * @return Regex string
     */
    std::string patternToRegex(const std::string& pattern) const;
    
    // Exact topic handlers
    std::unordered_map<std::string, MessageHandler> exactHandlers_;
    
    // Pattern handlers (with wildcards)
    std::vector<std::pair<std::string, MessageHandler>> patternHandlers_;
    
    // Thread safety
    mutable std::mutex handlersMutex_;
    
    // Logging
    void logDebug(const std::string& message) const;
    void logError(const std::string& message) const;
    void logInfo(const std::string& message) const;
};

} // namespace BackendDatalink
