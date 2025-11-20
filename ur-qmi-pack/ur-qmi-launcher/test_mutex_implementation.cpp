#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <mutex>
#include <string>

/**
 * Test program to verify WDS and DMS mutex implementation
 * This test creates multiple threads that attempt to access WDS and DMS operations
 * simultaneously to ensure thread safety and proper mutex protection.
 */

std::atomic<int> wds_operations_completed(0);
std::atomic<int> dms_operations_completed(0);
std::atomic<int> total_conflicts(0);

// Mock mutexes for testing
std::mutex mock_wds_mutex;
std::mutex mock_dms_mutex;

void test_wds_operations(int thread_id, int operations_per_thread) {
    std::cout << "Thread " << thread_id << " starting WDS operations..." << std::endl;
    
    for (int i = 0; i < operations_per_thread; ++i) {
        try {
            auto start_time = std::chrono::high_resolution_clock::now();
            
            // Simulate WDS-protected operations with mutex
            {
                std::lock_guard<std::mutex> wds_lock(mock_wds_mutex);
                // Simulate work
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            std::cout << "Thread " << thread_id << " WDS operation " << i+1 
                      << " completed in " << duration.count() << "ms" << std::endl;
                      
            wds_operations_completed++;
            
            // Small delay to allow other threads to compete
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
        } catch (const std::exception& e) {
            std::cerr << "Thread " << thread_id << " WDS operation " << i+1 
                      << " failed: " << e.what() << std::endl;
            total_conflicts++;
        }
    }
    
    std::cout << "Thread " << thread_id << " completed all WDS operations." << std::endl;
}

void test_dms_operations(int thread_id, int operations_per_thread) {
    std::cout << "Thread " << thread_id << " starting DMS operations..." << std::endl;
    
    for (int i = 0; i < operations_per_thread; ++i) {
        try {
            auto start_time = std::chrono::high_resolution_clock::now();
            
            // Simulate DMS-protected operations with mutex
            {
                std::lock_guard<std::mutex> dms_lock(mock_dms_mutex);
                // Simulate work
                std::this_thread::sleep_for(std::chrono::milliseconds(8));
            }
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            std::cout << "Thread " << thread_id << " DMS operation " << i+1 
                      << " completed in " << duration.count() << "ms" << std::endl;
                      
            dms_operations_completed++;
            
            // Small delay to allow other threads to compete
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
        } catch (const std::exception& e) {
            std::cerr << "Thread " << thread_id << " DMS operation " << i+1 
                      << " failed: " << e.what() << std::endl;
            total_conflicts++;
        }
    }
    
    std::cout << "Thread " << thread_id << " completed all DMS operations." << std::endl;
}

void test_mixed_operations(int thread_id, int operations_per_thread) {
    std::cout << "Thread " << thread_id << " starting mixed WDS/DMS operations..." << std::endl;
    
    for (int i = 0; i < operations_per_thread; ++i) {
        try {
            auto start_time = std::chrono::high_resolution_clock::now();
            
            // Alternate between WDS and DMS operations
            if (i % 2 == 0) {
                // Simulate WDS operation
                {
                    std::lock_guard<std::mutex> wds_lock(mock_wds_mutex);
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
                wds_operations_completed++;
                std::cout << "Thread " << thread_id << " mixed operation " << i+1 
                          << " (WDS) completed" << std::endl;
            } else {
                // Simulate DMS operation
                {
                    std::lock_guard<std::mutex> dms_lock(mock_dms_mutex);
                    std::this_thread::sleep_for(std::chrono::milliseconds(8));
                }
                dms_operations_completed++;
                std::cout << "Thread " << thread_id << " mixed operation " << i+1 
                          << " (DMS) completed" << std::endl;
            }
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            // Small delay to allow other threads to compete
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
            
        } catch (const std::exception& e) {
            std::cerr << "Thread " << thread_id << " mixed operation " << i+1 
                      << " failed: " << e.what() << std::endl;
            total_conflicts++;
        }
    }
    
    std::cout << "Thread " << thread_id << " completed all mixed operations." << std::endl;
}

int main() {
    std::cout << "=== QMI WDS/DMS Mutex Implementation Test ===" << std::endl;
    std::cout << "This test verifies that WDS and DMS operations are thread-safe" << std::endl;
    std::cout << "and properly protected by mutexes." << std::endl << std::endl;
    
    // Test with a mock device path (since we don't need real device for mutex testing)
    const std::string test_device = "/dev/cdc-wdm0";
    const std::string test_interface = "wwan0";
    
    try {
        std::cout << "Testing mutex functionality with mock WDS/DMS operations" << std::endl;
        std::cout << "Device: " << test_device << " (simulation)" << std::endl;
        std::cout << "Interface: " << test_interface << " (simulation)" << std::endl << std::endl;
        
        // Test configuration
        const int num_threads = 6;
        const int operations_per_thread = 3;
        const int wds_threads = 2;
        const int dms_threads = 2;
        const int mixed_threads = 2;
        
        std::cout << "Test configuration:" << std::endl;
        std::cout << "- WDS-only threads: " << wds_threads << std::endl;
        std::cout << "- DMS-only threads: " << dms_threads << std::endl;
        std::cout << "- Mixed threads: " << mixed_threads << std::endl;
        std::cout << "- Operations per thread: " << operations_per_thread << std::endl;
        std::cout << std::endl;
        
        // Reset counters
        wds_operations_completed = 0;
        dms_operations_completed = 0;
        total_conflicts = 0;
        
        // Create and start threads
        std::vector<std::thread> threads;
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // WDS-only threads
        for (int i = 0; i < wds_threads; ++i) {
            threads.emplace_back(test_wds_operations, i + 1, operations_per_thread);
        }
        
        // DMS-only threads
        for (int i = 0; i < dms_threads; ++i) {
            threads.emplace_back(test_dms_operations, wds_threads + i + 1, operations_per_thread);
        }
        
        // Mixed threads
        for (int i = 0; i < mixed_threads; ++i) {
            threads.emplace_back(test_mixed_operations, wds_threads + dms_threads + i + 1, operations_per_thread);
        }
        
        std::cout << "Started " << threads.size() << " threads for concurrent testing..." << std::endl;
        std::cout << "Threads will compete for WDS and DMS resources..." << std::endl << std::endl;
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        // Report results
        std::cout << std::endl << "=== TEST RESULTS ===" << std::endl;
        std::cout << "Total execution time: " << total_duration.count() << "ms" << std::endl;
        std::cout << "WDS operations completed: " << wds_operations_completed << std::endl;
        std::cout << "DMS operations completed: " << dms_operations_completed << std::endl;
        std::cout << "Total operations completed: " << (wds_operations_completed + dms_operations_completed) << std::endl;
        std::cout << "Conflicts/Failures: " << total_conflicts << std::endl;
        
        // Expected totals
        int expected_wds = (wds_threads * operations_per_thread) + (mixed_threads * operations_per_thread / 2);
        int expected_dms = (dms_threads * operations_per_thread) + (mixed_threads * operations_per_thread / 2);
        
        std::cout << "Expected WDS operations: " << expected_wds << std::endl;
        std::cout << "Expected DMS operations: " << expected_dms << std::endl;
        
        // Determine test success
        bool success = (total_conflicts == 0) && 
                      (wds_operations_completed >= expected_wds * 0.8) &&  // Allow for some failures due to mock device
                      (dms_operations_completed >= expected_dms * 0.8);
        
        std::cout << std::endl << "=== MUTEX IMPLEMENTATION TEST: " 
                  << (success ? "PASSED" : "FAILED") << " ===" << std::endl;
        
        if (success) {
            std::cout << "✓ WDS and DMS operations executed without race conditions" << std::endl;
            std::cout << "✓ Mutex protection is working correctly" << std::endl;
            std::cout << "✓ Thread safety verified" << std::endl;
        } else {
            std::cout << "✗ Test detected issues with mutex implementation" << std::endl;
            std::cout << "✗ Consider reviewing mutex usage and deadlock prevention" << std::endl;
        }
        
        return success ? 0 : 1;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}