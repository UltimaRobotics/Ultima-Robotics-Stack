

#include "../include/FileWatchdog.hpp"
#include "../include/LoggingMacros.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <chrono>
#include <thread>

// Constructor for raw content callback
FileWatchdog::FileWatchdog(ThreadMgr::ThreadManager& thread_manager,
                           const std::string& file_path, 
                           Callback callback, 
                           int poll_interval_ms)
    : thread_manager_(thread_manager)
    , file_path_(file_path)
    , callback_(callback)
    , json_callback_(nullptr)
    , use_json_callback_(false)
    , poll_interval_ms_(poll_interval_ms)
    , running_(false)
    , thread_id_(0)
    , last_modified_time_(0)
    , last_file_size_(0) {
}

// Constructor for JSON callback
FileWatchdog::FileWatchdog(ThreadMgr::ThreadManager& thread_manager,
                           const std::string& file_path, 
                           JsonCallback json_callback, 
                           int poll_interval_ms)
    : thread_manager_(thread_manager)
    , file_path_(file_path)
    , callback_(nullptr)
    , json_callback_(json_callback)
    , use_json_callback_(true)
    , poll_interval_ms_(poll_interval_ms)
    , running_(false)
    , thread_id_(0)
    , last_modified_time_(0)
    , last_file_size_(0) {
}

FileWatchdog::~FileWatchdog() {
    stop();
}

void FileWatchdog::start() {
    if (running_) {
        return;
    }
    
    running_ = true;
    last_modified_time_ = getLastModifiedTime(file_path_);
    last_file_size_ = getFileSize(file_path_);
    
    // Create thread using ThreadManager
    if (use_json_callback_) {
        thread_id_ = thread_manager_.createThread([this]() {
            this->watchLoopJson();
        });
    } else {
        thread_id_ = thread_manager_.createThread([this]() {
            this->watchLoopRaw();
        });
    }
    
    // Register thread with attachment for easy identification
    std::string attachment = "filewatchdog_" + file_path_;
    thread_manager_.registerThread(thread_id_, attachment);
    
    LOG_INFO("[FileWatchdog] Started watching file: " << file_path_ 
              << " (Thread ID: " << thread_id_ << ")" << std::endl);
}

void FileWatchdog::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    // Wait for thread to complete
    if (thread_id_ != 0) {
        thread_manager_.joinThread(thread_id_, std::chrono::seconds(5));
        
        // Unregister thread
        std::string attachment = "filewatchdog_" + file_path_;
        try {
            thread_manager_.unregisterThread(attachment);
        } catch (...) {
            // Ignore errors during cleanup
        }
        
        LOG_INFO("[FileWatchdog] Stopped watching file: " << file_path_ << std::endl);
    }
}

bool FileWatchdog::isRunning() const {
    return running_;
}

void FileWatchdog::watchLoopRaw() {
    while (running_) {
        time_t current_modified_time = getLastModifiedTime(file_path_);
        off_t current_file_size = getFileSize(file_path_);
        
        // Trigger callback if either timestamp OR file size changed
        if (current_file_size > 0 && 
            (current_modified_time > last_modified_time_ || current_file_size != last_file_size_)) {
            
            last_modified_time_ = current_modified_time;
            last_file_size_ = current_file_size;
            
            std::string content = readFileContent(file_path_);
            if (!content.empty() && callback_) {
                callback_(file_path_, content);
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms_));
    }
}

void FileWatchdog::watchLoopJson() {
    while (running_) {
        time_t current_modified_time = getLastModifiedTime(file_path_);
        off_t current_file_size = getFileSize(file_path_);
        
        // Trigger callback if either timestamp OR file size changed
        if (current_file_size > 0 && 
            (current_modified_time > last_modified_time_ || current_file_size != last_file_size_)) {
            
            last_modified_time_ = current_modified_time;
            last_file_size_ = current_file_size;
            
            std::string content = readFileContent(file_path_);
            if (!content.empty() && content.length() >= 10 && json_callback_) {
                try {
                    json parsed = json::parse(content);
                    json_callback_(parsed);
                } catch (const json::parse_error& e) {
                    // Silently ignore parse errors from incomplete JSON during writing
                } catch (const std::exception& e) {
                    LOG_ERROR("[FileWatchdog] Error processing JSON: " << e.what() << std::endl);
                }
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms_));
    }
}

time_t FileWatchdog::getLastModifiedTime(const std::string& file_path) {
    struct stat file_stat;
    if (stat(file_path.c_str(), &file_stat) == 0) {
        return file_stat.st_mtime;
    }
    return 0;
}

off_t FileWatchdog::getFileSize(const std::string& file_path) {
    struct stat file_stat;
    if (stat(file_path.c_str(), &file_stat) == 0) {
        return file_stat.st_size;
    }
    return 0;
}

std::string FileWatchdog::readFileContent(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

