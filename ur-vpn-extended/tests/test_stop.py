
#!/usr/bin/env python3
"""
Test suite for VPN Instance Manager HTTP API - Stop Operation
Usage: python test_stop.py <instance_name>
"""

import requests
import json
import sys

# Configuration
API_BASE_URL = "http://0.0.0.0:5000"
API_ENDPOINT = f"{API_BASE_URL}/api/operations/"

def print_response(response):
    """Pretty print the API response"""
    print(f"Status Code: {response.status_code}")
    try:
        print(f"Response:\n{json.dumps(response.json(), indent=2)}")
    except:
        print(f"Response: {response.text}")

def test_stop(instance_name):
    """Test stopping a VPN instance"""
    print(f"Testing: Stop VPN Instance '{instance_name}'")
    
    payload = {
        "operation_type": "stop",
        "instance_name": instance_name
    }
    
    response = requests.post(API_ENDPOINT, json=payload)
    print_response(response)
    return response.status_code == 200

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python test_stop.py <instance_name>")
        sys.exit(1)
    
    instance_name = sys.argv[1]
    
    success = test_stop(instance_name)
    sys.exit(0 if success else 1)
