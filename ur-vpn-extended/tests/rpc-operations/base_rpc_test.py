#!/usr/bin/env python3
"""
Base RPC Test Class for ur-vpn-manager
Provides common functionality for testing RPC operations via MQTT
"""

import json
import time
import subprocess
import signal
import os
import sys
from typing import Dict, Any, Optional
import paho.mqtt.client as mqtt

class BaseRPCTest:
    """Base class for RPC operations testing"""
    
    def __init__(self, broker_host: str = "127.0.0.1", broker_port: int = 1899):
        self.broker_host = broker_host
        self.broker_port = broker_port
        self.client_id = f"test_client_{int(time.time())}"
        self.request_topic = "direct_messaging/ur-vpn-manager/requests"
        self.response_topic = "direct_messaging/ur-vpn-manager/responses"
        self.heartbeat_topic = "clients/ur-vpn-manager/heartbeat"
        
        # MQTT client with compatibility for different API versions
        try:
            # Check if CallbackAPIVersion is available (paho-mqtt >= 2.0.0)
            if hasattr(mqtt, 'CallbackAPIVersion'):
                self.client = mqtt.Client(
                    client_id=self.client_id,
                    callback_api_version=mqtt.CallbackAPIVersion.VERSION2
                )
                print("✓ Using MQTT client with API version 2.0")
            else:
                # Use older API version
                self.client = mqtt.Client(self.client_id)
                print("✓ Using MQTT client with legacy API")
        except Exception as e:
            # Ultimate fallback
            print(f"⚠️ MQTT client initialization warning: {e}")
            self.client = mqtt.Client(self.client_id)
        
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message
        
        # Response tracking
        self.response_received = False
        self.response_data = None
        self.response_timeout = 10  # seconds
        
        # ur-vpn-manager process
        self.manager_process = None
        
    def _on_connect(self, client, userdata, flags, rc, properties=None):
        """MQTT connection callback (compatible with both API versions)"""
        if rc == 0:
            print(f"✓ Connected to MQTT broker at {self.broker_host}:{self.broker_port}")
            client.subscribe(self.response_topic)
            print(f"✓ Subscribed to response topic: {self.response_topic}")
        else:
            print(f"✗ Failed to connect to MQTT broker: {rc}")
            
    def _on_message(self, client, userdata, msg, properties=None):
        """MQTT message callback (compatible with both API versions)"""
        try:
            payload = msg.payload.decode('utf-8')
            print(f"📨 Received message on {msg.topic}: {payload}")
            
            if msg.topic == self.response_topic:
                self.response_data = json.loads(payload)
                self.response_received = True
        except Exception as e:
            print(f"✗ Error processing message: {e}")
            
    def setup(self):
        """Setup test environment"""
        print("🔧 Setting up test environment...")
        
        # Connect to MQTT broker
        try:
            self.client.connect(self.broker_host, self.broker_port, 60)
            self.client.loop_start()
            time.sleep(1)  # Wait for connection
        except Exception as e:
            print(f"✗ Failed to connect to MQTT broker: {e}")
            return False
            
        # Start ur-vpn-manager if not already running
        if not self._is_manager_running():
            print("🚀 Starting ur-vpn-manager...")
            try:
                self.manager_process = subprocess.Popen([
                    "./build/ur-vpn-manager",
                    "-pkg_config", "config/master-config.json",
                    "-rpc_config", "config/ur-rpc-config.json"
                ], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                
                # Wait for manager to start
                time.sleep(3)
                
                if not self._is_manager_running():
                    print("✗ Failed to start ur-vpn-manager")
                    return False
                print("✓ ur-vpn-manager started successfully")
            except Exception as e:
                print(f"✗ Failed to start ur-vpn-manager: {e}")
                return False
        else:
            print("✓ ur-vpn-manager is already running")
            
        return True
        
    def teardown(self):
        """Cleanup test environment"""
        print("🧹 Cleaning up test environment...")
        
        # Stop MQTT client
        if self.client:
            self.client.loop_stop()
            self.client.disconnect()
            
        # Stop ur-vpn-manager if we started it
        if self.manager_process:
            print("🛑 Stopping ur-vpn-manager...")
            self.manager_process.terminate()
            try:
                self.manager_process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.manager_process.kill()
                self.manager_process.wait()
            print("✓ ur-vpn-manager stopped")
            
    def _is_manager_running(self):
        """Check if ur-vpn-manager process is running"""
        try:
            result = subprocess.run(['pgrep', '-f', 'ur-vpn-manager'], 
                                  capture_output=True, text=True)
            return result.returncode == 0
        except:
            return False
            
    def send_rpc_request(self, method: str, params: Dict[str, Any], 
                        request_id: Optional[str] = None) -> Dict[str, Any]:
        """Send RPC request and wait for response"""
        if request_id is None:
            request_id = f"test_{int(time.time())}"
            
        # Reset response tracking
        self.response_received = False
        self.response_data = None
        
        # Build RPC request
        request = {
            "jsonrpc": "2.0",
            "method": method,
            "params": params,
            "id": request_id
        }
        
        request_json = json.dumps(request)
        print(f"📤 Sending RPC request: {request_json}")
        
        # Send request
        result = self.client.publish(self.request_topic, request_json)
        if result.rc != 0:
            raise Exception(f"Failed to publish request: {result.rc}")
            
        # Wait for response
        start_time = time.time()
        while not self.response_received and (time.time() - start_time) < self.response_timeout:
            time.sleep(0.1)
            
        if not self.response_received:
            raise Exception("Timeout waiting for RPC response")
            
        return self.response_data
        
    def assert_success(self, response: Dict[str, Any], expected_success: bool = True):
        """Assert that response indicates success"""
        if 'result' in response:
            result = response['result']
            if 'success' in result:
                assert result['success'] == expected_success, \
                    f"Expected success={expected_success}, got success={result['success']}"
                if not expected_success and 'error' in result:
                    print(f"Expected error: {result['error']}")
            else:
                print("Response doesn't contain 'success' field, assuming success")
        else:
            print("Response doesn't contain 'result' field, checking for 'error'")
            assert 'error' not in response, f"Unexpected error in response: {response}"
            
    def assert_contains_fields(self, response: Dict[str, Any], *fields):
        """Assert that response contains specified fields"""
        if 'result' in response:
            result = response['result']
            for field in fields:
                assert field in result, f"Response missing required field: {field}"
                
    def wait_for_heartbeat(self, timeout: int = 35) -> bool:
        """Wait for heartbeat message to verify manager is alive"""
        heartbeat_received = False
        
        def on_heartbeat(client, userdata, msg):
            nonlocal heartbeat_received
            if msg.topic == self.heartbeat_topic:
                heartbeat_received = True
                print(f"💓 Heartbeat received: {msg.payload.decode('utf-8')}")
                
        # Temporarily subscribe to heartbeat
        self.client.message_callback_add(self.heartbeat_topic, on_heartbeat)
        self.client.subscribe(self.heartbeat_topic)
        
        start_time = time.time()
        while not heartbeat_received and (time.time() - start_time) < timeout:
            time.sleep(0.5)
            
        # Unsubscribe from heartbeat
        self.client.unsubscribe(self.heartbeat_topic)
        self.client.message_callback_remove(self.heartbeat_topic)
        
        return heartbeat_received
        
    def run_test(self, test_func):
        """Run a test function with setup and teardown"""
        print(f"\n🧪 Running test: {test_func.__name__}")
        try:
            if not self.setup():
                print("✗ Test setup failed")
                return False
                
            test_func()
            print(f"✅ Test {test_func.__name__} passed")
            return True
            
        except Exception as e:
            print(f"❌ Test {test_func.__name__} failed: {e}")
            return False
        finally:
            self.teardown()
