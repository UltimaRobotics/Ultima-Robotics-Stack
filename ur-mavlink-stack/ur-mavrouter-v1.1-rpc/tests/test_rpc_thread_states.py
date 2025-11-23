#!/usr/bin/env python3
"""
Python test for collecting thread states, mainloop, and extensions via RPC requests
to ur-mavrouter in the Ultima Robotics Stack.

This test connects to the MQTT broker and sends JSON-RPC requests to:
1. Get status of all threads
2. Get status of specific threads (mainloop, extensions)
3. Start/stop threads via RPC operations
4. Monitor thread state changes
"""

import json
import time
import threading
import socket
import sys
from typing import Dict, Any, Optional, List
from dataclasses import dataclass
from datetime import datetime

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("Error: paho-mqtt not installed. Install with: pip install paho-mqtt")
    sys.exit(1)


@dataclass
class ThreadState:
    """Data class representing thread state information"""
    thread_name: str
    thread_id: int
    state: int
    is_alive: bool
    attachment_id: str
    timestamp: datetime


class RpcThreadStateCollector:
    """RPC client for collecting thread states from ur-mavrouter"""
    
    def __init__(self, broker_host: str = "127.0.0.1", broker_port: int = 1899, 
                 client_id: str = "test_rpc_client"):
        self.broker_host = broker_host
        self.broker_port = broker_port
        self.client_id = client_id
        self.request_topic = f"direct_messaging/ur-mavrouter/requests"
        self.response_topic = f"direct_messaging/ur-mavrouter/responses"  # Fixed: use ur-mavrouter's response topic
        
        # MQTT client
        self.client = mqtt.Client(client_id=client_id)
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message
        
        # Response tracking
        self.pending_requests = {}
        self.response_timeout = 10.0  # seconds
        self.request_counter = 0
        
        # Connection state
        self.connected = False
        self.response_received = threading.Event()
        
    def _on_connect(self, client, userdata, flags, rc):
        """MQTT connection callback"""
        if rc == 0:
            print(f"✓ Connected to MQTT broker at {self.broker_host}:{self.broker_port}")
            self.connected = True
            # Subscribe to response topic
            client.subscribe(self.response_topic)
            print(f"✓ Subscribed to response topic: {self.response_topic}")
        else:
            print(f"✗ Failed to connect to MQTT broker (return code: {rc})")
            self.connected = False
    
    def _on_message(self, client, userdata, msg):
        """MQTT message callback"""
        try:
            payload = msg.payload.decode('utf-8')
            response = json.loads(payload)
            
            # Extract request ID from response
            request_id = response.get('id')
            if request_id and request_id in self.pending_requests:
                self.pending_requests[request_id] = response
                self.response_received.set()
                print(f"✓ Received response for request {request_id}")
            else:
                print(f"⚠ Received message for unknown request ID: {request_id}")
            
        except json.JSONDecodeError as e:
            print(f"✗ Failed to decode JSON response: {e}")
        except Exception as e:
            print(f"✗ Error handling message: {e}")
    
    def connect(self) -> bool:
        """Connect to MQTT broker"""
        try:
            self.client.connect(self.broker_host, self.broker_port, 60)
            self.client.loop_start()
            
            # Wait for connection
            for i in range(50):  # 5 seconds timeout
                if self.connected:
                    return True
                time.sleep(0.1)
            
            print("✗ Connection timeout")
            return False
            
        except Exception as e:
            print(f"✗ Failed to connect: {e}")
            return False
    
    def disconnect(self):
        """Disconnect from MQTT broker"""
        if self.connected:
            self.client.loop_stop()
            self.client.disconnect()
            self.connected = False
            print("✓ Disconnected from MQTT broker")
    
    def _send_rpc_request(self, method: str, params: Dict[str, Any], 
                         timeout: float = 5.0) -> Optional[Dict[str, Any]]:
        """Send JSON-RPC request and wait for response"""
        if not self.connected:
            print("✗ Not connected to MQTT broker")
            return None
        
        # Generate unique request ID
        self.request_counter += 1
        request_id = f"{int(time.time() * 1000)}-{self.request_counter}"
        
        # Create JSON-RPC request
        request = {
            "jsonrpc": "2.0",
            "id": request_id,
            "method": method,
            "params": params
        }
        
        # Send request
        request_str = json.dumps(request)
        result = self.client.publish(self.request_topic, request_str)
        
        if result.rc != 0:
            print(f"✗ Failed to publish request (return code: {result.rc})")
            return None
        
        print(f"→ Sent {method} request (ID: {request_id})")
        
        # Wait for response
        self.pending_requests[request_id] = None
        self.response_received.clear()
        
        if self.response_received.wait(timeout):
            response = self.pending_requests.get(request_id)
            del self.pending_requests[request_id]
            return response
        else:
            print(f"✗ Timeout waiting for response to request {request_id}")
            del self.pending_requests[request_id]
            return None
    
    def get_all_thread_status(self) -> Optional[Dict[str, Any]]:
        """Get status of all threads"""
        params = {}
        response = self._send_rpc_request("thread_status", params)
        
        if response and 'result' in response:
            return response['result']
        elif response and 'error' in response:
            print(f"✗ RPC error: {response['error']}")
        else:
            print("✗ Invalid or missing response")
        
        return None
    
    def get_thread_status(self, thread_name: str) -> Optional[Dict[str, Any]]:
        """Get status of a specific thread"""
        params = {"thread_name": thread_name}
        response = self._send_rpc_request("thread_status", params)
        
        if response and 'result' in response:
            return response['result']
        elif response and 'error' in response:
            print(f"✗ RPC error: {response['error']}")
        else:
            print("✗ Invalid or missing response")
        
        return None
    
    def get_runtime_info(self) -> Optional[Dict[str, Any]]:
        """Get comprehensive runtime information including thread types and nature"""
        params = {}
        response = self._send_rpc_request("runtime-info", params)
        
        if response and 'result' in response:
            return response['result']
        elif response and 'error' in response:
            print(f"✗ RPC error: {response['error']}")
        else:
            print("✗ Invalid or missing response")
        
        return None
    
    def thread_operation(self, thread_name: str, operation: str) -> Optional[Dict[str, Any]]:
        """Perform operation on a thread (start, stop, pause, resume, restart, status)"""
        params = {
            "thread_name": thread_name,
            "operation": operation
        }
        response = self._send_rpc_request("thread_operation", params)
        
        if response and 'result' in response:
            return response['result']
        elif response and 'error' in response:
            print(f"✗ RPC error: {response['error']}")
        else:
            print("✗ Invalid or missing response")
        
        return None
    
    def parse_thread_states(self, result: Dict[str, Any]) -> List[ThreadState]:
        """Parse thread states from RPC response"""
        threads = []
        
        if 'threads' in result:
            for thread_name, thread_info in result['threads'].items():
                state = ThreadState(
                    thread_name=thread_name,
                    thread_id=thread_info.get('threadId', 0),
                    state=thread_info.get('state', 0),
                    is_alive=thread_info.get('isAlive', False),
                    attachment_id=thread_info.get('attachmentId', ''),
                    timestamp=datetime.now()
                )
                threads.append(state)
        
        return threads
    
    def print_thread_states(self, threads: List[ThreadState]):
        """Print thread states in a formatted table"""
        print("\n" + "="*80)
        print("THREAD STATES")
        print("="*80)
        print(f"{'Thread Name':<20} {'ID':<8} {'State':<8} {'Alive':<6} {'Attachment ID':<15} {'Timestamp'}")
        print("-"*80)
        
        for thread in threads:
            state_str = self._state_to_string(thread.state)
            alive_str = "Yes" if thread.is_alive else "No"
            timestamp_str = thread.timestamp.strftime("%H:%M:%S")
            
            print(f"{thread.thread_name:<20} {thread.thread_id:<8} {state_str:<8} "
                  f"{alive_str:<6} {thread.attachment_id:<15} {timestamp_str}")
        
        print("="*80)
    
    def _state_to_string(self, state: int) -> str:
        """Convert thread state integer to string"""
        state_map = {
            0: "Stopped",
            1: "Running", 
            2: "Paused",
            3: "Error",
            4: "Starting"
        }
        return state_map.get(state, f"Unknown({state})")
    
    def monitor_thread_states(self, duration: int = 30, interval: int = 5):
        """Monitor thread states for a specified duration"""
        print(f"\n🔍 Monitoring thread states for {duration} seconds (interval: {interval}s)")
        print("Press Ctrl+C to stop monitoring early\n")
        
        start_time = time.time()
        
        try:
            while time.time() - start_time < duration:
                print(f"\n--- {datetime.now().strftime('%Y-%m-%d %H:%M:%S')} ---")
                
                # Get all thread status
                result = self.get_all_thread_status()
                if result:
                    threads = self.parse_thread_states(result)
                    self.print_thread_states(threads)
                else:
                    print("✗ Failed to get thread status")
                
                # Wait for next iteration
                time.sleep(interval)
                
        except KeyboardInterrupt:
            print("\n⚠ Monitoring stopped by user")
        
        print("✓ Monitoring completed")


