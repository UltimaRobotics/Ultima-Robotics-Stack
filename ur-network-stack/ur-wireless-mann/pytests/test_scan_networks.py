
#!/usr/bin/env python3
"""
Test suite for scan_networks RPC functionality.
Tests the ability to scan for wireless networks on a specific interface.
"""

import paho.mqtt.client as mqtt
import json
import time
import sys
import uuid
from datetime import datetime

class ScanNetworksTest:
    def __init__(self, broker_host='127.0.0.1', broker_port=1899):
        self.broker_host = broker_host
        self.broker_port = broker_port
        self.client = mqtt.Client(client_id=f"test_scan_networks_{uuid.uuid4().hex[:8]}")
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
            
            # Check if this is async acknowledgment or final result
            if payload.get('result', {}).get('status') == 'processing':
                print(f"  Status: Scan started (async acknowledgment)")
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
            
    def test_scan_networks(self, interface_name="wlp4s0"):
        """Test scanning for wireless networks"""
        transaction_id = str(uuid.uuid4())
        
        request = {
            "action": "scan_networks",
            "transaction_id": transaction_id,
            "params": {
                "interface": interface_name
            }
        }
        
        print(f"\n{'='*60}")
        print(f"TEST: Scan Wireless Networks")
        print(f"{'='*60}")
        print(f"Transaction ID: {transaction_id}")
        print(f"Interface: {interface_name}")
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
        
        print(f"\n✓ Async acknowledgment received, waiting for scan results...")
        
        # Wait for actual scan results (can take longer)
        timeout = 40
        start_time = time.time()
        while self.async_response_received is None and (time.time() - start_time) < timeout:
            time.sleep(0.1)
            
        if self.async_response_received is None:
            print(f"\n✗ No scan results received within {timeout}s")
            return False
            
        # Validate response
        print(f"\n{'='*60}")
        print("RESPONSE VALIDATION")
        print(f"{'='*60}")
        
        success = True
        response = self.async_response_received
        
        # Check transaction ID matches
        if response.get('transaction_id') != transaction_id:
            print(f"✗ Transaction ID mismatch")
            success = False
        else:
            print(f"✓ Transaction ID matches")
            
        # Check action matches
        if response.get('action') != 'scan_networks':
            print(f"✗ Action mismatch")
            success = False
        else:
            print(f"✓ Action matches")
            
        # Check for error
        if 'error' in response and response['error']:
            print(f"✗ Response contains error: {response['error']}")
            success = False
        else:
            print(f"✓ No error in response")
            
        # Check result structure
        if 'result' in response:
            result = response['result']
            if 'networks' in result:
                networks = result['networks']
                print(f"✓ Found {len(networks)} network(s)")
                for net in networks[:5]:  # Show first 5 networks
                    ssid = net.get('ssid', 'unknown')
                    signal = net.get('signal_strength', 'N/A')
                    security = net.get('security', 'N/A')
                    print(f"  - SSID: {ssid}, Signal: {signal}, Security: {security}")
                if len(networks) > 5:
                    print(f"  ... and {len(networks) - 5} more")
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
    print("SCAN NETWORKS RPC TEST")
    print("="*60)
    print(f"Start time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    
    # You can change the interface name here
    interface_name = sys.argv[1] if len(sys.argv) > 1 else "wlp4s0"
    
    tester = ScanNetworksTest()
    
    if not tester.connect():
        print("\n✗ TEST FAILED: Could not connect to broker")
        return 1
        
    time.sleep(1)
    
    success = tester.test_scan_networks(interface_name)
    
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
