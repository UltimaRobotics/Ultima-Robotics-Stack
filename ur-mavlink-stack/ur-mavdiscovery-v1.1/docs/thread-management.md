
# Thread Management

## Overview

The ur-mavdiscovery project uses the ur-threadder-api library for robust thread management, providing thread creation, tracking, and lifecycle control.

## ThreadManager Integration

### Initialization

The DeviceManager creates a ThreadManager instance:

```cpp
threadManager_ = std::make_unique<ThreadMgr::ThreadManager>(20);
ThreadMgr::ThreadManager::setLogLevel(ThreadMgr::LogLevel::Info);
```

Capacity: 20 threads (configurable)

### Thread Types

The system manages three types of threads:

1. **Monitor Thread**: Single thread for udev event monitoring
2. **Verifier Threads**: One per device being verified
3. **Main Thread**: Application lifecycle (not managed by ThreadManager)

## Thread Lifecycle

### Creation

Threads are created with lambda functions:

```cpp
threadId_ = threadManager_.createThread([this]() {
    this->verifyThread();
});
```

Returns: Unique thread ID for tracking

### Registration

Threads are registered with identifiers:

```cpp
threadManager_.registerThread(threadId_, "device_monitor");
// or
threadManager_.registerThread(threadId_, devicePath);
```

Benefits:
- Named tracking
- Lookup by identifier
- Debug information

### Execution

Threads run independently:
- No blocking operations in main thread
- Each thread manages its own lifecycle
- Stop flags for graceful shutdown

### Termination

Graceful shutdown with timeout:

```cpp
bool completed = threadManager_.joinThread(threadId_, std::chrono::seconds(5));

if (!completed) {
    LOG_WARNING("Thread did not complete in time, stopping forcefully");
    threadManager_.stopThread(threadId_);
    threadManager_.joinThread(threadId_, std::chrono::seconds(2));
}
```

## Thread Safety

### Atomic Operations

Device state uses atomic types:

```cpp
std::atomic<DeviceState> state;
std::atomic<bool> running_;
std::atomic<bool> shouldStop_;
```

### Mutex Protection

Critical sections use mutex locks:

```cpp
std::lock_guard<std::mutex> lock(verifiersMutex_);
// Access shared verifiers_ map
```

### Thread-Safe Collections

The DeviceStateDB uses mutex-protected maps:

```cpp
std::lock_guard<std::mutex> lock(mutex_);
devices_[devicePath] = info;
```

## Monitoring

### Thread Count

Query active thread count:

```cpp
unsigned int threadCount = threadManager_->getThreadCount();
```

### Thread Status

Check if thread is alive:

```cpp
if (threadManager_.isThreadAlive(threadId_)) {
    // Thread is running
}
```

### Thread IDs

Get all active thread IDs:

```cpp
auto threadIds = threadManager_->getAllThreadIds();
for (auto id : threadIds) {
    LOG_DEBUG("Active thread ID: " + std::to_string(id));
}
```

## Cleanup Strategy

### Normal Shutdown

1. Set running flags to false
2. Stop monitor thread
3. Stop all verifier threads
4. Log remaining threads
5. Clean up ThreadManager

### Emergency Cleanup

If threads don't stop gracefully:
- Force stop after timeout
- Log warnings
- Continue cleanup

## Best Practices

1. **Always register threads**: Enables tracking
2. **Use timeouts**: Prevent indefinite waits
3. **Check thread status**: Before operations
4. **Clean unregistration**: Remove from manager on stop
5. **Log thread lifecycle**: Debug information
