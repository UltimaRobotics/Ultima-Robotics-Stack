#!/usr/bin/env python3
"""
Test RPC operation: add
Add a new VPN instance
Usage: python test_add_operation.py <instance_name> <config_file_path> [vpn_type] [auto_start]
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

def test_add(instance_name, config_file, vpn_type="openvpn", auto_start=True):
    """Test adding a new VPN instance via RPC"""
    test = BaseRPCTest()
    
    try:
        if not test.setup():
            print("✗ Test setup failed")
            return False
            
        print(f"Testing: Add VPN Instance '{instance_name}'")
        
        config_content = read_config_file(config_file)
        
        params = {
            "instance_name": instance_name,
            "config_content": config_content,
            "vpn_type": vpn_type,
            "auto_start": auto_start
        }
        
        response = test.send_rpc_request("add", params)
        print_response(response)
        
        # Basic validation
        if 'result' in response and response['result'].get('success'):
            print("✓ Add operation completed successfully")
            return True
        else:
            print("✗ Add operation failed")
            return False
            
    except Exception as e:
        print(f"✗ Test failed: {e}")
        return False
    finally:
        test.teardown()

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python test_add_operation.py <instance_name> <config_file_path> [vpn_type] [auto_start]")
        print("  vpn_type: openvpn (default) or wireguard")
        print("  auto_start: true (default) or false")
        sys.exit(1)
    
    instance_name = sys.argv[1]
    config_file = sys.argv[2]
    vpn_type = sys.argv[3] if len(sys.argv) > 3 else "openvpn"
    auto_start = sys.argv[4].lower() == "true" if len(sys.argv) > 4 else True
    
    success = test_add(instance_name, config_file, vpn_type, auto_start)
    sys.exit(0 if success else 1)
