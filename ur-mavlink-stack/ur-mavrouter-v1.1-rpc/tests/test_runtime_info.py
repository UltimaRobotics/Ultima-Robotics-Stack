#!/usr/bin/env python3
"""
Test script for the new runtime-info RPC method
"""

import json
import time
import sys
import os

# Add the current directory to path to import our test module
sys.path.insert(0, os.path.dirname(__file__))

from test_rpc_thread_states import RpcThreadStateCollector

def print_runtime_info(runtime_info):
    """Print formatted runtime information"""
    print("\n" + "="*80)
    print("🚀 UR-MAVROUTER RUNTIME INFORMATION")
    print("="*80)
    
    # Basic info
    print(f"📅 Timestamp: {runtime_info.get('timestamp', 'Unknown').strip()}")
    print(f"⏱️  Uptime: {runtime_info.get('uptime_seconds', 0)} seconds")
    print(f"📊 Status: {runtime_info.get('status', 'Unknown')}")
    print(f"💬 Message: {runtime_info.get('message', 'No message')}")
    
    # Thread summary
    print(f"\n🧵 Thread Summary:")
    print(f"   Total Threads: {runtime_info.get('total_threads', 0)}")
    print(f"   Running Threads: {runtime_info.get('running_threads', 0)}")
    
    # Thread details
    threads = runtime_info.get('threads', [])
    if threads:
        print(f"\n📋 Thread Details:")
        print("-" * 80)
        print(f"{'Name':<20} {'Nature':<12} {'Type':<15} {'State':<10} {'Alive':<6} {'Critical':<8} {'Description':<30}")
        print("-" * 80)
        for thread in threads:
            print(f"{thread['name']:<20} {thread['nature']:<12} {thread['type']:<15} "
                  f"{thread['state_name']:<10} {str(thread['is_alive']):<6} {str(thread['critical']):<8} "
                  f"{thread['description'][:29]:<30}")
    
    # Extensions
    extensions = runtime_info.get('extensions', [])
    if extensions:
        print(f"\n🔌 Extensions ({runtime_info.get('total_extensions', 0)} total):")
        print("-" * 70)
        print(f"{'Name':<20} {'Type':<8} {'Running':<8} {'Thread ID':<10} {'Address':<20}")
        print("-" * 70)
        for ext in extensions:
            print(f"{ext['name']:<20} {ext['type']:<8} {str(ext['is_running']):<8} "
                  f"{ext['thread_id']:<10} {ext['address'] + ':' + str(ext['port']):<20}")
    
    # System information
    system = runtime_info.get('system', {})
    if system:
        print(f"\n⚙️  System Information:")
        print(f"   RPC Initialized: {system.get('rpc_initialized', False)}")
        print(f"   Discovery Triggered: {system.get('discovery_triggered', False)}")
        print(f"   Mainloop Started: {system.get('mainloop_started', False)}")
        print(f"   Client ID: {system.get('client_id', 'Unknown')}")
        print(f"   Config Path: {system.get('config_path', 'Unknown')}")
    
    # Device discovery
    device_discovery = runtime_info.get('device_discovery', {})
    if device_discovery:
        print(f"\n🔍 Device Discovery:")
        print(f"   Heartbeat Active: {device_discovery.get('heartbeat_active', False)}")
        print(f"   Timeout: {device_discovery.get('heartbeat_timeout_seconds', 0)} seconds")
    
    print("="*80)

def test_runtime_info():
    """Test the runtime-info RPC method"""
    print("🧪 Testing Runtime Info RPC Method")
    print("="*50)
    
    collector = RpcThreadStateCollector()
    
    if not collector.connect():
        print("✗ Failed to connect - aborting test")
        return False
    
    try:
        print("\n📡 Sending runtime-info request...")
        runtime_info = collector.get_runtime_info()
        
        if runtime_info:
            print("✓ Successfully received runtime information")
            print_runtime_info(runtime_info)
            
            # Validate required fields
            required_fields = ['status', 'total_threads', 'running_threads', 'threads', 'system']
            missing_fields = [field for field in required_fields if field not in runtime_info]
            
            if missing_fields:
                print(f"\n⚠ Warning: Missing required fields: {missing_fields}")
            else:
                print(f"\n✅ All required fields present")
            
            # Check thread nature classification
            threads = runtime_info.get('threads', [])
            nature_counts = {}
            for thread in threads:
                nature = thread.get('nature', 'unknown')
                nature_counts[nature] = nature_counts.get(nature, 0) + 1
            
            print(f"\n📊 Thread Nature Distribution:")
            for nature, count in nature_counts.items():
                print(f"   {nature}: {count} thread(s)")
            
            return True
        else:
            print("✗ Failed to get runtime information")
            return False
            
    except Exception as e:
        print(f"✗ Test failed with exception: {e}")
        return False
    
    finally:
        collector.disconnect()

def test_continuous_runtime_monitoring():
    """Test continuous runtime monitoring"""
    print("\n🔄 Continuous Runtime Monitoring")
    print("="*50)
    
    collector = RpcThreadStateCollector()
    
    if not collector.connect():
        print("✗ Failed to connect - aborting test")
        return False
    
    try:
        for i in range(1):
            print(f"\n--- Runtime Check {i+1}/5 ---")
            
            runtime_info = collector.get_runtime_info()
            if runtime_info:
                total = runtime_info.get('total_threads', 0)
                running = runtime_info.get('running_threads', 0)
                status = runtime_info.get('status', 'unknown')
                print(f"Status: {status} | Threads: {running}/{total} running")
                
                # Show any changes in thread count
                if i > 0:
                    if total != prev_total or running != prev_running:
                        print(f"🔄 Thread count changed: {prev_running}/{prev_total} -> {running}/{total}")
                
                prev_total = total
                prev_running = running
            else:
                print("✗ Failed to get runtime info")
            
            if i < 4:  # Don't sleep after last iteration
                time.sleep(3)
        
        return True
        
    except KeyboardInterrupt:
        print("\n⚠ Monitoring stopped by user")
        return True
        
    except Exception as e:
        print(f"✗ Monitoring failed: {e}")
        return False
    
    finally:
        collector.disconnect()

def main():
    """Main function"""
    print("🚀 Runtime Info Test Suite")
    print("="*60)
    
    success = True
    
    # Test 1: Basic runtime info
    if not test_runtime_info():
        success = False
    
    # Test 2: Continuous monitoring
    if not test_continuous_runtime_monitoring():
        success = False
    
    print("\n" + "="*60)
    if success:
        print("✅ All runtime info tests passed!")
    else:
        print("❌ Some runtime info tests failed!")
    
    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())
