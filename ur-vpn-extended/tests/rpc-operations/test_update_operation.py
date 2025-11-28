#!/usr/bin/env python3
"""
Test RPC operation: update
Update an existing VPN instance configuration
Usage: python test_update_operation.py <instance_name> <config_file_path>
"""

import sys
import os
import json
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from base_rpc_test import BaseRPCTest

def print_response(response):
    """Pretty print the RPC response"""
    print(f"Response:\n{json.dumps(response, indent=2)}")

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
    """Test updating a VPN instance via RPC"""
    test = BaseRPCTest()
    
    try:
        if not test.setup():
            print("✗ Test setup failed")
            return False
            
        print(f"Testing: Update VPN Instance '{instance_name}'")
        
        config_content = read_config_file(config_file)
        
        params = {
            "instance_name": instance_name,
            "config_content": config_content
        }
        
        response = test.send_rpc_request("update", params)
        print_response(response)
        
        # Basic validation
        if 'result' in response and response['result'].get('success'):
            print("✓ Update operation completed successfully")
            return True
        else:
            print("✗ Update operation failed")
            return False
            
    except Exception as e:
        print(f"✗ Test failed: {e}")
        return False
    finally:
        test.teardown()

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python test_update_operation.py <instance_name> <config_file_path>")
        sys.exit(1)
    
    instance_name = sys.argv[1]
    config_file = sys.argv[2]
    
    success = test_update(instance_name, config_file)
    sys.exit(0 if success else 1)
