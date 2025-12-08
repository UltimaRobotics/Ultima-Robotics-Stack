
#!/usr/bin/env python3
"""
Test suite for get_wireless_config RPC functionality.
Tests the ability to retrieve the current wireless configuration.
"""

import paho.mqtt.client as mqtt
import json
import time
import sys
import uuid
from datetime import datetime

class GetWirelessConfigTest:
    def __init__(self, broker_host='127.0.0.1', broker_port=1899):
        self.broker_host = broker_host
        self.broker_port = broker_port
        self.client = mqtt.Client(client_id=f"test_get_wireless_config_{uuid.uuid4().hex[:8]}")
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
            
    def test_get_wireless_config(self):
        """Test getting wireless configuration"""
        transaction_id = str(uuid.uuid4())
        
        request = {
            "action": "get_wireless_config",
            "transaction_id": transaction_id,
            "params": {}
        }
        
        print(f"\n{'='*60}")
        print(f"TEST: Get Wireless Configuration")
        print(f"{'='*60}")
        print(f"Transaction ID: {transaction_id}")
        print(f"Request topic: {self.request_topic}")
        print(f"Request payload: {json.dumps(request, indent=2)}")
        
        self.response_received = None
        self.client.publish(self.request_topic, json.dumps(request), qos=0)
        print(f"✓ Request published")
        
        timeout = 15
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
        
        if self.response_received.get('transaction_id') != transaction_id:
            print(f"✗ Transaction ID mismatch")
            success = False
        else:
            print(f"✓ Transaction ID matches")
            
        if self.response_received.get('action') != 'get_wireless_config':
            print(f"✗ Action mismatch")
            success = False
        else:
            print(f"✓ Action matches")
            
        if 'error' in self.response_received and self.response_received['error']:
            print(f"✗ Response contains error: {self.response_received['error']}")
            success = False
        else:
            print(f"✓ No error in response")
            
        if 'result' in self.response_received:
            result = self.response_received['result']
            print(f"✓ Wireless configuration:")
            print(f"  - Enabled: {result.get('enabled', 'N/A')}")
            print(f"  - Mode: {result.get('mode', 'N/A')}")
            
            if 'sta_mode' in result:
                sta = result['sta_mode']
                print(f"  - STA Interface: {sta.get('interface', 'N/A')}")
                print(f"  - STA DHCP: {sta.get('dhcp_enabled', 'N/A')}")
            
            if 'ap_mode' in result:
                ap = result['ap_mode']
                print(f"  - AP SSID: {ap.get('ssid', 'N/A')}")
                print(f"  - AP Interface: {ap.get('interface', 'N/A')}")
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
    print("GET WIRELESS CONFIG RPC TEST")
    print("="*60)
    print(f"Start time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    
    tester = GetWirelessConfigTest()
    
    if not tester.connect():
        print("\n✗ TEST FAILED: Could not connect to broker")
        return 1
        
    time.sleep(1)
    
    success = tester.test_get_wireless_config()
    
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
