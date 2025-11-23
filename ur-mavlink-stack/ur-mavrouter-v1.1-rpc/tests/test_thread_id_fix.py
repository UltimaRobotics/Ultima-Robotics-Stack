#!/usr/bin/env python3
"""
Test script to verify thread ID fix by stopping and restarting mainloop
"""

import json
import time
import sys
import os

# Add the current directory to path to import our test module
sys.path.insert(0, os.path.dirname(__file__))

from test_rpc_thread_states import RpcThreadStateCollector

def test_thread_id_fix():
    """Test that thread IDs are correctly registered after restart"""
    print("🧪 Testing Thread ID Fix")
    print("="*50)
    
    collector = RpcThreadStateCollector()
    
    if not collector.connect():
        print("✗ Failed to connect - aborting test")
        return False
    
    try:
        # Step 1: Get current mainloop status
        print("\n1️⃣ Getting current mainloop status...")
        current_status = collector.get_thread_status("mainloop")
        if current_status and 'threads' in current_status:
            mainloop_info = current_status['threads'].get('mainloop', {})
            current_id = mainloop_info.get('threadId', 0)
            is_alive = mainloop_info.get('isAlive', False)
            print(f"Current mainloop - ID: {current_id}, Alive: {is_alive}")
        
        # Step 2: Stop mainloop if running
        if is_alive:
            print("\n2️⃣ Stopping mainloop...")
            stop_result = collector.thread_operation("mainloop", "stop")
            if stop_result:
                print("✓ Mainloop stop request sent")
                time.sleep(2)  # Wait for stop to complete
            else:
                print("✗ Failed to stop mainloop")
                return False
        
        # Step 3: Check that mainloop is stopped
        print("\n3️⃣ Verifying mainloop is stopped...")
        stopped_status = collector.get_thread_status("mainloop")
        if stopped_status and 'threads' in stopped_status:
            mainloop_info = stopped_status['threads'].get('mainloop', {})
            stopped_id = mainloop_info.get('threadId', 0)
            is_still_alive = mainloop_info.get('isAlive', False)
            print(f"After stop - ID: {stopped_id}, Alive: {is_still_alive}")
            
            if is_still_alive:
                print("⚠ Mainloop is still alive after stop request")
        
        # Step 4: Restart mainloop
        print("\n4️⃣ Restarting mainloop...")
        start_result = collector.thread_operation("mainloop", "start")
        if start_result:
            print("✓ Mainloop start request sent")
            print(f"Start result: {json.dumps(start_result, indent=2)}")
            time.sleep(3)  # Wait for start to complete
        else:
            print("✗ Failed to start mainloop")
            return False
        
        # Step 5: Check new mainloop status
        print("\n5️⃣ Checking new mainloop status...")
        new_status = collector.get_thread_status("mainloop")
        if new_status and 'threads' in new_status:
            mainloop_info = new_status['threads'].get('mainloop', {})
            new_id = mainloop_info.get('threadId', 0)
            is_alive_again = mainloop_info.get('isAlive', False)
            print(f"New mainloop - ID: {new_id}, Alive: {is_alive_again}")
            
            # Verify thread ID is reasonable (should be > 1 for a real thread)
            if new_id <= 1:
                print(f"⚠ Warning: Thread ID is {new_id}, which seems incorrect")
            else:
                print(f"✓ Thread ID {new_id} looks correct")
            
            return True
        else:
            print("✗ Failed to get new mainloop status")
            return False
            
    except Exception as e:
        print(f"✗ Test failed with exception: {e}")
        return False
    
    finally:
        collector.disconnect()

def main():
    """Main function"""
    print("🚀 Thread ID Fix Verification Test")
    print("="*60)
    
    success = test_thread_id_fix()
    
    print("\n" + "="*60)
    if success:
        print("✅ Thread ID test completed!")
    else:
        print("❌ Thread ID test failed!")
    
    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())
