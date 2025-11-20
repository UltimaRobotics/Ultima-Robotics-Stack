#include "file_watchdog.hpp"
#include <iostream>
#include <unistd.h>

FileWatchdog::FileWatchdog(bool verbose) 
    : verbose_(verbose), running_(false) {
}

FileWatchdog::~FileWatchdog() {
    stop();
}

void FileWatchdog::add_watch(const std::string& file_path, FileChangeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        if (!std::filesystem::exists(file_path)) {
            log("Warning: File does not exist, cannot watch: " + file_path);
            return;
        }
        
        WatchedFile wf;
        wf.path = file_path;
        wf.last_write_time = std::filesystem::last_write_time(file_path);
        wf.callback = callback;
        
        watched_files_[file_path] = wf;
        log("Added watch for file: " + file_path);
        
    } catch (const std::exception& e) {
        std::cerr << "Error adding watch for '" << file_path << "': " << e.what() << std::endl;
    }
}

void FileWatchdog::remove_watch(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = watched_files_.find(file_path);
    if (it != watched_files_.end()) {
        watched_files_.erase(it);
        log("Removed watch for file: " + file_path);
    }
}

void FileWatchdog::start(int interval_seconds) {
    if (running_) {
        log("Watchdog is already running");
        return;
    }
    
    running_ = true;
    watch_thread_ = std::thread(&FileWatchdog::watch_loop, this, interval_seconds);
    log("File watchdog started with " + std::to_string(interval_seconds) + "s interval");
}

void FileWatchdog::stop() {
    if (!running_) {
        return;
    }
    
    log("Stopping file watchdog...");
    running_ = false;
    
    if (watch_thread_.joinable()) {
        watch_thread_.join();
    }
    
    log("File watchdog stopped");
}

void FileWatchdog::check_files_once() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& [path, wf] : watched_files_) {
        try {
            if (!std::filesystem::exists(wf.path)) {
                log("Warning: Watched file no longer exists: " + wf.path);
                continue;
            }
            
            auto current_write_time = std::filesystem::last_write_time(wf.path);
            
            if (current_write_time != wf.last_write_time) {
                log("File changed: " + wf.path);
                wf.last_write_time = current_write_time;
                
                if (wf.callback) {
                    wf.callback(wf.path);
                }
            }
            
        } catch (const std::exception& e) {
            std::cerr << "Error checking file '" << wf.path << "': " << e.what() << std::endl;
        }
    }
}

void FileWatchdog::watch_loop(int interval_seconds) {
    log("Watch loop started");
    
    while (running_) {
        check_files_once();
        
        for (int i = 0; i < interval_seconds && running_; ++i) {
            sleep(1);  // POSIX sleep for 1 second
        }
    }
    
    log("Watch loop exited");
}

void FileWatchdog::log(const std::string& message) const {
    if (verbose_) {
        std::cout << "[FileWatchdog] " << message << std::endl;
    }
}
