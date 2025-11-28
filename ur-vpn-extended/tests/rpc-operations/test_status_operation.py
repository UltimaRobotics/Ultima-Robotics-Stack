#!/usr/bin/env python3
"""
Test RPC operation: status
Get VPN instance status
Usage: python test_status_operation.py [instance_name]
"""

import sys
import os
import json
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from base_rpc_test import BaseRPCTest

def print_response(response):
    """Pretty print the RPC response"""
    print(f"Response:\n{json.dumps(response, indent=2)}")

def test_status(instance_name=None):
    """Test getting VPN instance status via RPC"""
    test = BaseRPCTest()
    
    try:
        if not test.setup():
            print("✗ Test setup failed")
            return False
            
        if instance_name:
            print(f"Testing: Get Status of VPN Instance '{instance_name}'")
            params = {"instance_name": instance_name}
        else:
            print("Testing: Get Status of All VPN Instances")
            params = {}
        
        response = test.send_rpc_request("status", params)
        print_response(response)
        
        # Basic validation
        if 'result' in response and response['result'].get('success'):
            print("✓ Status operation completed successfully")
            return True
        else:
            print("✗ Status operation failed")
            return False
            
    except Exception as e:
        print(f"✗ Test failed: {e}")
        return False
    finally:
        test.teardown()

if __name__ == "__main__":
    instance_name = sys.argv[1] if len(sys.argv) > 1 else None
    
    success = test_status(instance_name)
    sys.exit(0 if success else 1)
