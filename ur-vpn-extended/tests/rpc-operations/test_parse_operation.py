#!/usr/bin/env python3
"""
Test RPC operation: parse
Parse VPN configuration without making system changes
Usage: python test_parse_operation.py <config_file_path>
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

def test_parse(config_file):
    """Test parsing VPN configuration via RPC"""
    test = BaseRPCTest()
    
    try:
        if not test.setup():
            print("✗ Test setup failed")
            return False
            
        print(f"Testing: Parse VPN Configuration from '{config_file}'")
        
        config_content = read_config_file(config_file)
        
        params = {
            "config_content": config_content
        }
        
        response = test.send_rpc_request("parse", params)
        print_response(response)
        
        # Basic validation
        if 'result' in response and response['result'].get('success'):
            print("✓ Parse operation completed successfully")
            return True
        else:
            print("✗ Parse operation failed")
            return False
            
    except Exception as e:
        print(f"✗ Test failed: {e}")
        return False
    finally:
        test.teardown()

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python test_parse_operation.py <config_file_path>")
        sys.exit(1)
    
    config_file = sys.argv[1]
    
    success = test_parse(config_file)
    sys.exit(0 if success else 1)
