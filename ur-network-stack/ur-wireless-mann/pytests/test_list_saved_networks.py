
#!/usr/bin/env python3
"""
Test suite for list_saved_networks RPC functionality.
Tests the ability to list all saved network credentials.
"""

import paho.mqtt.client as mqtt
import json
import time
import sys
import uuid
from datetime import datetime

class ListSavedNetworksTest:
    def __init__(self, broker_host='127.0.0.1', broker_port=1899):
        self.broker_host = broker_host
        self.broker_port = broker_port
        self.client = mqtt.Client(client_id=f"test_list_saved_networks_{uuid.uuid4().hex[:8]}")
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
            
    def test_list_saved_networks(self):
        """Test listing saved networks"""
        transaction_id = str(uuid.uuid4())
        
        request = {
            "action": "list_saved_networks",
            "transaction_id": transaction_id,
            "params": {}
        }
        
        print(f"\n{'='*60}")
        print(f"TEST: List Saved Networks")
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
            
        if self.response_received.get('action') != 'list_saved_networks':
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
            if 'networks' in result:
                networks = result['networks']
                print(f"✓ Found {len(networks)} saved network(s)")
                for net in networks:
                    ssid = net.get('ssid', 'unknown')
                    priority = net.get('priority', 'N/A')
                    security = net.get('security', 'N/A')
                    print(f"  - SSID: {ssid}, Priority: {priority}, Security: {security}")
            else:
                print(f"✗ No 'networks' field in result")
                success = False
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
    print("LIST SAVED NETWORKS RPC TEST")
    print("="*60)
    print(f"Start time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    
    tester = ListSavedNetworksTest()
    
    if not tester.connect():
        print("\n✗ TEST FAILED: Could not connect to broker")
        return 1
        
    time.sleep(1)
    
    success = tester.test_list_saved_networks()
    
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
