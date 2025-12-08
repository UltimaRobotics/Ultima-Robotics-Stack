
#ifndef FILE_WATCHDOG_HPP
#define FILE_WATCHDOG_HPP

#include <string>
#include <functional>
#include <atomic>
#include <memory>
#include "../thirdparty/nlohmann/json.hpp"
#include "../thirdparty/ur-threadder-api/cpp/include/ThreadManager.hpp"

using json = nlohmann::json;

class FileWatchdog {
public:
    using Callback = std::function<void(const std::string& file_path, const std::string& content)>;
    using JsonCallback = std::function<void(const json& parsed_json)>;

    // Constructor for raw content callback
    FileWatchdog(ThreadMgr::ThreadManager& thread_manager, 
                 const std::string& file_path, 
                 Callback callback, 
                 int poll_interval_ms = 100);
    
    // Constructor for JSON callback
    FileWatchdog(ThreadMgr::ThreadManager& thread_manager,
                 const std::string& file_path, 
                 JsonCallback json_callback, 
                 int poll_interval_ms = 100);
    
    ~FileWatchdog();

    void start();
    void stop();
    bool isRunning() const;
    unsigned int getThreadId() const { return thread_id_; }

private:
    void watchLoopRaw();
    void watchLoopJson();
    time_t getLastModifiedTime(const std::string& file_path);
    off_t getFileSize(const std::string& file_path);
    std::string readFileContent(const std::string& file_path);

    ThreadMgr::ThreadManager& thread_manager_;
    std::string file_path_;
    Callback callback_;
    JsonCallback json_callback_;
    bool use_json_callback_;
    int poll_interval_ms_;
    std::atomic<bool> running_;
    unsigned int thread_id_;
    time_t last_modified_time_;
    off_t last_file_size_;
};

#endif // FILE_WATCHDOG_HPP
