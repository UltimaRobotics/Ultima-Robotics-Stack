
#!/usr/bin/env python3
"""
Test suite for VPN Instance Manager HTTP API - Update Operation
Usage: python test_update.py <instance_name> <config_file_path>
"""

import requests
import json
import sys
import os

# Configuration
API_BASE_URL = "http://0.0.0.0:8080"
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

def test_update(instance_name, config_file):
    """Test updating a VPN instance"""
    print(f"Testing: Update VPN Instance '{instance_name}'")
    
    config_content = read_config_file(config_file)
    
    payload = {
        "operation_type": "update",
        "instance_name": instance_name,
        "config_content": config_content
    }
    
    response = requests.post(API_ENDPOINT, json=payload)
    print_response(response)
    return response.status_code == 200

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python test_update.py <instance_name> <config_file_path>")
        sys.exit(1)
    
    instance_name = sys.argv[1]
    config_file = sys.argv[2]
    
    success = test_update(instance_name, config_file)
    sys.exit(0 if success else 1)
