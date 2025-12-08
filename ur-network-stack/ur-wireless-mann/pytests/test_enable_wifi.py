
#!/usr/bin/env python3
"""
Test suite for enable_wifi RPC functionality.
Tests the ability to enable WiFi on the system.
"""

import paho.mqtt.client as mqtt
import json
import time
import sys
import uuid
from datetime import datetime

class EnableWifiTest:
    def __init__(self, broker_host='127.0.0.1', broker_port=1899):
        self.broker_host = broker_host
        self.broker_port = broker_port
        self.client = mqtt.Client(client_id=f"test_enable_wifi_{uuid.uuid4().hex[:8]}")
        self.request_topic = "direct_messaging/ur-wireless-mann/requests"
        self.response_topic = "direct_messaging/ur-wireless-mann/responses"
        self.response_received = None
        self.async_response_received = None
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
            print(f"  Transaction ID: {payload.get('transaction_id', 'N/A')}")
            
            if payload.get('result', {}).get('status') == 'processing':
                print(f"  Status: WiFi enabling (async acknowledgment)")
                self.response_received = payload
            else:
                print(f"  Status: Final result")
                print(f"  Payload: {json.dumps(payload, indent=2)}")
                self.async_response_received = payload
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
            
    def test_enable_wifi(self):
        """Test enabling WiFi"""
        transaction_id = str(uuid.uuid4())
        
        request = {
            "action": "enable_wifi",
            "transaction_id": transaction_id,
            "params": {}
        }
        
        print(f"\n{'='*60}")
        print(f"TEST: Enable WiFi")
        print(f"{'='*60}")
        print(f"Transaction ID: {transaction_id}")
        print(f"Request topic: {self.request_topic}")
        print(f"Request payload: {json.dumps(request, indent=2)}")
        
        self.response_received = None
        self.async_response_received = None
        self.client.publish(self.request_topic, json.dumps(request), qos=0)
        print(f"✓ Request published")
        
        # Wait for async acknowledgment
        timeout = 10
        start_time = time.time()
        while self.response_received is None and (time.time() - start_time) < timeout:
            time.sleep(0.1)
            
        if self.response_received is None:
            print(f"\n✗ No acknowledgment received within {timeout}s")
            return False
        
        print(f"\n✓ Async acknowledgment received, waiting for WiFi enable completion...")
        
        # Wait for actual completion
        timeout = 20
        start_time = time.time()
        while self.async_response_received is None and (time.time() - start_time) < timeout:
            time.sleep(0.1)
            
        if self.async_response_received is None:
            print(f"\n✗ No completion response received within {timeout}s")
            return False
            
        # Validate response
        print(f"\n{'='*60}")
        print("RESPONSE VALIDATION")
        print(f"{'='*60}")
        
        success = True
        response = self.async_response_received
        
        if response.get('transaction_id') != transaction_id:
            print(f"✗ Transaction ID mismatch")
            success = False
        else:
            print(f"✓ Transaction ID matches")
            
        if response.get('action') != 'enable_wifi':
            print(f"✗ Action mismatch")
            success = False
        else:
            print(f"✓ Action matches")
            
        if 'error' in response and response['error']:
            print(f"✗ Response contains error: {response['error']}")
            success = False
        else:
            print(f"✓ No error in response")
            
        if 'result' in response:
            result = response['result']
            print(f"✓ WiFi enable result:")
            print(f"  - Status: {result.get('status', 'N/A')}")
            print(f"  - Message: {result.get('message', 'N/A')}")
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
    print("ENABLE WIFI RPC TEST")
    print("="*60)
    print(f"Start time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    
    tester = EnableWifiTest()
    
    if not tester.connect():
        print("\n✗ TEST FAILED: Could not connect to broker")
        return 1
        
    time.sleep(1)
    
    success = tester.test_enable_wifi()
    
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
