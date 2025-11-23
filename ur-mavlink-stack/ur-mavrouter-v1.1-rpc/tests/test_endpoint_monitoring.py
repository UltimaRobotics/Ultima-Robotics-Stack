#!/usr/bin/env python3
"""
Python test for collecting endpoint monitoring data from ur-mavrouter
in the Ultima Robotics Stack.

This test connects to the MQTT broker and subscribes to the endpoint monitoring
topic to collect real-time endpoint status data published by the endpoint monitor.
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
class EndpointMessage:
    """Data class representing endpoint monitoring message"""
    topic: str
    payload: Dict[str, Any]
    timestamp: datetime
    sequence: int


class EndpointMonitoringCollector:
    """MQTT client for collecting endpoint monitoring data from ur-mavrouter"""
    
    def __init__(self, broker_host: str = "127.0.0.1", broker_port: int = 1899, 
                 client_id: str = "endpoint_monitor_test"):
        self.broker_host = broker_host
        self.broker_port = broker_port
        self.client_id = client_id
        self.monitoring_topic = "ur-shared-bus/ur-mavlink-stack/ur-mavrouter/notification"
        
        # Message storage
        self.received_messages: List[EndpointMessage] = []
        self.message_count = 0
        self.running = False
        self.lock = threading.Lock()
        
        # MQTT client setup
        self.client = mqtt.Client(client_id=client_id)
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message
        self.client.on_disconnect = self._on_disconnect
        
        print(f"EndpointMonitoringCollector initialized")
        print(f"Broker: {broker_host}:{broker_port}")
        print(f"Topic: {self.monitoring_topic}")
    
    def _on_connect(self, client, userdata, flags, rc):
        """MQTT connection callback"""
        if rc == 0:
            print(f"✓ Connected to MQTT broker (rc={rc})")
            # Subscribe to endpoint monitoring topic
            result, mid = client.subscribe(self.monitoring_topic, qos=1)
            if result == mqtt.MQTT_ERR_SUCCESS:
                print(f"✓ Subscribed to topic: {self.monitoring_topic}")
            else:
                print(f"✗ Failed to subscribe to topic (error code: {result})")
        else:
            print(f"✗ Failed to connect to MQTT broker (rc={rc})")
            print(f"  Connection result codes:")
            print(f"    1: Connection refused - incorrect protocol version")
            print(f"    2: Connection refused - invalid client identifier")
            print(f"    3: Connection refused - server unavailable")
            print(f"    4: Connection refused - bad username or password")
            print(f"    5: Connection refused - not authorised")
    
    def _on_message(self, client, userdata, msg):
        """MQTT message callback"""
        try:
            # Decode message payload
            payload_str = msg.payload.decode('utf-8')
            payload_json = json.loads(payload_str)
            
            # Extract sequence number if available
            sequence = payload_json.get('sequence', 0)
            
            # Create message object
            endpoint_msg = EndpointMessage(
                topic=msg.topic,
                payload=payload_json,
                timestamp=datetime.now(),
                sequence=sequence
            )
            
            # Store message
            with self.lock:
                self.received_messages.append(endpoint_msg)
                self.message_count += 1
            
            print(f"📨 Received message #{self.message_count} (sequence: {sequence})")
            print(f"   Topic: {msg.topic}")
            print(f"   Timestamp: {endpoint_msg.timestamp.strftime('%H:%M:%S.%f')[:-3]}")
            
            # Print summary of endpoint data
            if 'summary' in payload_json:
                summary = payload_json['summary']
                print(f"   Summary: {summary.get('total_endpoints', 0)} total, "
                      f"{summary.get('occupied_endpoints', 0)} occupied")
            
            # Print endpoint details if available
            if 'endpoints' in payload_json and 'main_router' in payload_json['endpoints']:
                main_router_endpoints = payload_json['endpoints']['main_router']
                if main_router_endpoints:
                    print(f"   Main router endpoints: {len(main_router_endpoints)}")
                    for endpoint in main_router_endpoints[:3]:  # Show first 3 endpoints
                        name = endpoint.get('name', 'unknown')
                        occupied = endpoint.get('occupied', False)
                        conn_state = endpoint.get('connection_state', 0)
                        print(f"     - {name}: occupied={occupied}, state={conn_state}")
                    if len(main_router_endpoints) > 3:
                        print(f"     ... and {len(main_router_endpoints) - 3} more")
            
            print()
            
        except json.JSONDecodeError as e:
            print(f"✗ Failed to parse JSON message: {e}")
            print(f"   Raw payload: {msg.payload[:200]}...")
        except Exception as e:
            print(f"✗ Error processing message: {e}")
    
    def _on_disconnect(self, client, userdata, rc):
        """MQTT disconnection callback"""
        print(f"🔌 Disconnected from MQTT broker (rc={rc})")
        self.running = False
    
    def start_collection(self, duration_seconds: int = 60, max_messages: int = 100):
        """Start collecting endpoint monitoring messages"""
        print(f"\n🚀 Starting endpoint monitoring data collection")
        print(f"   Duration: {duration_seconds} seconds")
        print(f"   Max messages: {max_messages}")
        print(f"   Press Ctrl+C to stop early\n")
        
        try:
            # Connect to MQTT broker
            print(f"🔗 Connecting to MQTT broker {self.broker_host}:{self.broker_port}...")
            self.client.connect(self.broker_host, self.broker_port, 60)
            
            # Start the network loop
            self.client.loop_start()
            self.running = True
            
            # Wait for messages or timeout
            start_time = time.time()
            while self.running and (time.time() - start_time) < duration_seconds:
                if self.message_count >= max_messages:
                    print(f"\n⏹️  Reached maximum message count ({max_messages})")
                    break
                
                time.sleep(0.1)
            
            # Stop collection
            self.running = False
            self.client.loop_stop()
            self.client.disconnect()
            
            print(f"\n🏁 Collection completed")
            
        except KeyboardInterrupt:
            print(f"\n⏹️  Collection stopped by user")
            self.running = False
            self.client.loop_stop()
            self.client.disconnect()
        except socket.error as e:
            print(f"\n✗ Connection error: {e}")
            print(f"   Make sure the MQTT broker is running on {self.broker_host}:{self.broker_port}")
        except Exception as e:
            print(f"\n✗ Unexpected error: {e}")
    
    def get_messages(self) -> List[EndpointMessage]:
        """Get all collected messages"""
        with self.lock:
            return self.received_messages.copy()
    
    def get_message_count(self) -> int:
        """Get the number of collected messages"""
        with self.lock:
            return self.message_count
    
    def print_summary(self):
        """Print a summary of collected messages"""
        messages = self.get_messages()
        
        print(f"\n📊 Collection Summary:")
        print(f"   Total messages received: {len(messages)}")
        
        if not messages:
            print(f"   No messages received - check if:")
            print(f"     • ur-mavrouter is running")
            print(f"     • Endpoint monitoring is enabled")
            print(f"     • MQTT broker is accessible")
            print(f"     • RPC client is configured")
            return
        
        # Time range
        first_time = messages[0].timestamp
        last_time = messages[-1].timestamp
        duration = (last_time - first_time).total_seconds()
        
        print(f"   Time range: {first_time.strftime('%H:%M:%S')} - {last_time.strftime('%H:%M:%S')}")
        print(f"   Collection duration: {duration:.2f} seconds")
        
        if duration > 0:
            frequency = len(messages) / duration
            print(f"   Message frequency: {frequency:.2f} messages/second")
        
        # Analyze message content
        sequences = [msg.sequence for msg in messages]
        print(f"   Sequence range: {min(sequences)} - {max(sequences)}")
        
        # Check for raw JSON structure (not response/request)
        raw_json_count = 0
        endpoint_data_count = 0
        
        for msg in messages:
            payload = msg.payload
            # Check if this is raw JSON data (not wrapped in response/request structure)
            if 'endpoints' in payload or 'summary' in payload or 'timestamp' in payload:
                raw_json_count += 1
            if 'endpoints' in payload:
                endpoint_data_count += 1
        
        print(f"   Raw JSON messages: {raw_json_count}/{len(messages)}")
        print(f"   Messages with endpoint data: {endpoint_data_count}/{len(messages)}")
        
        # Show sample payload
        if messages:
            print(f"\n📄 Sample message payload:")
            sample = messages[-1].payload  # Use last message as sample
            print(json.dumps(sample, indent=2, ensure_ascii=False))
    
    def save_to_file(self, filename: str = None):
        """Save collected messages to a JSON file"""
        if filename is None:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"endpoint_monitoring_data_{timestamp}.json"
        
        messages = self.get_messages()
        
        # Convert messages to serializable format
        data = {
            'collection_info': {
                'topic': self.monitoring_topic,
                'total_messages': len(messages),
                'collection_time': datetime.now().isoformat(),
                'broker': f"{self.broker_host}:{self.broker_port}"
            },
            'messages': []
        }
        
        for msg in messages:
            data['messages'].append({
                'topic': msg.topic,
                'timestamp': msg.timestamp.isoformat(),
                'sequence': msg.sequence,
                'payload': msg.payload
            })
        
        # Save to file
        try:
            with open(filename, 'w', encoding='utf-8') as f:
                json.dump(data, f, indent=2, ensure_ascii=False)
            
            print(f"\n💾 Data saved to: {filename}")
            print(f"   File size: {len(json.dumps(data))} bytes")
            
        except Exception as e:
            print(f"\n✗ Failed to save data to file: {e}")


def main():
    """Main test function"""
    print("🧪 Endpoint Monitoring Data Collection Test")
    print("=" * 50)
    
    # Create collector
    collector = EndpointMonitoringCollector()
    
    # Start collection (run for 30 seconds or collect 50 messages)
    collector.start_collection(duration_seconds=30, max_messages=50)
    
    # Print summary
    collector.print_summary()
    
    # Save data to file
    collector.save_to_file()
    
    print(f"\n✅ Test completed!")


if __name__ == "__main__":
    main()
