
#!/usr/bin/env python3
"""
Test suite for test_connection RPC functionality.
Tests the ability to test connectivity to a specific network.
"""

import paho.mqtt.client as mqtt
import json
import time
import sys
import uuid
from datetime import datetime

class TestConnectionTest:
    def __init__(self, broker_host='127.0.0.1', broker_port=1899):
        self.broker_host = broker_host
        self.broker_port = broker_port
        self.client = mqtt.Client(client_id=f"test_connection_{uuid.uuid4().hex[:8]}")
        self.request_topic = "direct_messaging/ur-wireless-mann/requests"
        self.response_topic = "direct_messaging/ur-wireless-mann/responses"
        self.response_received = None
        self.connected = False
        
    def on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            print(f"✓ Connected to broker at {self.broker_host}:{self.broker_port}")
            self.connected = True
            client.subscribe(self.response_topic)
            print(f"✓ Subscribed to response topic: {self.response_topic}")
        else:
            print(f"✗ Connection failed with code {rc}")
            
    def on_message(self, client, userdata, msg):
        try:
            payload = json.loads(msg.payload.decode())
            print(f"\n✓ Response received on {msg.topic}")
            print(f"  Payload: {json.dumps(payload, indent=2)}")
            self.response_received = payload
        except Exception as e:
            print(f"✗ Error parsing response: {e}")
            
    def connect(self):
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        
        try:
            self.client.connect(self.broker_host, self.broker_port, 60)
            self.client.loop_start()
            
            timeout = 10
            start_time = time.time()
            while not self.connected and (time.time() - start_time) < timeout:
                time.sleep(0.1)
                
            if not self.connected:
                print("✗ Failed to connect within timeout")
                return False
                
            return True
        except Exception as e:
            print(f"✗ Connection error: {e}")
            return False
            
    def test_connection(self, interface_name="wlan0", ssid="TestNetwork"):
        """Test connection to a network"""
        transaction_id = str(uuid.uuid4())
        
        request = {
            "action": "test_connection",
            "transaction_id": transaction_id,
            "params": {
                "interface": interface_name,
                "ssid": ssid
            }
        }
        
        print(f"\n{'='*60}")
        print(f"TEST: Test Connection to Network")
        print(f"{'='*60}")
        print(f"Transaction ID: {transaction_id}")
        print(f"Interface: {interface_name}")
        print(f"SSID: {ssid}")
        print(f"Request topic: {self.request_topic}")
        print(f"Request payload: {json.dumps(request, indent=2)}")
        
        self.response_received = None
        self.client.publish(self.request_topic, json.dumps(request), qos=0)
        print(f"✓ Request published")
        
        # Connection testing can take longer
        timeout = 30
        start_time = time.time()
        while self.response_received is None and (time.time() - start_time) < timeout:
            time.sleep(0.1)
            
        if self.response_received is None:
            print(f"\n✗ No response received within {timeout}s")
            return False
            
        # Validate response
        print(f"\n{'='*60}")
        print("RESPONSE VALIDATION")
        print(f"{'='*60}")
        
        success = True
        
        # Check transaction ID matches
        if self.response_received.get('transaction_id') != transaction_id:
            print(f"✗ Transaction ID mismatch")
            success = False
        else:
            print(f"✓ Transaction ID matches")
            
        # Check action matches
        if self.response_received.get('action') != 'test_connection':
            print(f"✗ Action mismatch")
            success = False
        else:
            print(f"✓ Action matches")
            
        # Check for error
        if 'error' in self.response_received:
            print(f"✗ Response contains error: {self.response_received['error']}")
            success = False
        else:
            print(f"✓ No error in response")
            
        # Check result structure
        if 'result' in self.response_received:
            result = self.response_received['result']
            print(f"✓ Connection test result:")
            print(f"  - Success: {result.get('success', 'N/A')}")
            print(f"  - Connected: {result.get('connected', 'N/A')}")
            if 'latency_ms' in result:
                print(f"  - Latency: {result['latency_ms']} ms")
            if 'message' in result:
                print(f"  - Message: {result['message']}")
        else:
            print(f"✗ No 'result' field in response")
            success = False
            
        return success
        
    def disconnect(self):
        self.client.loop_stop()
        self.client.disconnect()
        print("\n✓ Disconnected from broker")
        
def main():
    print("="*60)
    print("TEST CONNECTION RPC TEST")
    print("="*60)
    print(f"Start time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    
    # You can change these parameters
    interface_name = sys.argv[1] if len(sys.argv) > 1 else "wlan0"
    ssid = sys.argv[2] if len(sys.argv) > 2 else "TestNetwork"
    
    tester = TestConnectionTest()
    
    if not tester.connect():
        print("\n✗ TEST FAILED: Could not connect to broker")
        return 1
        
    time.sleep(1)
    
    success = tester.test_connection(interface_name, ssid)
    
    time.sleep(1)
    tester.disconnect()
    
    print("\n" + "="*60)
    if success:
        print("✓ TEST PASSED")
        print("="*60)
        return 0
    else:
        print("✗ TEST FAILED")
        print("="*60)
        return 1
        
if __name__ == "__main__":
    sys.exit(main())
