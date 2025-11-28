#!/usr/bin/env python3
"""
Test RPC operation: start
Start a VPN instance
Usage: python test_start_operation.py <instance_name>
"""

import sys
import os
import json
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from base_rpc_test import BaseRPCTest

def print_response(response):
    """Pretty print the RPC response"""
    print(f"Response:\n{json.dumps(response, indent=2)}")

def test_start(instance_name):
    """Test starting a VPN instance via RPC"""
    test = BaseRPCTest()
    
    try:
        if not test.setup():
            print("✗ Test setup failed")
            return False
            
        print(f"Testing: Start VPN Instance '{instance_name}'")
        
        params = {
            "instance_name": instance_name
        }
        
        response = test.send_rpc_request("start", params)
        print_response(response)
        
        # Basic validation
        if 'result' in response and response['result'].get('success'):
            print("✓ Start operation completed successfully")
            return True
        else:
            print("✗ Start operation failed")
            return False
            
    except Exception as e:
        print(f"✗ Test failed: {e}")
        return False
    finally:
        test.teardown()

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python test_start_operation.py <instance_name>")
        sys.exit(1)
    
    instance_name = sys.argv[1]
    
    success = test_start(instance_name)
    sys.exit(0 if success else 1)
