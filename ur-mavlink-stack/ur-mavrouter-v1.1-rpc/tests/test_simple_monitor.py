#!/usr/bin/env python3
"""
Simple monitoring test to check thread states
"""

import json
import time
import sys
import os

# Add the current directory to path to import our test module
sys.path.insert(0, os.path.dirname(__file__))

from test_rpc_thread_states import RpcThreadStateCollector

def main():
    """Simple monitoring test"""
    print("🔍 Simple Thread State Monitor")
    print("="*40)
    
    collector = RpcThreadStateCollector()
    
    if not collector.connect():
        print("✗ Failed to connect")
        return 1
    
    try:
        for i in range(5):
            print(f"\n--- Check {i+1} ---")
            
            result = collector.get_all_thread_status()
            if result:
                print(f"Status: {result.get('message', 'Unknown')}")
                print(f"Threads: {list(result.get('threads', {}).keys())}")
                
                for thread_name, thread_info in result.get('threads', {}).items():
                    print(f"  {thread_name}: ID={thread_info.get('threadId', 0)}, "
                          f"Alive={thread_info.get('isAlive', False)}, "
                          f"State={thread_info.get('state', 0)}")
            else:
                print("✗ Failed to get status")
            
            if i < 4:  # Don't sleep after last iteration
                time.sleep(3)
        
        return 0
        
    except Exception as e:
        print(f"✗ Error: {e}")
        return 1
    
    finally:
        collector.disconnect()

if __name__ == "__main__":
    sys.exit(main())