def test_basic_thread_operations():
    """Test basic thread operations"""
    print("\n🧪 Testing Basic Thread Operations")
    print("=" * 50)
    
    collector = RpcThreadStateCollector()
    
    if not collector.connect():
        print("✗ Failed to connect to MQTT broker")
        return False
    
    try:
        # Test 1: Get all thread status
        print("1️⃣ Testing get_all_thread_status...")
        result = collector.get_all_thread_status()
        if result:
            print("✓ get_all_thread_status test passed")
            print(f"Status: {result.get('message', 'Unknown')}")
        else:
            print("✗ get_all_thread_status test failed")
            return False
    
    # Test 2: Get runtime info (new feature)
        print("\n2️⃣ Testing get_runtime_info...")
        runtime_info = collector.get_runtime_info()
        if runtime_info:
            print("✓ get_runtime_info test passed")
            print(f"Runtime Status: {runtime_info.get('status', 'Unknown')}")
            print(f"Total Threads: {runtime_info.get('total_threads', 0)}")
            print(f"Running Threads: {runtime_info.get('running_threads', 0)}")
            
            # Display thread details with nature and type
            threads = runtime_info.get('threads', [])
            if threads:
                print("\n📋 Thread Details:")
                print("-" * 80)
                print(f"{'Name':<20} {'Nature':<12} {'Type':<15} {'State':<10} {'Alive':<6} {'Critical':<8}")
                print("-" * 80)
                for thread in threads:
                    print(f"{thread['name']:<20} {thread['nature']:<12} {thread['type']:<15} "
                          f"{thread['state_name']:<10} {str(thread['is_alive']):<6} {str(thread['critical']):<8}")
        else:
            print("✗ get_runtime_info test failed")
            return False
        
        # Test 3: Get mainloop status
        print("\n3️⃣ Testing get_thread_status for mainloop...")
        mainloop_result = collector.get_thread_status("mainloop")
        if mainloop_result:
            print("✓ get_thread_status(mainloop) test passed")
            print(f"Mainloop status: {json.dumps(mainloop_result, indent=2)}")
        else:
            print("✗ get_thread_status(mainloop) test failed")
        
        return True
        
    except Exception as e:
        print(f"✗ Test failed with exception: {e}")
        return False
    
    finally:
        collector.disconnect()


