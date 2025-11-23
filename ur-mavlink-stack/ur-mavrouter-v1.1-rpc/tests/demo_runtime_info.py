#!/usr/bin/env python3
"""
Simple demonstration of the runtime-info RPC method
"""

import json
import sys
import os

# Add the current directory to path to import our test module
sys.path.insert(0, os.path.dirname(__file__))

from test_rpc_thread_states import RpcThreadStateCollector

def main():
    """Demonstrate runtime-info method"""
    print("🚀 UR-MAVROUTER Runtime Info Demo")
    print("="*40)
    
    collector = RpcThreadStateCollector()
    
    if not collector.connect():
        print("❌ Failed to connect to MQTT broker")
        return 1
    
    try:
        print("📡 Requesting runtime information...")
        runtime_info = collector.get_runtime_info()
        
        if runtime_info:
            print("✅ Successfully received runtime information!\n")
            
            # Pretty print the full JSON response
            print("📄 Full JSON Response:")
            print("-" * 40)
            print(json.dumps(runtime_info, indent=2))
            
            print(f"\n📊 Summary:")
            print(f"   Status: {runtime_info.get('status', 'Unknown')}")
            print(f"   Total Threads: {runtime_info.get('total_threads', 0)}")
            print(f"   Running Threads: {runtime_info.get('running_threads', 0)}")
            print(f"   Extensions: {runtime_info.get('total_extensions', 0)}")
            print(f"   Uptime: {runtime_info.get('uptime_seconds', 0)} seconds")
            
            return 0
        else:
            print("❌ Failed to get runtime information")
            return 1
            
    except Exception as e:
        print(f"❌ Error: {e}")
        return 1
    
    finally:
        collector.disconnect()

if __name__ == "__main__":
    sys.exit(main())
