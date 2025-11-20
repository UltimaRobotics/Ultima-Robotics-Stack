
#!/usr/bin/env python3
"""
Test suite for VPN Instance Manager HTTP API - Add Operation
Usage: python test_add.py <instance_name> <config_file_path> [vpn_type] [auto_start]
"""

import requests
import json
import sys
import os

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

def read_config_file(filepath):
    """Read VPN configuration from file"""
    if not os.path.exists(filepath):
        print(f"Error: File '{filepath}' not found")
        sys.exit(1)
    
    try:
        with open(filepath, 'r') as f:
            content = f.read()
        return content
    except Exception as e:
        print(f"Error reading file: {e}")
        sys.exit(1)

def test_add(instance_name, config_file, vpn_type="openvpn", auto_start=True):
    """Test adding a new VPN instance"""
    print(f"Testing: Add VPN Instance '{instance_name}'")
    
    config_content = read_config_file(config_file)
    
    payload = {
        "operation_type": "add",
        "instance_name": instance_name,
        "config_content": config_content,
        "vpn_type": vpn_type,
        "auto_start": auto_start
    }
    
    response = requests.post(API_ENDPOINT, json=payload)
    print_response(response)
    return response.status_code == 200

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python test_add.py <instance_name> <config_file_path> [vpn_type] [auto_start]")
        print("  vpn_type: openvpn (default) or wireguard")
        print("  auto_start: true (default) or false")
        sys.exit(1)
    
    instance_name = sys.argv[1]
    config_file = sys.argv[2]
    vpn_type = sys.argv[3] if len(sys.argv) > 3 else "openvpn"
    auto_start = sys.argv[4].lower() == "true" if len(sys.argv) > 4 else True
    
    success = test_add(instance_name, config_file, vpn_type, auto_start)
    sys.exit(0 if success else 1)
