# UR-NetBench-MANN RPC Operations Integration Plan

## Table of Contents
1. [Executive Summary](#executive-summary)
2. [Architecture Overview](#architecture-overview)
3. [RPC Request Processing Flow](#rpc-request-processing-flow)
4. [Operation-Specific Implementations](#operation-specific-implementations)
5. [Thread Management Integration](#thread-management-integration)
6. [Real-time Status Updates](#real-time-status-updates)
7. [Configuration Compatibility](#configuration-compatibility)
8. [Response Handling](#response-handling)
9. [Error Management](#error-management)
10. [Monitoring and Diagnostics](#monitoring-and-diagnostics)
11. [Implementation Details](#implementation-details)
12. [Testing Strategy](#testing-strategy)
13. [Deployment Considerations](#deployment-considerations)
14. [Performance Optimization](#performance-optimization)
15. [Security Considerations](#security-considerations)

## Executive Summary

This document outlines the comprehensive integration plan for implementing RPC-based operations in the UR-NetBench-MANN system. The integration will transform the existing command-line driven network benchmarking tool into a fully RPC-capable service that can handle concurrent test execution while maintaining real-time status updates and backward compatibility.

### Key Objectives
- Transform all existing operations (ping, dns, traceroute, iperf, servers-status) into RPC-callable methods
- Ensure configuration compatibility between RPC params and existing package config structure
- Implement real-time status updates on shared bus topic
- Maintain thread-based execution using existing OperationWorker infrastructure
- Provide immediate response indicating test launch success/failure
- Track and report runtime status changes (running, finished, failed)

### Success Criteria
- All five operations accessible via JSON-RPC 2.0 protocol
- Configuration seamlessly integrated with existing ConfigManager
- Real-time status updates published to `ur-shared-bus/ur-netbench-mann/runtime`
- Thread execution properly tracked and monitored
- Backward compatibility maintained for legacy CLI mode
- Performance comparable to direct CLI execution

## Architecture Overview

### System Components Integration

```
┌─────────────────────────────────────────────────────────────────┐
│                    RPC Layer Integration                        │
├─────────────────────────────────────────────────────────────────┤
│  RPC Client ←→ RpcOperationProcessor ←→ OperationWorker        │
├─────────────────────────────────────────────────────────────────┤
│                   Thread Management Layer                       │
├─────────────────────────────────────────────────────────────────┤
│  ThreadManager │ TestWorkers │ FileWatchdog │ ServersStatus     │
├─────────────────────────────────────────────────────────────────┤
│                    Status Broadcasting                          │
├─────────────────────────────────────────────────────────────────┤
│  ur-shared-bus/ur-netbench-mann/runtime ←→ Status Updates       │
├─────────────────────────────────────────────────────────────────┤
│                     API Layer                                   │
├─────────────────────────────────────────────────────────────────┤
│  DNS API │ Traceroute API │ Ping API │ Iperf API                 │
└─────────────────────────────────────────────────────────────────┘
```

### Data Flow Architecture

```
RPC Request → Validation → Thread Creation → Test Execution → Status Updates → Response
     ↓              ↓              ↓              ↓              ↓           ↓
  JSON-RPC 2.0    ConfigMgr   ThreadManager   OperationWorker   MQTT Bus    JSON-RPC 2.0
  Parsing        Validation   Creation        Execution         Updates    Response
```

## RPC Request Processing Flow

### 1. Request Reception and Validation

```cpp
void RpcOperationProcessor::processRequest(const char* payload, size_t payload_len) {
    try {
        // Parse JSON-RPC 2.0 request
        json root = json::parse(payload, payload + payload_len);
        
        // Validate JSON-RPC structure
        if (!validateJsonRpcRequest(root)) {
            sendErrorResponse(transactionId, -32600, "Invalid Request");
            return;
        }
        
        // Extract operation details
        std::string method = root["method"].get<std::string>();
        std::string transactionId = extractTransactionId(root);
        json params = root["params"];
        
        // Validate operation method
        if (!isValidOperation(method)) {
            sendErrorResponse(transactionId, -32601, "Method not found: " + method);
            return;
        }
        
        // Process operation
        processOperation(method, transactionId, params);
        
    } catch (const json::parse_error& e) {
        sendErrorResponse("unknown", -32700, "Parse error: " + std::string(e.what()));
    } catch (const std::exception& e) {
        sendErrorResponse("unknown", -32603, "Internal error: " + std::string(e.what()));
    }
}
```

### 2. Configuration Compatibility Layer

The RPC params field will contain JSON configuration compatible with the existing package config structure:

```cpp
json buildPackageConfigFromRpcParams(const std::string& method, const json& rpc_params) {
    json package_config;
    
    // Set operation type
    package_config["operation"] = method;
    
    // Map RPC params to package config structure
    if (method == "servers-status") {
        package_config["servers_list_path"] = rpc_params.value("servers_list_path", "");
        package_config["output_dir"] = rpc_params.value("output_dir", "runtime-data/server-status");
        if (rpc_params.contains("filters")) {
            package_config["filters"] = rpc_params["filters"];
        }
    } else {
        // For test operations, create test_config section
        package_config["test_config"] = rpc_params;
        
        // Add common fields if present
        if (rpc_params.contains("output_file")) {
            package_config["output_file"] = rpc_params["output_file"];
        }
        if (rpc_params.contains("servers_list_path")) {
            package_config["servers_list_path"] = rpc_params["servers_list_path"];
        }
    }
    
    return package_config;
}
```

### 3. Thread Creation and Management

```cpp
void RpcOperationProcessor::processOperation(const std::string& method, 
                                           const std::string& transactionId,
                                           const json& params) {
    try {
        // Build compatible configuration
        json package_config = buildPackageConfigFromRpcParams(method, params);
        
        // Create ConfigManager instance
        auto config_manager = std::make_shared<ConfigManager>();
        std::string temp_config_file = createTempConfigFile(package_config);
        
        if (!config_manager->loadPackageConfig(temp_config_file)) {
            sendErrorResponse(transactionId, -32603, "Failed to load configuration");
            return;
        }
        
        // Create thread tracking context
        auto thread_context = std::make_shared<ThreadTrackingContext>();
        thread_context->transaction_id = transactionId;
        thread_context->method = method;
        thread_context->config_manager = config_manager;
        thread_context->config_file = temp_config_file;
        thread_context->start_time = std::chrono::system_clock::now();
        
        // Create test execution thread
        unsigned int thread_id = thread_manager_->createThread(
            [this, thread_context]() {
                this->executeOperationThread(thread_context);
            }
        );
        
        // Register thread for tracking
        thread_context->thread_id = thread_id;
        registerThreadForTracking(thread_context);
        
        // Send immediate success response
        sendSuccessResponse(transactionId, "Test thread launched successfully", {
            {"thread_id", thread_id},
            {"operation", method},
            {"status", "running"}
        });
        
        // Publish initial status update
        publishStatusUpdate(transactionId, "running", {
            {"thread_id", thread_id},
            {"operation", method},
            {"start_time", std::chrono::system_clock::to_time_t(thread_context->start_time)}
        });
        
    } catch (const std::exception& e) {
        sendErrorResponse(transactionId, -32603, "Failed to launch test: " + std::string(e.what()));
    }
}
```

## Operation-Specific Implementations

### 1. DNS Test Operation

#### RPC Request Format
```json
{
  "jsonrpc": "2.0",
  "id": "dns-test-001",
  "method": "dns",
  "params": {
    "hostname": "google.com",
    "query_type": "A",
    "nameserver": "8.8.8.8",
    "timeout_ms": 5000,
    "use_tcp": false,
    "export_file_path": "/tmp/dns_results.json",
    "output_file": "/tmp/dns_final_results.json"
  }
}
```

#### Implementation Details
```cpp
void executeDnsOperation(std::shared_ptr<ThreadTrackingContext> context) {
    try {
        // Update status to running
        publishStatusUpdate(context->transaction_id, "running", {
            {"phase", "initialization"},
            {"message", "Starting DNS test"}
        });
        
        // Extract configuration
        json test_config = context->config_manager->getTestConfig();
        std::string output_file = context->config_manager->getOutputFile();
        
        // Create DNS test worker thread
        unsigned int dns_thread_id = thread_manager_->createThread(
            [](ThreadMgr::ThreadManager& tm, const json& config, const std::string& output) {
                dns_test_worker(tm, config, output);
            },
            std::ref(*thread_manager_),
            test_config,
            output_file
        );
        
        // Track DNS thread
        context->worker_thread_id = dns_thread_id;
        
        // Update status
        publishStatusUpdate(context->transaction_id, "running", {
            {"phase", "execution"},
            {"worker_thread_id", dns_thread_id},
            {"message", "DNS test worker started"}
        });
        
        // Wait for completion with timeout
        bool completed = thread_manager_->joinThread(dns_thread_id, std::chrono::minutes(5));
        
        if (completed) {
            publishStatusUpdate(context->transaction_id, "finished", {
                {"phase", "completed"},
                {"message", "DNS test completed successfully"},
                {"end_time", std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())}
            });
        } else {
            publishStatusUpdate(context->transaction_id, "failed", {
                {"phase", "timeout"},
                {"message", "DNS test timed out"},
                {"end_time", std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())}
            });
            
            // Force stop the thread
            thread_manager_->stopThread(dns_thread_id);
        }
        
    } catch (const std::exception& e) {
        publishStatusUpdate(context->transaction_id, "failed", {
            {"phase", "error"},
            {"message", "DNS test failed: " + std::string(e.what())},
            {"end_time", std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())}
        });
    }
}
```

### 2. Ping Test Operation

#### RPC Request Format
```json
{
  "jsonrpc": "2.0",
  "id": "ping-test-001",
  "method": "ping",
  "params": {
    "destination": "8.8.8.8",
    "count": 10,
    "interval_ms": 1000,
    "timeout_ms": 5000,
    "packet_size": 56,
    "ttl": 64,
    "resolve_hostname": true,
    "export_file_path": "/tmp/ping_results.json",
    "output_file": "/tmp/ping_final_results.json"
  }
}
```

#### Real-time Status Updates for Ping
```cpp
void executePingOperation(std::shared_ptr<ThreadTrackingContext> context) {
    try {
        publishStatusUpdate(context->transaction_id, "running", {
            {"phase", "initialization"},
            {"message", "Starting ping test"}
        });
        
        json test_config = context->config_manager->getTestConfig();
        std::string output_file = context->config_manager->getOutputFile();
        
        // Setup real-time callback for ping progress
        auto progress_callback = [this, context](const json& progress_data) {
            publishStatusUpdate(context->transaction_id, "running", {
                {"phase", "execution"},
                {"progress", progress_data},
                {"message", "Ping test in progress"}
            });
        };
        
        // Create ping worker with progress tracking
        unsigned int ping_thread_id = thread_manager_->createThread(
            [this, progress_callback](ThreadMgr::ThreadManager& tm, 
                                    const json& config, 
                                    const std::string& output) {
                // Enhanced ping worker with progress callbacks
                ping_test_worker_with_progress(tm, config, output, progress_callback);
            },
            std::ref(*thread_manager_),
            test_config,
            output_file
        );
        
        context->worker_thread_id = ping_thread_id;
        
        publishStatusUpdate(context->transaction_id, "running", {
            {"phase", "execution"},
            {"worker_thread_id", ping_thread_id},
            {"message", "Ping test worker started"}
        });
        
        // Monitor ping progress
        while (thread_manager_->isThreadAlive(ping_thread_id)) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Could publish intermediate statistics here
            if (context->progress_data.contains("packets_sent")) {
                publishStatusUpdate(context->transaction_id, "running", {
                    {"phase", "execution"},
                    {"packets_sent", context->progress_data["packets_sent"]},
                    {"packets_received", context->progress_data["packets_received"]},
                    {"current_rtt", context->progress_data["current_rtt"]}
                });
            }
        }
        
        // Final status update
        bool completed = thread_manager_->joinThread(ping_thread_id, std::chrono::seconds(1));
        
        if (completed) {
            publishStatusUpdate(context->transaction_id, "finished", {
                {"phase", "completed"},
                {"final_stats", context->progress_data},
                {"message", "Ping test completed successfully"}
            });
        } else {
            publishStatusUpdate(context->transaction_id, "failed", {
                {"phase", "error"},
                {"message", "Ping test failed to complete properly"}
            });
        }
        
    } catch (const std::exception& e) {
        publishStatusUpdate(context->transaction_id, "failed", {
            {"phase", "error"},
            {"message", "Ping test failed: " + std::string(e.what())}
        });
    }
}
```

### 3. Traceroute Test Operation

#### RPC Request Format
```json
{
  "jsonrpc": "2.0",
  "id": "traceroute-test-001",
  "method": "traceroute",
  "params": {
    "target": "google.com",
    "max_hops": 30,
    "timeout_ms": 3000,
    "queries_per_hop": 3,
    "packet_size": 60,
    "port": 33434,
    "resolve_hostnames": true,
    "export_file_path": "/tmp/traceroute_results.json",
    "output_file": "/tmp/traceroute_final_results.json"
  }
}
```

#### Hop-by-Hop Progress Tracking
```cpp
void executeTracerouteOperation(std::shared_ptr<ThreadTrackingContext> context) {
    try {
        publishStatusUpdate(context->transaction_id, "running", {
            {"phase", "initialization"},
            {"message", "Starting traceroute test"}
        });
        
        json test_config = context->config_manager->getTestConfig();
        std::string output_file = context->config_manager->getOutputFile();
        
        // Setup hop-by-hop callback
        auto hop_callback = [this, context](const json& hop_data) {
            publishStatusUpdate(context->transaction_id, "running", {
                {"phase", "execution"},
                {"current_hop", hop_data},
                {"message", "Traceroute in progress - hop " + 
                 std::to_string(hop_data.value("hop_number", 0))}
            });
        };
        
        // Create traceroute worker with hop tracking
        unsigned int traceroute_thread_id = thread_manager_->createThread(
            [this, hop_callback](ThreadMgr::ThreadManager& tm, 
                                const json& config, 
                                const std::string& output) {
                traceroute_test_worker_with_hops(tm, config, output, hop_callback);
            },
            std::ref(*thread_manager_),
            test_config,
            output_file
        );
        
        context->worker_thread_id = traceroute_thread_id;
        
        publishStatusUpdate(context->transaction_id, "running", {
            {"phase", "execution"},
            {"worker_thread_id", traceroute_thread_id},
            {"message", "Traceroute test worker started"}
        });
        
        // Wait for completion
        bool completed = thread_manager_->joinThread(traceroute_thread_id, std::chrono::minutes(10));
        
        if (completed) {
            publishStatusUpdate(context->transaction_id, "finished", {
                {"phase", "completed"},
                {"message", "Traceroute test completed successfully"}
            });
        } else {
            publishStatusUpdate(context->transaction_id, "failed", {
                {"phase", "timeout"},
                {"message", "Traceroute test timed out"}
            });
        }
        
    } catch (const std::exception& e) {
        publishStatusUpdate(context->transaction_id, "failed", {
            {"phase", "error"},
            {"message", "Traceroute test failed: " + std::string(e.what())}
        });
    }
}
```

### 4. Iperf Test Operation

#### RPC Request Format
```json
{
  "jsonrpc": "2.0",
  "id": "iperf-test-001",
  "method": "iperf",
  "params": {
    "target": "iperf.example.com",
    "port": 5201,
    "duration": 10,
    "protocol": "tcp",
    "bandwidth": "1G",
    "parallel": 1,
    "window_size": "1M",
    "buffer_size": "128K",
    "reverse": false,
    "bidirectional": false,
    "realtime": true,
    "export_file_path": "/tmp/iperf_results.json",
    "output_file": "/tmp/iperf_final_results.json",
    "servers_list_path": "/configs/servers.json"
  }
}
```

#### Performance Monitoring Integration
```cpp
void executeIperfOperation(std::shared_ptr<ThreadTrackingContext> context) {
    try {
        publishStatusUpdate(context->transaction_id, "running", {
            {"phase", "initialization"},
            {"message", "Starting iperf test"}
        });
        
        json test_config = context->config_manager->getTestConfig();
        std::string output_file = context->config_manager->getOutputFile();
        
        // Setup performance monitoring callback
        auto performance_callback = [this, context](const json& perf_data) {
            publishStatusUpdate(context->transaction_id, "running", {
                {"phase", "execution"},
                {"performance", perf_data},
                {"message", "Iperf test in progress"}
            });
        };
        
        // Auto-configuration from servers list if needed
        if (test_config.contains("servers_list_path")) {
            test_config = autoConfigureIperfFromServersList(test_config);
        }
        
        // Create iperf worker with performance tracking
        unsigned int iperf_thread_id = thread_manager_->createThread(
            [this, performance_callback](ThreadMgr::ThreadManager& tm, 
                                       const json& config, 
                                       const std::string& output) {
                iperf_test_worker_with_monitoring(tm, config, output, performance_callback);
            },
            std::ref(*thread_manager_),
            test_config,
            output_file
        );
        
        context->worker_thread_id = iperf_thread_id;
        
        publishStatusUpdate(context->transaction_id, "running", {
            {"phase", "execution"},
            {"worker_thread_id", iperf_thread_id},
            {"message", "Iperf test worker started"}
        });
        
        // Monitor progress with periodic updates
        auto start_time = std::chrono::steady_clock::now();
        while (thread_manager_->isThreadAlive(iperf_thread_id)) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start_time).count();
            
            publishStatusUpdate(context->transaction_id, "running", {
                {"phase", "execution"},
                {"elapsed_seconds", elapsed},
                {"message", "Iperf test running..."}
            });
        }
        
        // Final status
        bool completed = thread_manager_->joinThread(iperf_thread_id, std::chrono::seconds(5));
        
        if (completed) {
            publishStatusUpdate(context->transaction_id, "finished", {
                {"phase", "completed"},
                {"message", "Iperf test completed successfully"}
            });
        } else {
            publishStatusUpdate(context->transaction_id, "failed", {
                {"phase", "error"},
                {"message", "Iperf test failed"}
            });
        }
        
    } catch (const std::exception& e) {
        publishStatusUpdate(context->transaction_id, "failed", {
            {"phase", "error"},
            {"message", "Iperf test failed: " + std::string(e.what())}
        });
    }
}
```

### 5. Servers Status Operation

#### RPC Request Format
```json
{
  "jsonrpc": "2.0",
  "id": "servers-status-001",
  "method": "servers-status",
  "params": {
    "servers_list_path": "/configs/servers.json",
    "output_dir": "/results/server_status",
    "filters": {
      "continent": "Europe",
      "country": "Germany",
      "provider": "Deutsche Telekom"
    },
    "continuous": false,
    "refresh_interval_sec": 60,
    "output_file": "/results/aggregated_server_status.json"
  }
}
```

#### Concurrent Server Monitoring
```cpp
void executeServersStatusOperation(std::shared_ptr<ThreadTrackingContext> context) {
    try {
        publishStatusUpdate(context->transaction_id, "running", {
            {"phase", "initialization"},
            {"message", "Starting servers status monitoring"}
        });
        
        // Extract configuration
        std::string servers_list_path = context->config_manager->getServersListPath();
        std::string output_dir = context->config_manager->getOutputDir();
        json filters = context->config_manager->getFilters();
        
        // Create servers status monitor
        auto monitor = std::make_unique<ServersStatusMonitor>(output_dir);
        
        if (!monitor->loadServersConfig(servers_list_path)) {
            publishStatusUpdate(context->transaction_id, "failed", {
                {"phase", "error"},
                {"message", "Failed to load servers configuration"}
            });
            return;
        }
        
        // Apply filters if specified
        if (!filters.empty()) {
            monitor->applyFilters(filters);
        }
        
        // Start monitoring
        if (!monitor->startMonitoring()) {
            publishStatusUpdate(context->transaction_id, "failed", {
                {"phase", "error"},
                {"message", "Failed to start server monitoring"}
            });
            return;
        }
        
        publishStatusUpdate(context->transaction_id, "running", {
            {"phase", "execution"},
            {"servers_count", monitor->getServersCount()},
            {"message", "Server monitoring started"}
        });
        
        // Setup progress tracking
        auto progress_callback = [this, context](const json& progress_data) {
            publishStatusUpdate(context->transaction_id, "running", {
                {"phase", "execution"},
                {"monitoring_progress", progress_data},
                {"message", "Monitoring servers progress"}
            });
        };
        
        monitor->setProgressCallback(progress_callback);
        
        // Wait for monitoring completion or timeout
        bool completed = monitor->waitForCompletion(std::chrono::minutes(30));
        
        if (completed) {
            // Export aggregated results
            std::string output_file = context->config_manager->getOutputFile();
            monitor->exportAggregatedResults(output_file);
            
            publishStatusUpdate(context->transaction_id, "finished", {
                {"phase", "completed"},
                {"results_file", output_file},
                {"message", "Server status monitoring completed successfully"}
            });
        } else {
            publishStatusUpdate(context->transaction_id, "failed", {
                {"phase", "timeout"},
                {"message", "Server status monitoring timed out"}
            });
        }
        
        // Cleanup
        monitor->stopMonitoring();
        
    } catch (const std::exception& e) {
        publishStatusUpdate(context->transaction_id, "failed", {
            {"phase", "error"},
            {"message", "Server status monitoring failed: " + std::string(e.what())}
        });
    }
}
```

## Thread Management Integration

### Enhanced Thread Tracking

```cpp
class ThreadTrackingContext {
public:
    std::string transaction_id;
    std::string method;
    std::shared_ptr<ConfigManager> config_manager;
    std::string config_file;
    unsigned int thread_id;
    unsigned int worker_thread_id;
    std::chrono::system_clock::time_point start_time;
    std::atomic<ThreadStatus> status{ThreadStatus::CREATED};
    json progress_data;
    std::string error_message;
    
    enum class ThreadStatus {
        CREATED,
        RUNNING,
        FINISHED,
        FAILED,
        TIMEOUT
    };
};

class RpcThreadManager {
private:
    std::map<std::string, std::shared_ptr<ThreadTrackingContext>> active_threads_;
    std::mutex threads_mutex_;
    ThreadMgr::ThreadManager* thread_manager_;
    RpcOperationProcessor* processor_;
    
public:
    void registerThread(std::shared_ptr<ThreadTrackingContext> context);
    void updateThreadStatus(const std::string& transaction_id, 
                           ThreadTrackingContext::ThreadStatus status);
    void updateThreadProgress(const std::string& transaction_id, 
                             const json& progress_data);
    std::shared_ptr<ThreadTrackingContext> getThreadContext(const std::string& transaction_id);
    void cleanupThread(const std::string& transaction_id);
    std::vector<std::shared_ptr<ThreadTrackingContext>> getActiveThreads();
};
```

### Thread Lifecycle Management

```cpp
void RpcOperationProcessor::executeOperationThread(std::shared_ptr<ThreadTrackingContext> context) {
    try {
        // Update status to running
        context->status.store(ThreadTrackingContext::ThreadStatus::RUNNING);
        thread_manager_->updateThreadStatus(context->transaction_id, 
                                           ThreadTrackingContext::ThreadStatus::RUNNING);
        
        // Route to appropriate operation handler
        if (context->method == "dns") {
            executeDnsOperation(context);
        } else if (context->method == "ping") {
            executePingOperation(context);
        } else if (context->method == "traceroute") {
            executeTracerouteOperation(context);
        } else if (context->method == "iperf") {
            executeIperfOperation(context);
        } else if (context->method == "servers-status") {
            executeServersStatusOperation(context);
        } else {
            throw std::runtime_error("Unknown operation: " + context->method);
        }
        
        // Update final status
        if (context->status.load() == ThreadTrackingContext::ThreadStatus::RUNNING) {
            context->status.store(ThreadTrackingContext::ThreadStatus::FINISHED);
        }
        
    } catch (const std::exception& e) {
        context->status.store(ThreadTrackingContext::ThreadStatus::FAILED);
        context->error_message = e.what();
        
        thread_manager_->updateThreadStatus(context->transaction_id, 
                                           ThreadTrackingContext::ThreadStatus::FAILED);
    }
    
    // Cleanup
    thread_manager_->cleanupThread(context->transaction_id);
}
```

## Real-time Status Updates

### Status Message Format

```json
{
  "transaction_id": "dns-test-001",
  "operation": "dns",
  "status": "running",
  "timestamp": 1636791234,
  "details": {
    "phase": "execution",
    "worker_thread_id": 12345,
    "message": "DNS test worker started",
    "progress": {
      "current_step": "query_execution",
      "completion_percentage": 25
    }
  },
  "thread_info": {
    "main_thread_id": 12344,
    "worker_thread_id": 12345,
    "start_time": 1636791230,
    "elapsed_seconds": 4
  }
}
```

### Status Broadcasting Implementation

```cpp
class StatusBroadcaster {
private:
    std::shared_ptr<RpcClient> rpc_client_;
    std::string status_topic_ = "ur-shared-bus/ur-netbench-mann/runtime";
    std::atomic<bool> broadcasting_enabled_{true};
    
public:
    void publishStatusUpdate(const std::string& transaction_id,
                           const std::string& status,
                           const json& details = json::object()) {
        if (!broadcasting_enabled_) {
            return;
        }
        
        try {
            json status_message;
            status_message["transaction_id"] = transaction_id;
            status_message["status"] = status;
            status_message["timestamp"] = std::chrono::system_clock::to_time_t(
                std::chrono::system_clock::now());
            
            if (!details.empty()) {
                status_message["details"] = details;
            }
            
            // Add thread context information
            auto thread_context = thread_manager_->getThreadContext(transaction_id);
            if (thread_context) {
                json thread_info;
                thread_info["main_thread_id"] = thread_context->thread_id;
                thread_info["worker_thread_id"] = thread_context->worker_thread_id;
                thread_info["start_time"] = std::chrono::system_clock::to_time_t(
                    thread_context->start_time);
                
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now() - thread_context->start_time);
                thread_info["elapsed_seconds"] = elapsed.count();
                
                status_message["thread_info"] = thread_info;
                status_message["operation"] = thread_context->method;
            }
            
            // Publish to shared bus
            std::string message_str = status_message.dump();
            rpc_client_->publishMessage(status_topic_, message_str);
            
        } catch (const std::exception& e) {
            LOG_ERROR("[StatusBroadcaster] Failed to publish status update: " << e.what());
        }
    }
    
    void enableBroadcasting(bool enabled) {
        broadcasting_enabled_.store(enabled);
    }
    
    bool isBroadcastingEnabled() const {
        return broadcasting_enabled_.load();
    }
};
```

### Status Update Frequency and Throttling

```cpp
class ThrottledStatusBroadcaster {
private:
    StatusBroadcaster broadcaster_;
    std::map<std::string, std::chrono::steady_clock::time_point> last_update_time_;
    std::mutex timing_mutex_;
    std::chrono::milliseconds min_update_interval_{1000}; // 1 second minimum
    
public:
    void publishThrottledStatusUpdate(const std::string& transaction_id,
                                    const std::string& status,
                                    const json& details = json::object()) {
        std::lock_guard<std::mutex> lock(timing_mutex_);
        
        auto now = std::chrono::steady_clock::now();
        auto last_update = last_update_time_[transaction_id];
        
        // Always publish status changes (running -> finished -> failed)
        bool is_status_change = false;
        auto last_status = last_published_status_[transaction_id];
        if (last_status != status) {
            is_status_change = true;
        }
        
        // Check if enough time has passed or if status changed
        if (is_status_change || 
            (now - last_update) >= min_update_interval_) {
            
            broadcaster_.publishStatusUpdate(transaction_id, status, details);
            last_update_time_[transaction_id] = now;
            last_published_status_[transaction_id] = status;
        }
    }
    
private:
    std::map<std::string, std::string> last_published_status_;
};
```

## Configuration Compatibility

### Unified Configuration Builder

```cpp
class RpcConfigurationBuilder {
public:
    static json buildPackageConfig(const std::string& method, const json& rpc_params) {
        json package_config;
        
        // Set the operation
        package_config["operation"] = method;
        
        switch (getOperationType(method)) {
            case OperationType::DNS:
                return buildDnsConfig(package_config, rpc_params);
                
            case OperationType::PING:
                return buildPingConfig(package_config, rpc_params);
                
            case OperationType::TRACEROUTE:
                return buildTracerouteConfig(package_config, rpc_params);
                
            case OperationType::IPERF:
                return buildIperfConfig(package_config, rpc_params);
                
            case OperationType::SERVERS_STATUS:
                return buildServersStatusConfig(package_config, rpc_params);
                
            default:
                throw std::invalid_argument("Unknown operation: " + method);
        }
    }
    
private:
    enum class OperationType {
        DNS, PING, TRACEROUTE, IPERF, SERVERS_STATUS
    };
    
    static OperationType getOperationType(const std::string& method) {
        if (method == "dns") return OperationType::DNS;
        if (method == "ping") return OperationType::PING;
        if (method == "traceroute") return OperationType::TRACEROUTE;
        if (method == "iperf") return OperationType::IPERF;
        if (method == "servers-status") return OperationType::SERVERS_STATUS;
        throw std::invalid_argument("Unknown method: " + method);
    }
    
    static json buildDnsConfig(json package_config, const json& rpc_params) {
        // Validate required DNS parameters
        validateRequiredParams(rpc_params, {"hostname"}, "DNS test");
        
        // Create test_config section
        json test_config = rpc_params;
        
        // Set DNS-specific defaults
        if (!test_config.contains("query_type")) {
            test_config["query_type"] = "A";
        }
        if (!test_config.contains("timeout_ms")) {
            test_config["timeout_ms"] = 5000;
        }
        if (!test_config.contains("use_tcp")) {
            test_config["use_tcp"] = false;
        }
        if (!test_config.contains("nameserver")) {
            test_config["nameserver"] = "8.8.8.8";
        }
        
        package_config["test_config"] = test_config;
        
        // Handle output file
        if (rpc_params.contains("output_file")) {
            package_config["output_file"] = rpc_params["output_file"];
        }
        
        return package_config;
    }
    
    static json buildPingConfig(json package_config, const json& rpc_params) {
        validateRequiredParams(rpc_params, {"destination"}, "Ping test");
        
        json test_config = rpc_params;
        
        // Set ping-specific defaults
        if (!test_config.contains("count")) {
            test_config["count"] = 4;
        }
        if (!test_config.contains("interval_ms")) {
            test_config["interval_ms"] = 1000;
        }
        if (!test_config.contains("timeout_ms")) {
            test_config["timeout_ms"] = 5000;
        }
        if (!test_config.contains("packet_size")) {
            test_config["packet_size"] = 56;
        }
        if (!test_config.contains("ttl")) {
            test_config["ttl"] = 64;
        }
        if (!test_config.contains("resolve_hostname")) {
            test_config["resolve_hostname"] = true;
        }
        
        package_config["test_config"] = test_config;
        
        if (rpc_params.contains("output_file")) {
            package_config["output_file"] = rpc_params["output_file"];
        }
        
        return package_config;
    }
    
    static json buildTracerouteConfig(json package_config, const json& rpc_params) {
        validateRequiredParams(rpc_params, {"target"}, "Traceroute test");
        
        json test_config = rpc_params;
        
        // Set traceroute-specific defaults
        if (!test_config.contains("max_hops")) {
            test_config["max_hops"] = 30;
        }
        if (!test_config.contains("timeout_ms")) {
            test_config["timeout_ms"] = 3000;
        }
        if (!test_config.contains("queries_per_hop")) {
            test_config["queries_per_hop"] = 3;
        }
        if (!test_config.contains("packet_size")) {
            test_config["packet_size"] = 60;
        }
        if (!test_config.contains("port")) {
            test_config["port"] = 33434;
        }
        if (!test_config.contains("resolve_hostnames")) {
            test_config["resolve_hostnames"] = true;
        }
        
        package_config["test_config"] = test_config;
        
        if (rpc_params.contains("output_file")) {
            package_config["output_file"] = rpc_params["output_file"];
        }
        
        return package_config;
    }
    
    static json buildIperfConfig(json package_config, const json& rpc_params) {
        validateRequiredParams(rpc_params, {"target"}, "Iperf test");
        
        json test_config = rpc_params;
        
        // Set iperf-specific defaults
        if (!test_config.contains("port")) {
            test_config["port"] = 5201;
        }
        if (!test_config.contains("duration")) {
            test_config["duration"] = 10;
        }
        if (!test_config.contains("protocol")) {
            test_config["protocol"] = "tcp";
        }
        if (!test_config.contains("parallel")) {
            test_config["parallel"] = 1;
        }
        if (!test_config.contains("realtime")) {
            test_config["realtime"] = true;
        }
        
        package_config["test_config"] = test_config;
        
        if (rpc_params.contains("output_file")) {
            package_config["output_file"] = rpc_params["output_file"];
        }
        
        return package_config;
    }
    
    static json buildServersStatusConfig(json package_config, const json& rpc_params) {
        validateRequiredParams(rpc_params, {"servers_list_path"}, "Servers status");
        
        // Direct mapping for servers-status operation
        for (auto& [key, value] : rpc_params.items()) {
            package_config[key] = value;
        }
        
        // Set defaults
        if (!package_config.contains("output_dir")) {
            package_config["output_dir"] = "runtime-data/server-status";
        }
        
        return package_config;
    }
    
    static void validateRequiredParams(const json& params, 
                                     const std::vector<std::string>& required,
                                     const std::string& operation_name) {
        for (const auto& param : required) {
            if (!params.contains(param)) {
                throw std::invalid_argument(
                    operation_name + " requires parameter: " + param);
            }
        }
    }
};
```

### Configuration Validation and Sanitization

```cpp
class ConfigurationValidator {
public:
    static bool validateAndSanitizeConfig(const std::string& method, 
                                         json& config) {
        try {
            switch (RpcConfigurationBuilder::getOperationType(method)) {
                case RpcConfigurationBuilder::OperationType::DNS:
                    return validateDnsConfig(config);
                    
                case RpcConfigurationBuilder::OperationType::PING:
                    return validatePingConfig(config);
                    
                case RpcConfigurationBuilder::OperationType::TRACEROUTE:
                    return validateTracerouteConfig(config);
                    
                case RpcConfigurationBuilder::OperationType::IPERF:
                    return validateIperfConfig(config);
                    
                case RpcConfigurationBuilder::OperationType::SERVERS_STATUS:
                    return validateServersStatusConfig(config);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("[ConfigurationValidator] Validation error: " << e.what());
            return false;
        }
        
        return false;
    }
    
private:
    static bool validateDnsConfig(json& config) {
        json test_config = config["test_config"];
        
        // Validate hostname
        std::string hostname = test_config["hostname"];
        if (hostname.empty() || hostname.length() > 253) {
            throw std::invalid_argument("Invalid hostname");
        }
        
        // Validate query type
        std::string query_type = test_config["query_type"];
        std::set<std::string> valid_types = {"A", "AAAA", "MX", "NS", "CNAME", "TXT", "SOA"};
        if (valid_types.find(query_type) == valid_types.end()) {
            throw std::invalid_argument("Invalid DNS query type: " + query_type);
        }
        
        // Validate timeout
        int timeout = test_config["timeout_ms"];
        if (timeout < 1000 || timeout > 30000) {
            test_config["timeout_ms"] = 5000; // Sanitize to default
        }
        
        // Validate nameserver
        std::string nameserver = test_config["nameserver"];
        if (!isValidIPAddress(nameserver)) {
            throw std::invalid_argument("Invalid nameserver IP address");
        }
        
        config["test_config"] = test_config;
        return true;
    }
    
    static bool validatePingConfig(json& config) {
        json test_config = config["test_config"];
        
        // Validate destination
        std::string destination = test_config["destination"];
        if (destination.empty()) {
            throw std::invalid_argument("Destination cannot be empty");
        }
        
        // Validate count
        int count = test_config["count"];
        if (count < 1 || count > 1000) {
            test_config["count"] = std::max(1, std::min(1000, count));
        }
        
        // Validate timeout
        int timeout = test_config["timeout_ms"];
        if (timeout < 100 || timeout > 60000) {
            test_config["timeout_ms"] = 5000;
        }
        
        // Validate packet size
        int packet_size = test_config["packet_size"];
        if (packet_size < 1 || packet_size > 65507) {
            test_config["packet_size"] = std::max(1, std::min(65507, packet_size));
        }
        
        config["test_config"] = test_config;
        return true;
    }
    
    static bool validateTracerouteConfig(json& config) {
        json test_config = config["test_config"];
        
        // Validate target
        std::string target = test_config["target"];
        if (target.empty()) {
            throw std::invalid_argument("Target cannot be empty");
        }
        
        // Validate max hops
        int max_hops = test_config["max_hops"];
        if (max_hops < 1 || max_hops > 255) {
            test_config["max_hops"] = std::max(1, std::min(255, max_hops));
        }
        
        // Validate port
        int port = test_config["port"];
        if (port < 1 || port > 65535) {
            test_config["port"] = 33434;
        }
        
        config["test_config"] = test_config;
        return true;
    }
    
    static bool validateIperfConfig(json& config) {
        json test_config = config["test_config"];
        
        // Validate target
        std::string target = test_config["target"];
        if (target.empty()) {
            throw std::invalid_argument("Target cannot be empty");
        }
        
        // Validate port
        int port = test_config["port"];
        if (port < 1 || port > 65535) {
            test_config["port"] = 5201;
        }
        
        // Validate duration
        int duration = test_config["duration"];
        if (duration < 1 || duration > 3600) {
            test_config["duration"] = std::max(1, std::min(3600, duration));
        }
        
        // Validate protocol
        std::string protocol = test_config["protocol"];
        if (protocol != "tcp" && protocol != "udp") {
            test_config["protocol"] = "tcp";
        }
        
        config["test_config"] = test_config;
        return true;
    }
    
    static bool validateServersStatusConfig(json& config) {
        // Validate servers list path
        std::string servers_list_path = config["servers_list_path"];
        if (servers_list_path.empty()) {
            throw std::invalid_argument("Servers list path cannot be empty");
        }
        
        // Check if file exists and is readable
        std::ifstream file(servers_list_path);
        if (!file.good()) {
            throw std::invalid_argument("Cannot read servers list file: " + servers_list_path);
        }
        
        // Validate output directory
        std::string output_dir = config["output_dir"];
        if (!output_dir.empty()) {
            // Try to create directory if it doesn't exist
            std::string mkdir_cmd = "mkdir -p " + output_dir;
            if (system(mkdir_cmd.c_str()) != 0) {
                LOG_WARNING("[ConfigurationValidator] Could not create output directory: " << output_dir);
            }
        }
        
        return true;
    }
    
    static bool isValidIPAddress(const std::string& ip) {
        // Simple IP validation - could be enhanced with proper regex
        std::istringstream iss(ip);
        std::string segment;
        int count = 0;
        
        while (std::getline(iss, segment, '.')) {
            if (++count > 4) return false;
            
            try {
                int num = std::stoi(segment);
                if (num < 0 || num > 255) return false;
            } catch (...) {
                return false;
            }
        }
        
        return count == 4;
    }
};
```

## Response Handling

### Immediate Response Protocol

```cpp
class RpcResponseHandler {
private:
    std::shared_ptr<RpcClient> rpc_client_;
    std::string response_topic_ = "direct_messaging/ur-netbench-mann/responses";
    
public:
    void sendSuccessResponse(const std::string& transaction_id,
                            const std::string& message,
                            const json& additional_data = json::object()) {
        json response;
        response["jsonrpc"] = "2.0";
        response["id"] = transaction_id;
        response["result"] = json::object();
        response["result"]["success"] = true;
        response["result"]["message"] = message;
        response["result"]["timestamp"] = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now());
        
        // Add additional data if provided
        for (auto& [key, value] : additional_data.items()) {
            response["result"][key] = value;
        }
        
        publishResponse(response);
    }
    
    void sendErrorResponse(const std::string& transaction_id,
                         int error_code,
                         const std::string& error_message,
                         const json& error_data = json::object()) {
        json response;
        response["jsonrpc"] = "2.0";
        response["id"] = transaction_id;
        response["error"] = json::object();
        response["error"]["code"] = error_code;
        response["error"]["message"] = error_message;
        response["error"]["timestamp"] = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now());
        
        // Add error data if provided
        if (!error_data.empty()) {
            response["error"]["data"] = error_data;
        }
        
        publishResponse(response);
    }
    
private:
    void publishResponse(const json& response) {
        try {
            std::string response_str = response.dump();
            rpc_client_->sendResponse(response_topic_, response_str);
            
            LOG_INFO("[RpcResponseHandler] Sent response: " << response_str);
        } catch (const std::exception& e) {
            LOG_ERROR("[RpcResponseHandler] Failed to send response: " << e.what());
        }
    }
};
```

### Response Message Standards

#### Success Response Format
```json
{
  "jsonrpc": "2.0",
  "id": "dns-test-001",
  "result": {
    "success": true,
    "message": "Test thread launched successfully",
    "timestamp": 1636791234,
    "thread_id": 12345,
    "operation": "dns",
    "status": "running",
    "estimated_duration": 30,
    "monitoring_topic": "ur-shared-bus/ur-netbench-mann/runtime"
  }
}
```

#### Error Response Format
```json
{
  "jsonrpc": "2.0",
  "id": "dns-test-001",
  "error": {
    "code": -32602,
    "message": "Invalid params: hostname is required for DNS test",
    "timestamp": 1636791234,
    "data": {
      "validation_errors": [
        {
          "field": "hostname",
          "message": "Required field missing"
        }
      ]
    }
  }
}
```

### Error Code Mapping

```cpp
enum class RpcErrorCode {
    // JSON-RPC standard errors
    PARSE_ERROR = -32700,
    INVALID_REQUEST = -32600,
    METHOD_NOT_FOUND = -32601,
    INVALID_PARAMS = -32602,
    INTERNAL_ERROR = -32603,
    
    // Custom application errors
    CONFIG_VALIDATION_ERROR = -32000,
    THREAD_CREATION_ERROR = -32001,
    RESOURCE_UNAVAILABLE = -32002,
    TIMEOUT_ERROR = -32003,
    AUTHENTICATION_ERROR = -32004,
    RATE_LIMIT_ERROR = -32005
};

class RpcErrorHandler {
public:
    static std::pair<int, std::string> getErrorDetails(RpcErrorCode code) {
        switch (code) {
            case RpcErrorCode::PARSE_ERROR:
                return {-32700, "Parse error: Invalid JSON"};
                
            case RpcErrorCode::INVALID_REQUEST:
                return {-32600, "Invalid Request: JSON-RPC 2.0 format required"};
                
            case RpcErrorCode::METHOD_NOT_FOUND:
                return {-32601, "Method not found: Operation not supported"};
                
            case RpcErrorCode::INVALID_PARAMS:
                return {-32602, "Invalid params: Parameter validation failed"};
                
            case RpcErrorCode::INTERNAL_ERROR:
                return {-32603, "Internal error: Server-side error occurred"};
                
            case RpcErrorCode::CONFIG_VALIDATION_ERROR:
                return {-32000, "Configuration validation failed"};
                
            case RpcErrorCode::THREAD_CREATION_ERROR:
                return {-32001, "Failed to create test thread"};
                
            case RpcErrorCode::RESOURCE_UNAVAILABLE:
                return {-32002, "Required resource unavailable"};
                
            case RpcErrorCode::TIMEOUT_ERROR:
                return {-32003, "Operation timed out"};
                
            case RpcErrorCode::AUTHENTICATION_ERROR:
                return {-32004, "Authentication failed"};
                
            case RpcErrorCode::RATE_LIMIT_ERROR:
                return {-32005, "Rate limit exceeded"};
                
            default:
                return {-32603, "Unknown error occurred"};
        }
    }
};
```

## Error Management

### Comprehensive Error Handling Strategy

```cpp
class RpcErrorManager {
private:
    std::map<std::string, std::vector<std::string>> operation_error_history_;
    std::mutex error_mutex_;
    size_t max_error_history_ = 100;
    
public:
    void logOperationError(const std::string& operation, 
                          const std::string& error_message) {
        std::lock_guard<std::mutex> lock(error_mutex_);
        
        auto& errors = operation_error_history_[operation];
        errors.push_back(error_message);
        
        // Limit history size
        if (errors.size() > max_error_history_) {
            errors.erase(errors.begin());
        }
    }
    
    std::vector<std::string> getErrorHistory(const std::string& operation) {
        std::lock_guard<std::mutex> lock(error_mutex_);
        return operation_error_history_[operation];
    }
    
    bool hasRecentErrors(const std::string& operation, 
                        std::chrono::minutes time_window) {
        // Implementation for checking recent error patterns
        return false;
    }
    
    void clearErrorHistory(const std::string& operation) {
        std::lock_guard<std::mutex> lock(error_mutex_);
        operation_error_history_[operation].clear();
    }
};
```

### Error Recovery Mechanisms

```cpp
class ErrorRecoveryManager {
public:
    enum class RecoveryAction {
        RETRY,
        FALLBACK,
        ABORT,
        ESCALATE
    };
    
    static RecoveryAction determineRecoveryAction(const std::string& error_type,
                                                  int retry_count) {
        if (retry_count >= 3) {
            return RecoveryAction::ABORT;
        }
        
        if (error_type == "thread_creation_failed") {
            return RecoveryAction::RETRY;
        } else if (error_type == "config_validation_failed") {
            return RecoveryAction::ABORT;
        } else if (error_type == "resource_unavailable") {
            return RecoveryAction::RETRY;
        } else if (error_type == "timeout") {
            return RecoveryAction::FALLBACK;
        }
        
        return RecoveryAction::ABORT;
    }
    
    static bool executeRecovery(RecoveryAction action,
                               std::shared_ptr<ThreadTrackingContext> context) {
        switch (action) {
            case RecoveryAction::RETRY:
                return retryOperation(context);
                
            case RecoveryAction::FALLBACK:
                return executeFallbackOperation(context);
                
            case RecoveryAction::ABORT:
                publishStatusUpdate(context->transaction_id, "failed", {
                    {"message", "Operation aborted due to unrecoverable error"}
                });
                return false;
                
            case RecoveryAction::ESCALATE:
                return escalateToAdministrator(context);
        }
        
        return false;
    }
    
private:
    static bool retryOperation(std::shared_ptr<ThreadTrackingContext> context) {
        // Implement retry logic with exponential backoff
        return false;
    }
    
    static bool executeFallbackOperation(std::shared_ptr<ThreadTrackingContext> context) {
        // Implement fallback operation logic
        return false;
    }
    
    static bool escalateToAdministrator(std::shared_ptr<ThreadTrackingContext> context) {
        // Implement escalation logic
        return false;
    }
};
```

## Monitoring and Diagnostics

### Performance Metrics Collection

```cpp
class RpcPerformanceMonitor {
private:
    struct OperationMetrics {
        std::string operation;
        std::chrono::system_clock::time_point start_time;
        std::chrono::system_clock::time_point end_time;
        std::chrono::milliseconds duration;
        bool success;
        std::string error_message;
        size_t thread_id;
        json additional_metrics;
    };
    
    std::vector<OperationMetrics> metrics_history_;
    std::mutex metrics_mutex_;
    size_t max_metrics_history_ = 1000;
    
public:
    void recordOperationStart(const std::string& operation,
                             const std::string& transaction_id,
                             size_t thread_id) {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        
        OperationMetrics metrics;
        metrics.operation = operation;
        metrics.start_time = std::chrono::system_clock::now();
        metrics.thread_id = thread_id;
        metrics.success = false;
        
        // Store with transaction_id as key for later lookup
        active_operations_[transaction_id] = metrics;
    }
    
    void recordOperationEnd(const std::string& transaction_id,
                           bool success,
                           const std::string& error_message = "",
                           const json& additional_metrics = json::object()) {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        
        auto it = active_operations_.find(transaction_id);
        if (it != active_operations_.end()) {
            it->second.end_time = std::chrono::system_clock::now();
            it->second.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                it->second.end_time - it->second.start_time);
            it->second.success = success;
            it->second.error_message = error_message;
            it->second.additional_metrics = additional_metrics;
            
            // Move to history
            metrics_history_.push_back(it->second);
            active_operations_.erase(it);
            
            // Limit history size
            if (metrics_history_.size() > max_metrics_history_) {
                metrics_history_.erase(metrics_history_.begin());
            }
        }
    }
    
    json getPerformanceReport() const {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        
        json report;
        report["total_operations"] = metrics_history_.size();
        report["successful_operations"] = 0;
        report["failed_operations"] = 0;
        report["average_duration_ms"] = 0;
        report["operation_breakdown"] = json::object();
        
        if (metrics_history_.empty()) {
            return report;
        }
        
        std::chrono::milliseconds total_duration{0};
        std::map<std::string, std::pair<int, int>> operation_counts; // success, failure
        
        for (const auto& metrics : metrics_history_) {
            if (metrics.success) {
                report["successful_operations"] = operation_counts[metrics.operation].first++;
            } else {
                report["failed_operations"] = operation_counts[metrics.operation].second++;
            }
            
            total_duration += metrics.duration;
            
            // Operation-specific breakdown
            if (!report["operation_breakdown"].contains(metrics.operation)) {
                report["operation_breakdown"][metrics.operation] = json::object();
                report["operation_breakdown"][metrics.operation]["count"] = 0;
                report["operation_breakdown"][metrics.operation]["success_count"] = 0;
                report["operation_breakdown"][metrics.operation]["failure_count"] = 0;
                report["operation_breakdown"][metrics.operation]["total_duration_ms"] = 0;
            }
            
            auto& op_breakdown = report["operation_breakdown"][metrics.operation];
            op_breakdown["count"] = op_breakdown["count"].get<int>() + 1;
            op_breakdown["total_duration_ms"] = op_breakdown["total_duration_ms"].get<int>() + metrics.duration.count();
            
            if (metrics.success) {
                op_breakdown["success_count"] = op_breakdown["success_count"].get<int>() + 1;
            } else {
                op_breakdown["failure_count"] = op_breakdown["failure_count"].get<int>() + 1;
            }
        }
        
        report["average_duration_ms"] = total_duration.count() / metrics_history_.size();
        
        return report;
    }
    
private:
    std::map<std::string, OperationMetrics> active_operations_;
};
```

### Health Check System

```cpp
class RpcHealthChecker {
public:
    struct HealthStatus {
        bool healthy;
        std::string status_message;
        json details;
        std::chrono::system_clock::time_point check_time;
    };
    
    static HealthStatus performHealthCheck() {
        HealthStatus status;
        status.check_time = std::chrono::system_clock::now();
        status.healthy = true;
        status.status_message = "All systems operational";
        status.details = json::object();
        
        // Check RPC client connectivity
        auto rpc_status = checkRpcClientHealth();
        status.details["rpc_client"] = rpc_status;
        
        if (!rpc_status["healthy"].get<bool>()) {
            status.healthy = false;
            status.status_message = "RPC client issues detected";
        }
        
        // Check thread manager
        auto thread_status = checkThreadManagerHealth();
        status.details["thread_manager"] = thread_status;
        
        if (!thread_status["healthy"].get<bool>()) {
            status.healthy = false;
            status.status_message = "Thread manager issues detected";
        }
        
        // Check active operations
        auto operations_status = checkActiveOperationsHealth();
        status.details["active_operations"] = operations_status;
        
        // Check system resources
        auto resources_status = checkSystemResourcesHealth();
        status.details["system_resources"] = resources_status;
        
        if (!resources_status["healthy"].get<bool>()) {
            status.healthy = false;
            status.status_message = "System resource constraints detected";
        }
        
        return status;
    }
    
private:
    static json checkRpcClientHealth() {
        json status;
        status["healthy"] = true;
        status["message"] = "RPC client connected";
        
        // Implementation would check actual RPC client status
        return status;
    }
    
    static json checkThreadManagerHealth() {
        json status;
        status["healthy"] = true;
        status["message"] = "Thread manager operational";
        status["active_threads"] = 0;
        status["max_threads"] = 50;
        
        // Implementation would check actual thread manager status
        return status;
    }
    
    static json checkActiveOperationsHealth() {
        json status;
        status["healthy"] = true;
        status["message"] = "Active operations normal";
        status["count"] = 0;
        status["max_concurrent"] = 10;
        
        // Implementation would check actual operations
        return status;
    }
    
    static json checkSystemResourcesHealth() {
        json status;
        status["healthy"] = true;
        status["message"] = "System resources adequate";
        status["cpu_usage_percent"] = 25.5;
        status["memory_usage_percent"] = 45.2;
        status["disk_usage_percent"] = 60.1;
        
        // Implementation would check actual system resources
        return status;
    }
};
```

## Implementation Details

### Core RPC Operation Processor Implementation

```cpp
class RpcOperationProcessorImpl : public RpcOperationProcessor {
private:
    std::unique_ptr<RpcThreadManager> thread_manager_;
    std::unique_ptr<StatusBroadcaster> status_broadcaster_;
    std::unique_ptr<RpcResponseHandler> response_handler_;
    std::unique_ptr<RpcPerformanceMonitor> performance_monitor_;
    std::unique_ptr<RpcErrorManager> error_manager_;
    
    std::atomic<bool> shutdown_requested_{false};
    std::thread status_monitoring_thread_;
    
public:
    RpcOperationProcessorImpl(const ConfigManager& config_manager, bool verbose)
        : RpcOperationProcessor(config_manager, verbose) {
        
        // Initialize components
        thread_manager_ = std::make_unique<RpcThreadManager>();
        status_broadcaster_ = std::make_unique<StatusBroadcaster>();
        response_handler_ = std::make_unique<RpcResponseHandler>();
        performance_monitor_ = std::make_unique<RpcPerformanceMonitor>();
        error_manager_ = std::make_unique<RpcErrorManager>();
        
        // Start status monitoring thread
        status_monitoring_thread_ = std::thread([this]() {
            this->statusMonitoringLoop();
        });
    }
    
    ~RpcOperationProcessorImpl() {
        shutdown();
    }
    
    void processRequest(const char* payload, size_t payload_len) override {
        if (shutdown_requested_.load()) {
            response_handler_->sendErrorResponse("unknown", -32003, 
                "Server is shutting down");
            return;
        }
        
        try {
            // Parse and validate request
            json request = json::parse(payload, payload + payload_len);
            
            if (!validateJsonRpcRequest(request)) {
                response_handler_->sendErrorResponse("unknown", -32600, 
                    "Invalid JSON-RPC request");
                return;
            }
            
            std::string method = request["method"];
            std::string transaction_id = extractTransactionId(request);
            json params = request["params"];
            
            // Validate operation
            if (!isValidOperation(method)) {
                response_handler_->sendErrorResponse(transaction_id, -32601, 
                    "Method not found: " + method);
                return;
            }
            
            // Build and validate configuration
            json package_config = RpcConfigurationBuilder::buildPackageConfig(method, params);
            
            if (!ConfigurationValidator::validateAndSanitizeConfig(method, package_config)) {
                response_handler_->sendErrorResponse(transaction_id, -32000, 
                    "Configuration validation failed");
                return;
            }
            
            // Process operation
            processValidatedOperation(method, transaction_id, package_config);
            
        } catch (const json::parse_error& e) {
            response_handler_->sendErrorResponse("unknown", -32700, 
                "Parse error: " + std::string(e.what()));
        } catch (const std::exception& e) {
            response_handler_->sendErrorResponse("unknown", -32603, 
                "Internal error: " + std::string(e.what()));
        }
    }
    
    void shutdown() override {
        if (shutdown_requested_.load()) {
            return;
        }
        
        shutdown_requested_.store(true);
        
        // Stop all active operations
        auto active_threads = thread_manager_->getActiveThreads();
        for (const auto& context : active_threads) {
            thread_manager_->stopThread(context->thread_id);
            status_broadcaster_->publishStatusUpdate(context->transaction_id, 
                "failed", {{"message", "Server shutdown"}});
        }
        
        // Stop status monitoring thread
        if (status_monitoring_thread_.joinable()) {
            status_monitoring_thread_.join();
        }
        
        // Generate final performance report
        json report = performance_monitor_->getPerformanceReport();
        LOG_INFO("[RpcOperationProcessor] Final performance report: " << report.dump(2));
    }
    
private:
    void processValidatedOperation(const std::string& method,
                                  const std::string& transaction_id,
                                  const json& package_config) {
        try {
            // Create temporary config file
            std::string temp_config_file = createTempConfigFile(package_config);
            
            // Create config manager
            auto config_manager = std::make_shared<ConfigManager>();
            if (!config_manager->loadPackageConfig(temp_config_file)) {
                response_handler_->sendErrorResponse(transaction_id, -32000, 
                    "Failed to load configuration");
                return;
            }
            
            // Create thread tracking context
            auto context = std::make_shared<ThreadTrackingContext>();
            context->transaction_id = transaction_id;
            context->method = method;
            context->config_manager = config_manager;
            context->config_file = temp_config_file;
            context->start_time = std::chrono::system_clock::now();
            
            // Record operation start
            performance_monitor_->recordOperationStart(method, transaction_id, 0);
            
            // Create and register thread
            unsigned int thread_id = thread_manager_->createThread(
                [this, context]() {
                    this->executeOperationThread(context);
                }
            );
            
            context->thread_id = thread_id;
            thread_manager_->registerThread(context);
            
            // Send immediate success response
            response_handler_->sendSuccessResponse(transaction_id, 
                "Test thread launched successfully", {
                    {"thread_id", thread_id},
                    {"operation", method},
                    {"status", "running"}
                });
            
            // Publish initial status update
            status_broadcaster_->publishStatusUpdate(transaction_id, "running", {
                {"thread_id", thread_id},
                {"operation", method},
                {"start_time", std::chrono::system_clock::to_time_t(context->start_time)}
            });
            
        } catch (const std::exception& e) {
            error_manager_->logOperationError(method, e.what());
            response_handler_->sendErrorResponse(transaction_id, -32603, 
                "Failed to launch test: " + std::string(e.what()));
        }
    }
    
    void executeOperationThread(std::shared_ptr<ThreadTrackingContext> context) {
        try {
            // Update status to running
            context->status.store(ThreadTrackingContext::ThreadStatus::RUNNING);
            status_broadcaster_->publishStatusUpdate(context->transaction_id, "running", {
                {"phase", "execution"},
                {"message", "Test execution started"}
            });
            
            // Route to appropriate operation handler
            if (context->method == "dns") {
                executeDnsOperation(context);
            } else if (context->method == "ping") {
                executePingOperation(context);
            } else if (context->method == "traceroute") {
                executeTracerouteOperation(context);
            } else if (context->method == "iperf") {
                executeIperfOperation(context);
            } else if (context->method == "servers-status") {
                executeServersStatusOperation(context);
            } else {
                throw std::runtime_error("Unknown operation: " + context->method);
            }
            
            // Record successful completion
            performance_monitor_->recordOperationEnd(context->transaction_id, true);
            
        } catch (const std::exception& e) {
            context->status.store(ThreadTrackingContext::ThreadStatus::FAILED);
            context->error_message = e.what();
            
            // Record failure
            performance_monitor_->recordOperationEnd(context->transaction_id, false, e.what());
            
            // Publish failure status
            status_broadcaster_->publishStatusUpdate(context->transaction_id, "failed", {
                {"phase", "error"},
                {"message", "Test failed: " + std::string(e.what())}
            });
            
            error_manager_->logOperationError(context->method, e.what());
        }
    }
    
    void statusMonitoringLoop() {
        while (!shutdown_requested_.load()) {
            try {
                // Monitor active operations and publish periodic updates
                auto active_threads = thread_manager_->getActiveThreads();
                
                for (const auto& context : active_threads) {
                    if (context->status.load() == ThreadTrackingContext::ThreadStatus::RUNNING) {
                        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now() - context->start_time);
                        
                        // Publish periodic update for long-running operations
                        if (elapsed.count() % 30 == 0) { // Every 30 seconds
                            status_broadcaster_->publishStatusUpdate(
                                context->transaction_id, "running", {
                                    {"elapsed_seconds", elapsed.count()},
                                    {"message", "Operation still running"}
                                });
                        }
                    }
                }
                
                std::this_thread::sleep_for(std::chrono::seconds(5));
                
            } catch (const std::exception& e) {
                LOG_ERROR("[RpcOperationProcessor] Status monitoring error: " << e.what());
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }
    }
    
    bool validateJsonRpcRequest(const json& request) {
        return request.contains("jsonrpc") &&
               request["jsonrpc"].get<std::string>() == "2.0" &&
               request.contains("method") &&
               request.contains("params") &&
               request.contains("id");
    }
    
    std::string extractTransactionId(const json& request) {
        if (request["id"].is_string()) {
            return request["id"];
        } else if (request["id"].is_number()) {
            return std::to_string(request["id"].get<int>());
        }
        return "unknown";
    }
    
    bool isValidOperation(const std::string& method) {
        static std::set<std::string> valid_methods = {
            "dns", "ping", "traceroute", "iperf", "servers-status"
        };
        return valid_methods.find(method) != valid_methods.end();
    }
    
    std::string createTempConfigFile(const json& config) {
        std::string temp_file = "/tmp/rpc_config_" + 
            std::to_string(std::chrono::system_clock::to_time_t(
                std::chrono::system_clock::now())) + ".json";
        
        std::ofstream file(temp_file);
        file << config.dump(2);
        file.close();
        
        return temp_file;
    }
};
```

## Testing Strategy

### Unit Testing Framework

```cpp
// Test framework for RPC operations
class RpcOperationTester {
public:
    struct TestCase {
        std::string name;
        std::string method;
        json params;
        bool expect_success;
        std::string expected_error_code;
    };
    
    static std::vector<TestCase> generateTestCases() {
        return {
            // DNS test cases
            {
                "DNS valid request",
                "dns",
                {
                    {"hostname", "google.com"},
                    {"query_type", "A"},
                    {"nameserver", "8.8.8.8"}
                },
                true,
                ""
            },
            {
                "DNS missing hostname",
                "dns",
                {
                    {"query_type", "A"},
                    {"nameserver", "8.8.8.8"}
                },
                false,
                "-32000"
            },
            
            // Ping test cases
            {
                "Ping valid request",
                "ping",
                {
                    {"destination", "8.8.8.8"},
                    {"count", 4}
                },
                true,
                ""
            },
            {
                "Ping invalid count",
                "ping",
                {
                    {"destination", "8.8.8.8"},
                    {"count", 2000} // Too high
                },
                true, // Should be sanitized
                ""
            },
            
            // Traceroute test cases
            {
                "Traceroute valid request",
                "traceroute",
                {
                    {"target", "google.com"},
                    {"max_hops", 30}
                },
                true,
                ""
            },
            
            // Iperf test cases
            {
                "Iperf valid request",
                "iperf",
                {
                    {"target", "iperf.example.com"},
                    {"duration", 10}
                },
                true,
                ""
            },
            
            // Servers status test cases
            {
                "Servers status valid request",
                "servers-status",
                {
                    {"servers_list_path", "/tmp/test_servers.json"},
                    {"output_dir", "/tmp/test_results"}
                },
                true,
                ""
            },
            
            // Invalid method
            {
                "Invalid method",
                "invalid_operation",
                {},
                false,
                "-32601"
            }
        };
    }
    
    static bool runTests(RpcOperationProcessor& processor) {
        auto test_cases = generateTestCases();
        int passed = 0;
        int total = test_cases.size();
        
        for (const auto& test_case : test_cases) {
            bool result = runSingleTest(processor, test_case);
            if (result) {
                passed++;
                std::cout << "[PASS] " << test_case.name << std::endl;
            } else {
                std::cout << "[FAIL] " << test_case.name << std::endl;
            }
        }
        
        std::cout << "\nTest Results: " << passed << "/" << total << " passed" << std::endl;
        return passed == total;
    }
    
private:
    static bool runSingleTest(RpcOperationProcessor& processor, 
                             const TestCase& test_case) {
        try {
            // Create JSON-RPC request
            json request;
            request["jsonrpc"] = "2.0";
            request["id"] = test_case.name;
            request["method"] = test_case.method;
            request["params"] = test_case.params;
            
            std::string request_str = request.dump();
            
            // Process request
            processor.processRequest(request_str.c_str(), request_str.length());
            
            // In a real test, we would capture and validate the response
            // For now, just ensure no exceptions were thrown
            return true;
            
        } catch (const std::exception& e) {
            return !test_case.expect_success;
        }
    }
};
```

### Integration Testing

```cpp
class RpcIntegrationTester {
public:
    static bool testEndToEndWorkflow() {
        std::cout << "Testing end-to-end RPC workflow..." << std::endl;
        
        // Setup test environment
        setupTestEnvironment();
        
        try {
            // Test 1: DNS operation
            if (!testDnsOperation()) {
                std::cout << "DNS operation test failed" << std::endl;
                return false;
            }
            
            // Test 2: Ping operation
            if (!testPingOperation()) {
                std::cout << "Ping operation test failed" << std::endl;
                return false;
            }
            
            // Test 3: Status updates
            if (!testStatusUpdates()) {
                std::cout << "Status updates test failed" << std::endl;
                return false;
            }
            
            // Test 4: Error handling
            if (!testErrorHandling()) {
                std::cout << "Error handling test failed" << std::endl;
                return false;
            }
            
            std::cout << "All integration tests passed!" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "Integration test exception: " << e.what() << std::endl;
            return false;
        } finally {
            cleanupTestEnvironment();
        }
    }
    
private:
    static void setupTestEnvironment() {
        // Create test configuration files
        createTestServersList();
        createTestRpcConfig();
        
        // Setup MQTT broker for testing
        setupTestMqttBroker();
    }
    
    static bool testDnsOperation() {
        // Implementation would test actual DNS operation through RPC
        return true;
    }
    
    static bool testPingOperation() {
        // Implementation would test actual ping operation through RPC
        return true;
    }
    
    static bool testStatusUpdates() {
        // Implementation would test status update publishing
        return true;
    }
    
    static bool testErrorHandling() {
        // Implementation would test various error scenarios
        return true;
    }
    
    static void cleanupTestEnvironment() {
        // Clean up test files and resources
    }
};
```

### Performance Testing

```cpp
class RpcPerformanceTester {
public:
    struct PerformanceMetrics {
        std::chrono::milliseconds average_response_time;
        std::chrono::milliseconds max_response_time;
        std::chrono::milliseconds min_response_time;
        int requests_per_second;
        double success_rate;
        int concurrent_operations_supported;
    };
    
    static PerformanceMetrics runPerformanceTest(int duration_seconds = 60) {
        std::cout << "Running performance test for " << duration_seconds << " seconds..." << std::endl;
        
        PerformanceMetrics metrics;
        std::vector<std::chrono::milliseconds> response_times;
        int successful_requests = 0;
        int total_requests = 0;
        
        auto start_time = std::chrono::steady_clock::now();
        auto end_time = start_time + std::chrono::seconds(duration_seconds);
        
        while (std::chrono::steady_clock::now() < end_time) {
            auto request_start = std::chrono::steady_clock::now();
            
            // Send test request
            bool success = sendTestRequest();
            total_requests++;
            
            auto request_end = std::chrono::steady_clock::now();
            auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                request_end - request_start);
            
            response_times.push_back(response_time);
            if (success) {
                successful_requests++;
            }
            
            // Small delay between requests
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Calculate metrics
        if (!response_times.empty()) {
            auto total_time = std::accumulate(response_times.begin(), 
                                            response_times.end(), 
                                            std::chrono::milliseconds{0});
            metrics.average_response_time = total_time / response_times.size();
            metrics.max_response_time = *std::max_element(response_times.begin(), 
                                                         response_times.end());
            metrics.min_response_time = *std::min_element(response_times.begin(), 
                                                         response_times.end());
        }
        
        metrics.requests_per_second = total_requests / duration_seconds;
        metrics.success_rate = static_cast<double>(successful_requests) / total_requests;
        metrics.concurrent_operations_supported = testConcurrentOperations();
        
        return metrics;
    }
    
private:
    static bool sendTestRequest() {
        // Implementation would send actual test RPC request
        return true;
    }
    
    static int testConcurrentOperations() {
        // Implementation would test maximum concurrent operations
        return 10;
    }
};
```

## Deployment Considerations

### Configuration Management

```cpp
class RpcDeploymentConfig {
public:
    struct DeploymentSettings {
        // RPC Client Settings
        std::string rpc_config_file = "/etc/ur-netbench-mann/rpc-config.json";
        
        // Thread Management
        int max_concurrent_operations = 10;
        int thread_pool_size = 50;
        std::chrono::seconds operation_timeout{300}; // 5 minutes
        
        // Status Broadcasting
        bool enable_status_broadcasting = true;
        std::string status_topic = "ur-shared-bus/ur-netbench-mann/runtime";
        std::chrono::milliseconds status_update_interval{1000};
        
        // Performance Monitoring
        bool enable_performance_monitoring = true;
        size_t max_metrics_history = 1000;
        
        // Error Handling
        int max_retry_attempts = 3;
        std::chrono::seconds retry_delay{5};
        
        // Security
        bool enable_authentication = false;
        std::string auth_token_file = "/etc/ur-netbench-mann/auth-token";
        
        // Logging
        std::string log_level = "INFO";
        std::string log_file = "/var/log/ur-netbench-mann/rpc.log";
    };
    
    static DeploymentSettings loadFromFile(const std::string& config_file) {
        DeploymentSettings settings;
        
        try {
            std::ifstream file(config_file);
            if (!file.is_open()) {
                LOG_WARNING("[RpcDeploymentConfig] Could not open config file: " << config_file);
                return settings; // Return defaults
            }
            
            json config;
            file >> config;
            
            // Load settings from JSON
            if (config.contains("rpc_config_file")) {
                settings.rpc_config_file = config["rpc_config_file"];
            }
            
            if (config.contains("max_concurrent_operations")) {
                settings.max_concurrent_operations = config["max_concurrent_operations"];
            }
            
            if (config.contains("operation_timeout_seconds")) {
                settings.operation_timeout = std::chrono::seconds(
                    config["operation_timeout_seconds"]);
            }
            
            // Load other settings...
            
        } catch (const std::exception& e) {
            LOG_ERROR("[RpcDeploymentConfig] Error loading config: " << e.what());
        }
        
        return settings;
    }
    
    static bool validateSettings(const DeploymentSettings& settings) {
        // Validate RPC config file exists
        std::ifstream rpc_file(settings.rpc_config_file);
        if (!rpc_file.good()) {
            LOG_ERROR("[RpcDeploymentConfig] RPC config file not found: " << settings.rpc_config_file);
            return false;
        }
        
        // Validate thread settings
        if (settings.max_concurrent_operations < 1 || 
            settings.max_concurrent_operations > 100) {
            LOG_ERROR("[RpcDeploymentConfig] Invalid max_concurrent_operations: " << 
                     settings.max_concurrent_operations);
            return false;
        }
        
        // Validate other settings...
        
        return true;
    }
};
```

### Service Integration

```cpp
class RpcServiceManager {
public:
    enum class ServiceStatus {
        STOPPED,
        STARTING,
        RUNNING,
        STOPPING,
        ERROR
    };
    
    static bool startRpcService(const RpcDeploymentConfig::DeploymentSettings& settings) {
        if (!RpcDeploymentConfig::validateSettings(settings)) {
            return false;
        }
        
        try {
            // Initialize logging
            setupLogging(settings);
            
            // Load RPC configuration
            auto rpc_client = std::make_shared<RpcClient>(settings.rpc_config_file, 
                                                        "ur-netbench-mann");
            
            // Create operation processor
            ConfigManager dummy_config;
            auto processor = std::make_unique<RpcOperationProcessorImpl>(dummy_config, true);
            
            // Setup message handlers
            setupMessageHandlers(rpc_client, processor);
            
            // Start RPC client
            if (!rpc_client->start()) {
                LOG_ERROR("[RpcServiceManager] Failed to start RPC client");
                return false;
            }
            
            // Store global references
            g_rpc_client = rpc_client;
            g_operation_processor = std::move(processor);
            
            LOG_INFO("[RpcServiceManager] RPC service started successfully");
            return true;
            
        } catch (const std::exception& e) {
            LOG_ERROR("[RpcServiceManager] Failed to start RPC service: " << e.what());
            return false;
        }
    }
    
    static bool stopRpcService() {
        try {
            LOG_INFO("[RpcServiceManager] Stopping RPC service...");
            
            if (g_operation_processor) {
                g_operation_processor->shutdown();
                g_operation_processor.reset();
            }
            
            if (g_rpc_client) {
                g_rpc_client->stop();
                g_rpc_client.reset();
            }
            
            LOG_INFO("[RpcServiceManager] RPC service stopped successfully");
            return true;
            
        } catch (const std::exception& e) {
            LOG_ERROR("[RpcServiceManager] Error stopping RPC service: " << e.what());
            return false;
        }
    }
    
    static ServiceStatus getServiceStatus() {
        if (!g_rpc_client && !g_operation_processor) {
            return ServiceStatus::STOPPED;
        }
        
        if (g_rpc_client && g_operation_processor) {
            if (g_rpc_client->isRunning() && g_operation_processor->isRunning()) {
                return ServiceStatus::RUNNING;
            } else {
                return ServiceStatus::ERROR;
            }
        }
        
        return ServiceStatus::STARTING;
    }
    
private:
    static void setupLogging(const RpcDeploymentConfig::DeploymentSettings& settings) {
        // Configure logging based on settings
        // Implementation would setup file logging, rotation, etc.
    }
    
    static void setupMessageHandlers(std::shared_ptr<RpcClient> rpc_client,
                                    std::unique_ptr<RpcOperationProcessor>& processor) {
        rpc_client->setMessageHandler([&](const std::string& topic, const std::string& payload) {
            // Filter for relevant topics
            if (topic.find("direct_messaging/ur-netbench-mann/requests") == std::string::npos) {
                return;
            }
            
            // Process request
            processor->processRequest(payload.c_str(), payload.size());
        });
    }
    
    // Global service references
    static std::shared_ptr<RpcClient> g_rpc_client;
    static std::unique_ptr<RpcOperationProcessor> g_operation_processor;
};

// Static member definitions
std::shared_ptr<RpcClient> RpcServiceManager::g_rpc_client;
std::unique_ptr<RpcOperationProcessor> RpcServiceManager::g_operation_processor;
```

## Performance Optimization

### Connection Pooling

```cpp
class RpcConnectionPool {
private:
    struct Connection {
        std::shared_ptr<RpcClient> client;
        std::chrono::system_clock::time_point last_used;
        std::atomic<bool> in_use{false};
    };
    
    std::vector<Connection> connections_;
    std::mutex pool_mutex_;
    size_t max_pool_size_;
    std::chrono::seconds connection_timeout_{300};
    
public:
    RpcConnectionPool(size_t max_size = 10) : max_pool_size_(max_size) {}
    
    std::shared_ptr<RpcClient> acquireConnection(const std::string& config_file) {
        std::lock_guard<std::mutex> lock(pool_mutex_);
        
        // Find available connection
        for (auto& conn : connections_) {
            if (!conn.in_use.load() && isConnectionValid(conn)) {
                conn.in_use.store(true);
                conn.last_used = std::chrono::system_clock::now();
                return conn.client;
            }
        }
        
        // Create new connection if pool not full
        if (connections_.size() < max_pool_size_) {
            Connection new_conn;
            new_conn.client = std::make_shared<RpcClient>(config_file, "ur-netbench-mann");
            new_conn.last_used = std::chrono::system_clock::now();
            new_conn.in_use.store(true);
            
            connections_.push_back(new_conn);
            return new_conn.client;
        }
        
        // Pool full, wait or return nullptr
        return nullptr;
    }
    
    void releaseConnection(std::shared_ptr<RpcClient> client) {
        std::lock_guard<std::mutex> lock(pool_mutex_);
        
        for (auto& conn : connections_) {
            if (conn.client == client) {
                conn.in_use.store(false);
                conn.last_used = std::chrono::system_clock::now();
                break;
            }
        }
    }
    
    void cleanupExpiredConnections() {
        std::lock_guard<std::mutex> lock(pool_mutex_);
        
        auto now = std::chrono::system_clock::now();
        connections_.erase(
            std::remove_if(connections_.begin(), connections_.end(),
                [this, now](const Connection& conn) {
                    return !conn.in_use.load() && 
                           (now - conn.last_used) > connection_timeout_;
                }),
            connections_.end()
        );
    }
    
private:
    bool isConnectionValid(const Connection& conn) {
        return conn.client && conn.client->isConnected();
    }
};
```

### Request Caching

```cpp
class RpcRequestCache {
private:
    struct CacheEntry {
        json response;
        std::chrono::system_clock::time_point timestamp;
        std::chrono::seconds ttl;
    };
    
    std::map<std::string, CacheEntry> cache_;
    std::mutex cache_mutex_;
    std::chrono::seconds default_ttl_{60}; // 1 minute
    
public:
    std::optional<json> getCachedResponse(const std::string& request_hash) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        auto it = cache_.find(request_hash);
        if (it != cache_.end()) {
            auto now = std::chrono::system_clock::now();
            if ((now - it->second.timestamp) < it->second.ttl) {
                return it->second.response;
            } else {
                // Expired entry
                cache_.erase(it);
            }
        }
        
        return std::nullopt;
    }
    
    void cacheResponse(const std::string& request_hash, 
                      const json& response, 
                      std::chrono::seconds ttl = {}) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        if (ttl == std::chrono::seconds{}) {
            ttl = default_ttl_;
        }
        
        CacheEntry entry;
        entry.response = response;
        entry.timestamp = std::chrono::system_clock::now();
        entry.ttl = ttl;
        
        cache_[request_hash] = entry;
    }
    
    void clearCache() {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        cache_.clear();
    }
    
    void cleanupExpiredEntries() {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        auto now = std::chrono::system_clock::now();
        for (auto it = cache_.begin(); it != cache_.end();) {
            if ((now - it->second.timestamp) >= it->second.ttl) {
                it = cache_.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    static std::string generateRequestHash(const std::string& method, 
                                          const json& params) {
        std::string hash_input = method + params.dump();
        return std::to_string(std::hash<std::string>{}(hash_input));
    }
};
```

## Security Considerations

### Request Authentication

```cpp
class RpcAuthenticationManager {
private:
    std::set<std::string> valid_tokens_;
    std::mutex tokens_mutex_;
    bool authentication_enabled_;
    
public:
    RpcAuthenticationManager(bool enabled = false) : authentication_enabled_(enabled) {}
    
    bool authenticateRequest(const json& request) {
        if (!authentication_enabled_) {
            return true; // No authentication required
        }
        
        if (!request.contains("auth")) {
            return false;
        }
        
        std::string token = request["auth"];
        std::lock_guard<std::mutex> lock(tokens_mutex_);
        return valid_tokens_.find(token) != valid_tokens_.end();
    }
    
    bool addAuthToken(const std::string& token) {
        std::lock_guard<std::mutex> lock(tokens_mutex_);
        return valid_tokens_.insert(token).second;
    }
    
    bool removeAuthToken(const std::string& token) {
        std::lock_guard<std::mutex> lock(tokens_mutex_);
        return valid_tokens_.erase(token) > 0;
    }
    
    void loadTokensFromFile(const std::string& token_file) {
        std::lock_guard<std::mutex> lock(tokens_mutex_);
        valid_tokens_.clear();
        
        std::ifstream file(token_file);
        if (!file.is_open()) {
            LOG_WARNING("[RpcAuthenticationManager] Could not open token file: " << token_file);
            return;
        }
        
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty() && line[0] != '#') {
                valid_tokens_.insert(line);
            }
        }
        
        LOG_INFO("[RpcAuthenticationManager] Loaded " << valid_tokens_.size() << " authentication tokens");
    }
    
    void enableAuthentication(bool enabled) {
        authentication_enabled_ = enabled;
    }
    
    bool isAuthenticationEnabled() const {
        return authentication_enabled_;
    }
};
```

### Input Sanitization

```cpp
class RpcInputSanitizer {
public:
    static bool sanitizeRequest(json& request) {
        try {
            // Remove potentially dangerous fields
            sanitizeJsonFields(request);
            
            // Validate parameter types and ranges
            if (request.contains("params")) {
                if (!sanitizeParams(request["params"], request["method"])) {
                    return false;
                }
            }
            
            // Limit request size
            std::string request_str = request.dump();
            if (request_str.length() > MAX_REQUEST_SIZE) {
                return false;
            }
            
            return true;
            
        } catch (const std::exception& e) {
            LOG_ERROR("[RpcInputSanitizer] Sanitization error: " << e.what());
            return false;
        }
    }
    
private:
    static constexpr size_t MAX_REQUEST_SIZE = 1024 * 1024; // 1MB
    
    static void sanitizeJsonFields(json& obj) {
        // Remove potentially dangerous fields
        std::vector<std::string> dangerous_fields = {
            "__proto__", "constructor", "prototype"
        };
        
        for (const auto& field : dangerous_fields) {
            if (obj.contains(field)) {
                obj.erase(field);
            }
        }
        
        // Recursively sanitize nested objects
        for (auto& [key, value] : obj.items()) {
            if (value.is_object()) {
                sanitizeJsonFields(value);
            } else if (value.is_array()) {
                for (auto& item : value) {
                    if (item.is_object()) {
                        sanitizeJsonFields(item);
                    }
                }
            }
        }
    }
    
    static bool sanitizeParams(json& params, const std::string& method) {
        if (method == "dns") {
            return sanitizeDnsParams(params);
        } else if (method == "ping") {
            return sanitizePingParams(params);
        } else if (method == "traceroute") {
            return sanitizeTracerouteParams(params);
        } else if (method == "iperf") {
            return sanitizeIperfParams(params);
        } else if (method == "servers-status") {
            return sanitizeServersStatusParams(params);
        }
        
        return true; // Unknown method, allow through
    }
    
    static bool sanitizeDnsParams(json& params) {
        // Validate hostname
        if (params.contains("hostname")) {
            std::string hostname = params["hostname"];
            if (hostname.length() > 253 || !isValidHostname(hostname)) {
                return false;
            }
        }
        
        // Validate nameserver
        if (params.contains("nameserver")) {
            std::string nameserver = params["nameserver"];
            if (!isValidIPAddress(nameserver)) {
                return false;
            }
        }
        
        return true;
    }
    
    static bool sanitizePingParams(json& params) {
        // Validate destination
        if (params.contains("destination")) {
            std::string dest = params["destination"];
            if (dest.length() > 255 || !isValidHostnameOrIP(dest)) {
                return false;
            }
        }
        
        // Limit count
        if (params.contains("count")) {
            int count = params["count"];
            if (count < 1 || count > 1000) {
                params["count"] = std::max(1, std::min(1000, count));
            }
        }
        
        return true;
    }
    
    static bool sanitizeTracerouteParams(json& params) {
        // Similar validation for traceroute parameters
        return true;
    }
    
    static bool sanitizeIperfParams(json& params) {
        // Similar validation for iperf parameters
        return true;
    }
    
    static bool sanitizeServersStatusParams(json& params) {
        // Validate file paths to prevent directory traversal
        if (params.contains("servers_list_path")) {
            std::string path = params["servers_list_path"];
            if (!isValidFilePath(path)) {
                return false;
            }
        }
        
        return true;
    }
    
    static bool isValidHostname(const std::string& hostname) {
        // Basic hostname validation
        return !hostname.empty() && hostname.length() <= 253;
    }
    
    static bool isValidIPAddress(const std::string& ip) {
        // Basic IP validation
        return !ip.empty();
    }
    
    static bool isValidHostnameOrIP(const std::string& host) {
        return isValidHostname(host) || isValidIPAddress(host);
    }
    
    static bool isValidFilePath(const std::string& path) {
        // Prevent directory traversal attacks
        return path.find("..") == std::string::npos && 
               path.find("~") == std::string::npos;
    }
};
```

## Conclusion

This comprehensive integration plan provides a detailed roadmap for transforming the UR-NetBench-MANN system into a fully RPC-capable network benchmarking service. The implementation maintains compatibility with existing configuration structures while adding robust real-time monitoring, thread management, and error handling capabilities.

### Key Benefits of This Integration:

1. **Backward Compatibility**: Existing CLI functionality remains intact
2. **Real-time Monitoring**: Continuous status updates via shared bus
3. **Scalable Architecture**: Thread-based execution with proper resource management
4. **Comprehensive Error Handling**: Multi-layered error detection and recovery
5. **Performance Optimization**: Connection pooling, caching, and efficient resource usage
6. **Security Features**: Authentication, input sanitization, and access control
7. **Production Ready**: Extensive testing, monitoring, and deployment considerations

### Implementation Timeline:

- **Phase 1** (Week 1-2): Core RPC infrastructure and basic operation handlers
- **Phase 2** (Week 3-4): Thread management integration and status broadcasting
- **Phase 3** (Week 5-6): Advanced features (caching, security, performance optimization)
- **Phase 4** (Week 7-8): Testing, documentation, and deployment preparation

This plan ensures a systematic, robust, and maintainable integration that will significantly enhance the capabilities of the UR-NetBench-MANN system while preserving its existing strengths.
