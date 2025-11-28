#!/usr/bin/env python3
"""
Test RPC operation: enable
Enable and start a VPN instance
Usage: python test_enable_operation.py <instance_name>
"""

import sys
import os
import json
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from base_rpc_test import BaseRPCTest

def print_response(response):
    """Pretty print the RPC response"""
    print(f"Response:\n{json.dumps(response, indent=2)}")

def test_enable(instance_name):
    """Test enabling a VPN instance via RPC"""
    test = BaseRPCTest()
    
    try:
        if not test.setup():
            print("✗ Test setup failed")
            return False
            
        print(f"Testing: Enable VPN Instance '{instance_name}'")
        
        params = {
            "instance_name": instance_name
        }
        
        response = test.send_rpc_request("enable", params)
        print_response(response)
        
        # Basic validation
        if 'result' in response and response['result'].get('success'):
            print("✓ Enable operation completed successfully")
            return True
        else:
            print("✗ Enable operation failed")
            return False
            
    except Exception as e:
        print(f"✗ Test failed: {e}")
        return False
    finally:
        test.teardown()

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python test_enable_operation.py <instance_name>")
        sys.exit(1)
    
    instance_name = sys.argv[1]
    
    success = test_enable(instance_name)
    sys.exit(0 if success else 1)