def test_extension_monitoring():
    """Test extension monitoring and control"""
    print("\n🧪 Testing Extension Monitoring")
    print("="*50)
    
    collector = RpcThreadStateCollector()
    
    if not collector.connect():
        print("✗ Failed to connect - aborting test")
        return False
    
    try:
        # Get all thread status to see extensions
        print("\n🔍 Checking for extension threads...")
        result = collector.get_all_thread_status()
        
        if result:
            threads = collector.parse_thread_states(result)
            extension_threads = [t for t in threads if 'extension' in t.thread_name.lower() 
                               or 'udp' in t.thread_name.lower() 
                               or 'tcp' in t.thread_name.lower()]
            
            if extension_threads:
                print(f"✓ Found {len(extension_threads)} extension threads:")
                for ext_thread in extension_threads:
                    print(f"  - {ext_thread.thread_name} (ID: {ext_thread.thread_id}, "
                          f"Alive: {ext_thread.is_alive})")
                
                # Test getting status of each extension
                for ext_thread in extension_threads:
                    print(f"\n🔍 Getting detailed status for {ext_thread.thread_name}...")
                    ext_result = collector.get_thread_status(ext_thread.thread_name)
                    if ext_result:
                        print(f"✓ Status retrieved for {ext_thread.thread_name}")
                    else:
                        print(f"✗ Failed to get status for {ext_thread.thread_name}")
            else:
                print("ℹ No extension threads found (they may not be started yet)")
                
                # Try to check if there are any threads that could be extensions
                all_threads = [t.thread_name for t in threads]
                print(f"Available threads: {all_threads}")
        
        return True
        
    except Exception as e:
        print(f"✗ Extension test failed with exception: {e}")
        return False
    
    finally:
        collector.disconnect()


