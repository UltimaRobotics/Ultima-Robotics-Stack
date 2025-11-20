
#!/usr/bin/env python3
"""
Test suite for VPN Instance Manager HTTP API - Delete Operation
Usage: python test_delete.py <instance_name> [vpn_type]
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

def test_delete(instance_name, vpn_type=""):
    """Test deleting a VPN instance"""
    print(f"Testing: Delete VPN Instance '{instance_name}'")
    
    payload = {
        "operation_type": "delete",
        "instance_name": instance_name
    }
    
    if vpn_type:
        payload["vpn_type"] = vpn_type
    
    response = requests.post(API_ENDPOINT, json=payload)
    print_response(response)
    return response.status_code == 200

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python test_delete.py <instance_name> [vpn_type]")
        print("  vpn_type: optional - openvpn or wireguard")
        sys.exit(1)
    
    instance_name = sys.argv[1]
    vpn_type = sys.argv[2] if len(sys.argv) > 2 else ""
    
    success = test_delete(instance_name, vpn_type)
    sys.exit(0 if success else 1)
