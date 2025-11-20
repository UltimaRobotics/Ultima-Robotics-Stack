#pragma once

#include <string>
#include <map>
#include <filesystem>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

using FileChangeCallback = std::function<void(const std::string&)>;

class FileWatchdog {
public:
    explicit FileWatchdog(bool verbose = false);
    ~FileWatchdog();

    void add_watch(const std::string& file_path, FileChangeCallback callback);
    void remove_watch(const std::string& file_path);
    
    void start(int interval_seconds = 5);
    void stop();
    
    bool is_running() const { return running_; }
    
    void check_files_once();
    
private:
    bool verbose_;
    std::atomic<bool> running_;
    std::thread watch_thread_;
    std::mutex mutex_;
    
    struct WatchedFile {
        std::string path;
        std::filesystem::file_time_type last_write_time;
        FileChangeCallback callback;
    };
    
    std::map<std::string, WatchedFile> watched_files_;
    
    void watch_loop(int interval_seconds);
    void log(const std::string& message) const;
};
