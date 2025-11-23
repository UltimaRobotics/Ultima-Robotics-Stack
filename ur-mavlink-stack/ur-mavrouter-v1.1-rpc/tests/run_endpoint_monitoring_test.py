#!/usr/bin/env python3
"""
Test runner script for endpoint monitoring data collection test
"""

import subprocess
import sys
import os
import json
from datetime import datetime

def check_dependencies():
    """Check if required dependencies are installed"""
    try:
        import paho.mqtt.client as mqtt
        print("✓ paho-mqtt is installed")
        return True
    except ImportError:
        print("✗ paho-mqtt is not installed")
        print("Installing dependencies...")
        try:
            subprocess.check_call([sys.executable, "-m", "pip", "install", "-r", "requirements.txt"])
            print("✓ Dependencies installed successfully")
            return True
        except subprocess.CalledProcessError:
            print("✗ Failed to install dependencies")
            return False

def save_test_results(test_output, return_code, start_time, end_time):
    """Save test results and any captured messages to a local file"""
    
    # Create results data structure
    results = {
        "test_info": {
            "test_name": "endpoint_monitoring_data_collection",
            "start_time": start_time.isoformat(),
            "end_time": end_time.isoformat(),
            "duration_seconds": (end_time - start_time).total_seconds(),
            "return_code": return_code,
            "success": return_code == 0
        },
        "test_output": test_output,
        "messages_captured": [],
        "summary": {
            "total_lines_output": len(test_output.split('\n')) if test_output else 0,
            "has_connection_success": "✓ Connected to MQTT broker" in test_output if test_output else False,
            "has_subscription_success": "✓ Subscribed to topic" in test_output if test_output else False,
            "messages_received_count": test_output.count("📨 Received message") if test_output else 0
        }
    }
    
    # Extract message data from output if available
    if test_output:
        lines = test_output.split('\n')
        message_data = {}
        current_message = {}
        
        for line in lines:
            if "📨 Received message #" in line:
                # Save previous message if exists
                if current_message:
                    results["messages_captured"].append(current_message.copy())
                
                # Start new message
                parts = line.split()
                msg_num = parts[2] if len(parts) > 2 else "unknown"
                seq_part = line.split("(sequence: ")
                sequence = seq_part[1].rstrip(")") if len(seq_part) > 1 else "unknown"
                
                current_message = {
                    "message_number": msg_num,
                    "sequence": sequence,
                    "timestamp": "",
                    "topic": "",
                    "summary": {},
                    "endpoints": []
                }
            
            elif "Topic:" in line and current_message:
                current_message["topic"] = line.split("Topic:", 1)[1].strip()
            
            elif "Timestamp:" in line and current_message:
                current_message["timestamp"] = line.split("Timestamp:", 1)[1].strip()
            
            elif "Summary:" in line and current_message:
                summary_part = line.split("Summary:", 1)[1].strip()
                if "total," in summary_part and "occupied" in summary_part:
                    total_part = summary_part.split(" total,")[0]
                    occupied_part = summary_part.split(" total,")[1].split(" occupied")[0]
                    try:
                        current_message["summary"]["total_endpoints"] = int(total_part)
                        current_message["summary"]["occupied_endpoints"] = int(occupied_part)
                    except ValueError:
                        pass
            
            elif "Main router endpoints:" in line and current_message:
                count_part = line.split("Main router endpoints:")[1].strip()
                try:
                    current_message["endpoint_count"] = int(count_part)
                except ValueError:
                    pass
        
        # Save last message if exists
        if current_message:
            results["messages_captured"].append(current_message)
    
    # Generate filename with timestamp
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = f"endpoint_monitoring_test_results_{timestamp}.json"
    
    # Save to file
    try:
        with open(filename, 'w', encoding='utf-8') as f:
            json.dump(results, f, indent=2, ensure_ascii=False)
        
        print(f"\n💾 Test results saved to: {filename}")
        print(f"   File contains: {len(results['messages_captured'])} captured messages")
        print(f"   Test duration: {results['test_info']['duration_seconds']:.2f} seconds")
        print(f"   Success status: {'✅ PASS' if return_code == 0 else '❌ FAIL'}")
        
        return filename
        
    except Exception as e:
        print(f"\n✗ Failed to save test results: {e}")
        return None

def main():
    """Main test runner"""
    print("🚀 Endpoint Monitoring Test Runner")
    print("=" * 50)
    
    # Check dependencies
    if not check_dependencies():
        print("❌ Cannot proceed without dependencies")
        return 1
    
    # Change to test directory
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    
    # Record start time
    start_time = datetime.now()
    
    # Run the endpoint monitoring test
    cmd = [sys.executable, "test_endpoint_monitoring.py"]
    
    print(f"\n🧪 Running: {' '.join(cmd)}")
    print("-" * 50)
    
    try:
        # Run test and capture output
        result = subprocess.run(cmd, cwd=os.path.dirname(__file__), 
                              capture_output=True, text=True)
        
        # Record end time
        end_time = datetime.now()
        
        # Print the test output
        print(result.stdout)
        if result.stderr:
            print("STDERR:", result.stderr)
        
        # Save results to file
        output_file = save_test_results(
            result.stdout + "\nSTDERR:\n" + result.stderr if result.stderr else result.stdout,
            result.returncode,
            start_time,
            end_time
        )
        
        # Print final summary
        print("\n" + "=" * 50)
        print("📊 Test Execution Summary")
        print("=" * 50)
        
        duration = (end_time - start_time).total_seconds()
        status = "✅ PASS" if result.returncode == 0 else "❌ FAIL"
        
        print(f"Status: {status}")
        print(f"Duration: {duration:.2f} seconds")
        print(f"Return Code: {result.returncode}")
        
        if output_file:
            print(f"Results File: {output_file}")
        
        # Check for key success indicators
        if result.stdout:
            if "✓ Connected to MQTT broker" in result.stdout:
                print("✓ MQTT Connection: Successful")
            else:
                print("✗ MQTT Connection: Failed")
            
            if "✓ Subscribed to topic" in result.stdout:
                print("✓ Topic Subscription: Successful")
            else:
                print("✗ Topic Subscription: Failed")
            
            msg_count = result.stdout.count("📨 Received message")
            print(f"✓ Messages Received: {msg_count}")
            
            if msg_count > 0:
                print("✓ Data Collection: Working")
            else:
                print("✗ Data Collection: No messages received")
        
        return result.returncode
        
    except Exception as e:
        print(f"✗ Failed to run test: {e}")
        end_time = datetime.now()
        
        # Save error results
        save_test_results(
            f"Test execution failed with error: {e}",
            1,
            start_time,
            end_time
        )
        
        return 1

if __name__ == "__main__":
    sys.exit(main())
