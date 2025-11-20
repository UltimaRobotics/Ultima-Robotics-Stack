
#!/usr/bin/env python3
"""
Test suite for VPN Instance Manager HTTP API - Status Operation
Usage: python test_status.py [instance_name]
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

def test_status(instance_name=None):
    """Test getting status of VPN instance(s)"""
    if instance_name:
        print(f"Testing: Get Status of VPN Instance '{instance_name}'")
        payload = {
            "operation_type": "status",
            "instance_name": instance_name
        }
    else:
        print("Testing: Get Status of All VPN Instances")
        payload = {
            "operation_type": "status"
        }
    
    response = requests.post(API_ENDPOINT, json=payload)
    print_response(response)
    return response.status_code == 200

if __name__ == "__main__":
    instance_name = sys.argv[1] if len(sys.argv) > 1 else None
    
    success = test_status(instance_name)
    sys.exit(0 if success else 1)