def interactive_mode():
    """Interactive mode for manual testing"""
    print("\n🎮 Interactive RPC Test Mode")
    print("="*50)
    print("Available commands:")
    print("  all          - Get all thread status")
    print("  mainloop     - Get mainloop status")
    print("  <thread>     - Get specific thread status")
    print("  start <name> - Start a thread")
    print("  stop <name>  - Stop a thread")
    print("  monitor      - Monitor thread states for 60 seconds")
    print("  quit         - Exit interactive mode")
    print("="*50)
    
    collector = RpcThreadStateCollector()
    
    if not collector.connect():
        print("✗ Failed to connect - aborting interactive mode")
        return
    
    try:
        while True:
            try:
                cmd = input("\n> ").strip().lower()
                
                if cmd == "quit" or cmd == "q":
                    break
                elif cmd == "all":
                    result = collector.get_all_thread_status()
                    if result:
                        threads = collector.parse_thread_states(result)
                        collector.print_thread_states(threads)
                elif cmd == "mainloop":
                    result = collector.get_thread_status("mainloop")
                    if result:
                        print(json.dumps(result, indent=2))
                elif cmd == "monitor":
                    collector.monitor_thread_states(duration=60, interval=5)
                elif cmd.startswith("start "):
                    thread_name = cmd[6:].strip()
                    result = collector.thread_operation(thread_name, "start")
                    if result:
                        print(json.dumps(result, indent=2))
                elif cmd.startswith("stop "):
                    thread_name = cmd[5:].strip()
                    result = collector.thread_operation(thread_name, "stop")
                    if result:
                        print(json.dumps(result, indent=2))
                elif cmd and not cmd.startswith(" "):
                    # Treat as thread name
                    result = collector.get_thread_status(cmd)
                    if result:
                        print(json.dumps(result, indent=2))
                    else:
                        print(f"✗ No status returned for thread '{cmd}'")
                        
            except EOFError:
                break
            except KeyboardInterrupt:
                print("\nUse 'quit' to exit")
                
    finally:
        collector.disconnect()
        print("\n✓ Interactive mode ended")


def main():
    """Main test function"""
    print("🚀 RPC Thread State Test Suite for Ultima Robotics Stack")
    print("="*60)
    
    if len(sys.argv) > 1:
        mode = sys.argv[1].lower()
        if mode == "interactive":
            interactive_mode()
            return
        elif mode == "monitor":
            collector = RpcThreadStateCollector()
            if collector.connect():
                duration = int(sys.argv[2]) if len(sys.argv) > 2 else 60
                collector.monitor_thread_states(duration=duration)
            collector.disconnect()
            return
        elif mode == "extensions":
            test_extension_monitoring()
            return
    
    # Run basic tests
    success = True
    
    print("\n📋 Running test suite...")
    
    # Test 1: Basic operations
    if not test_basic_thread_operations():
        success = False
    
    # Test 2: Extension monitoring
    if not test_extension_monitoring():
        success = False
    
    # Summary
    print("\n" + "="*60)
    if success:
        print("✅ All tests completed successfully!")
    else:
        print("❌ Some tests failed!")
    
    print("\nUsage examples:")
    print("  python3 test_rpc_thread_states.py interactive  # Interactive mode")
    print("  python3 test_rpc_thread_states.py monitor 120  # Monitor for 2 minutes")
    print("  python3 test_rpc_thread_states.py extensions   # Test extension monitoring")
    print("="*60)


if __name__ == "__main__":
    main()
