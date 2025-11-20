
#!/usr/bin/env python3
"""
Test suite for VPN Instance Manager HTTP API - Stats Operation
Usage: python test_stats.py
"""

import requests
import json
import sys

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

def test_stats():
    """Test getting aggregated statistics"""
    print("Testing: Get Aggregated Statistics")
    
    payload = {
        "operation_type": "stats"
    }
    
    response = requests.post(API_ENDPOINT, json=payload)
    print_response(response)
    return response.status_code == 200

if __name__ == "__main__":
    success = test_stats()
    sys.exit(0 if success else 1)
