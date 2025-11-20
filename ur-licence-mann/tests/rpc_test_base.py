#!/usr/bin/env python3
"""
Base utilities for RPC testing of ur-licence-mann
"""

import json
import time
import uuid
from typing import Dict, Any, Optional
import paho.mqtt.client as mqtt


class RpcTestClient:
    """RPC test client for ur-licence-mann operations"""

    def __init__(self, broker_host="localhost", broker_port=1883):
        self.broker_host = "127.0.0.1"
        self.broker_port = 1899
        self.client = mqtt.Client(
            client_id=f"test_client_{uuid.uuid4().hex[:8]}")
        self.request_topic = "direct_messaging/ur-licence-mann/requests"
        self.response_topic = "direct_messaging/ur-licence-mann/responses"
        self.responses = {}
        self.connected = False

        # Setup callbacks
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message

    def _on_connect(self, client, userdata, flags, rc):
        """Callback for when client connects to broker"""
        if rc == 0:
            print(
                f"✓ Connected to MQTT broker at {self.broker_host}:{self.broker_port}"
            )
            self.connected = True
            # Subscribe to response topic
            client.subscribe(self.response_topic)
            print(f"✓ Subscribed to {self.response_topic}")
        else:
            print(f"✗ Connection failed with code {rc}")
            self.connected = False

    def _on_message(self, client, userdata, msg):
        """Callback for when message is received"""
        try:
            payload = json.loads(msg.payload.decode())
            request_id = payload.get("id", "unknown")
            self.responses[request_id] = payload
            print(f"✓ Received response for request {request_id}")
        except Exception as e:
            print(f"✗ Error processing message: {e}")

    def connect(self, timeout=5):
        """Connect to MQTT broker"""
        try:
            self.client.connect(self.broker_host, self.broker_port, 60)
            self.client.loop_start()

            # Wait for connection
            start_time = time.time()
            while not self.connected and (time.time() - start_time) < timeout:
                time.sleep(0.1)

            return self.connected
        except Exception as e:
            print(f"✗ Connection error: {e}")
            return False

    def disconnect(self):
        """Disconnect from MQTT broker"""
        self.client.loop_stop()
        self.client.disconnect()
        print("✓ Disconnected from MQTT broker")

    def send_request(self, operation: str, parameters: Dict[str, str]) -> str:
        """Send RPC request and return request ID"""
        request_id = str(uuid.uuid4())

        request_payload = {
            "jsonrpc": "2.0",
            "id": request_id,
            "method": operation,
            "params": {
                "operation": operation,
                "parameters": parameters
            }
        }

        payload_str = json.dumps(request_payload)
        result = self.client.publish(self.request_topic, payload_str, qos=1)

        if result.rc == mqtt.MQTT_ERR_SUCCESS:
            print(f"✓ Sent {operation} request with ID {request_id}")
            return request_id
        else:
            print(f"✗ Failed to send request: {result.rc}")
            return None

    def wait_for_response(self,
                          request_id: str,
                          timeout: float = 10.0) -> Optional[Dict[str, Any]]:
        """Wait for response to a specific request"""
        start_time = time.time()

        while (time.time() - start_time) < timeout:
            if request_id in self.responses:
                return self.responses[request_id]
            time.sleep(0.1)

        print(f"✗ Timeout waiting for response to {request_id}")
        return None

    def call_operation(self,
                       operation: str,
                       parameters: Dict[str, str],
                       timeout: float = 10.0) -> Optional[Dict[str, Any]]:
        """Send request and wait for response"""
        request_id = self.send_request(operation, parameters)
        if not request_id:
            return None

        return self.wait_for_response(request_id, timeout)


def print_result(operation: str, response: Optional[Dict[str, Any]]):
    """Pretty print operation result"""
    print(f"\n{'='*60}")
    print(f"Operation: {operation}")
    print(f"{'='*60}")

    if not response:
        print("✗ No response received")
        return

    if "error" in response:
        print(f"✗ Error: {response['error']}")
    elif "result" in response:
        result = response["result"]
        if isinstance(result, dict):
            if result.get("success"):
                print(
                    f"✓ Success: {result.get('message', 'Operation completed')}"
                )
                if "data" in result and result["data"]:
                    print("\nData:")
                    for key, value in result["data"].items():
                        print(f"  {key}: {value}")
            else:
                print(f"✗ Failed: {result.get('message', 'Operation failed')}")
                print(f"  Exit code: {result.get('exit_code', 'unknown')}")
        else:
            print(f"Result: {result}")
    else:
        print(f"Raw response: {json.dumps(response, indent=2)}")

    print(f"{'='*60}\n")
