#!/usr/bin/env python3
"""
MQTT Client for testing ur-qmi-ident requests and responses
Sends requests to 'direct_messaging/ur-qmi-ident/requests' and handles responses
"""

import json
import time
import threading
import paho.mqtt.client as mqtt
from typing import Dict, Any, Callable, Optional

class UQMIIdentClient:
    def __init__(self, broker_host: str = "0.0.0.0", broker_port: int = 1899):
        self.broker_host = broker_host
        self.broker_port = broker_port
        self.client = mqtt.Client()
        self.response_data = {}
        self.response_received = threading.Event()
        self.request_id = None
        
        # Setup MQTT callbacks
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message
        self.client.on_disconnect = self._on_disconnect
        
    def _on_connect(self, client, userdata, flags, rc):
        """Called when the client connects to the broker"""
        if rc == 0:
            print(f"✓ Connected to MQTT broker at {self.broker_host}:{self.broker_port}")
            # Subscribe to response topic
            client.subscribe("direct_messaging/ur-qmi-ident/responses")
            print("✓ Subscribed to response topic: direct_messaging/ur-qmi-ident/responses")
        else:
            print(f"✗ Failed to connect to MQTT broker. Return code: {rc}")
            
    def _on_message(self, client, userdata, msg):
        """Called when a message is received"""
        try:
            topic = msg.topic
            payload = msg.payload.decode('utf-8')
            print(f"\n📨 Received message on topic: {topic}")
            print(f"📄 Payload: {payload}")
            
            # Parse JSON response
            response = json.loads(payload)
            self.response_data = response
            self.response_received.set()
            
        except json.JSONDecodeError as e:
            print(f"✗ Failed to parse JSON response: {e}")
        except Exception as e:
            print(f"✗ Error handling message: {e}")
            
    def _on_disconnect(self, client, userdata, rc):
        """Called when the client disconnects"""
        print("🔌 Disconnected from MQTT broker")
        
    def connect(self) -> bool:
        """Connect to the MQTT broker"""
        try:
            print(f"🔗 Connecting to MQTT broker at {self.broker_host}:{self.broker_port}...")
            self.client.connect(self.broker_host, self.broker_port, 60)
            self.client.loop_start()
            
            # Wait for connection
            time.sleep(1)
            return True
        except Exception as e:
            print(f"✗ Failed to connect: {e}")
            return False
            
    def disconnect(self):
        """Disconnect from the MQTT broker"""
        self.client.loop_stop()
        self.client.disconnect()
        
    def send_request(self, method: str, params: Optional[Dict[str, Any]] = None, timeout: int = 10) -> Dict[str, Any]:
        """
        Send a JSON-RPC request to ur-qmi-ident
        
        Args:
            method: The RPC method to call (e.g., "get_devices", "scan_devices")
            params: Optional parameters for the method
            timeout: Timeout in seconds for response
            
        Returns:
            The response data as a dictionary
        """
        if params is None:
            params = {}
            
        # Generate unique request ID
        self.request_id = f"test_request_{int(time.time() * 1000)}"
        
        # Create JSON-RPC request
        request = {
            "jsonrpc": "2.0",
            "id": self.request_id,
            "method": method,
            "params": params
        }
        
        # Send request
        request_json = json.dumps(request, indent=2)
        print(f"\n📤 Sending request to: direct_messaging/ur-qmi-ident/requests")
        print(f"📋 Request:\n{request_json}")
        
        self.client.publish("direct_messaging/ur-qmi-ident/requests", request_json)
        
        # Wait for response
        print(f"⏳ Waiting for response (timeout: {timeout}s)...")
        
        if self.response_received.wait(timeout):
            print("✓ Response received!")
            response = self.response_data.copy()
            self.response_received.clear()
            return response
        else:
            print("✗ Timeout waiting for response")
            return {"error": "timeout", "id": self.request_id}

def main():
    """Main test function"""
    print("🚀 ur-qmi-ident MQTT Test Client")
    print("=" * 50)
    
    # Create client
    client = UQMIIdentClient()
    
    try:
        # Connect to broker
        if not client.connect():
            print("❌ Failed to connect to MQTT broker")
            print("💡 Make sure ur-qmi-ident is running with RPC enabled:")
            print("   sudo ./build/ur-qmi-ident -manager --rpc-config config/ur-rpc-config.json")
            return
            
        # Wait a moment for connection to stabilize
        time.sleep(2)
        
        # Test connection with a simple ping-like request
        print(f"\n{'='*20} Connection Test {'='*20}")
        print("📡 Testing connection with get_device_count...")
        
        response = client.send_request("get_device_count", {}, timeout=5)
        
        if response.get('error') == 'timeout':
            print("❌ Connection test failed - no response from ur-qmi-ident")
            print("💡 Make sure ur-qmi-ident is running and RPC is properly configured")
            return
        else:
            print("✅ Connection successful - ur-qmi-ident is responding!")
            
        # Test different requests
        test_requests = [
            {
                "name": "Get Device Count",
                "method": "get_device_count",
                "params": {}
            },
            {
                "name": "List Devices",
                "method": "list_devices",
                "params": {}
            },
            {
                "name": "Get Current Devices",
                "method": "get_devices",
                "params": {}
            },
            {
                "name": "Scan Devices",
                "method": "scan_devices", 
                "params": {}
            }
        ]
        
        for test in test_requests:
            print(f"\n{'='*20} {test['name']} {'='*20}")
            
            response = client.send_request(test['method'], test['params'])
            
            print(f"\n📊 Response Summary:")
            if 'error' in response:
                print(f"  ❌ Error: {response.get('error', 'Unknown error')}")
            else:
                print(f"  ✅ Success - ID: {response.get('id', 'Unknown')}")
                if 'result' in response:
                    result = response['result']
                    if isinstance(result, dict):
                        print(f"  📈 Result keys: {list(result.keys())}")
                    elif isinstance(result, list):
                        print(f"  📈 Result: {len(result)} items")
                        if result and isinstance(result[0], dict):
                            print(f"  📋 First item keys: {list(result[0].keys())}")
                    else:
                        print(f"  📈 Result type: {type(result).__name__}")
                        
            print(f"\n📄 Full Response:\n{json.dumps(response, indent=2)}")
            print("\n" + "-"*60)
            time.sleep(3)  # Wait between requests
            
    except KeyboardInterrupt:
        print("\n🛑 Test interrupted by user")
    except Exception as e:
        print(f"\n✗ Test failed with error: {e}")
    finally:
        # Cleanup
        print("\n🧹 Cleaning up...")
        client.disconnect()
        print("✅ Test completed")

if __name__ == "__main__":
    main()
