
#!/usr/bin/env python3
"""
Test suite for VPN Instance Manager HTTP API - List Operation
Usage: python test_list.py [vpn_type]
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

def test_list(vpn_type=None):
    """Test listing VPN instances"""
    if vpn_type:
        print(f"Testing: List VPN Instances of type '{vpn_type}'")
        payload = {
            "operation_type": "list",
            "vpn_type": vpn_type
        }
    else:
        print("Testing: List All VPN Instances")
        payload = {
            "operation_type": "list"
        }
    
    response = requests.post(API_ENDPOINT, json=payload)
    print_response(response)
    return response.status_code == 200

if __name__ == "__main__":
    vpn_type = sys.argv[1] if len(sys.argv) > 1 else None
    
    success = test_list(vpn_type)
    sys.exit(0 if success else 1)
