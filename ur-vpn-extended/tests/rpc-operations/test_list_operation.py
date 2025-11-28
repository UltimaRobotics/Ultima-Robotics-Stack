#!/usr/bin/env python3
"""
Test RPC operation: list
List all VPN instances
Usage: python test_list_operation.py [vpn_type]
"""

import sys
import os
import json
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from base_rpc_test import BaseRPCTest

def print_response(response):
    """Pretty print the RPC response"""
    print(f"Response:\n{json.dumps(response, indent=2)}")

def test_list(vpn_type=None):
    """Test listing VPN instances via RPC"""
    test = BaseRPCTest()
    
    try:
        if not test.setup():
            print("✗ Test setup failed")
            return False
            
        if vpn_type:
            print(f"Testing: List VPN Instances of type '{vpn_type}'")
            params = {"vpn_type": vpn_type}
        else:
            print("Testing: List All VPN Instances")
            params = {}
        
        response = test.send_rpc_request("list", params)
        print_response(response)
        
        # Basic validation
        if 'result' in response and response['result'].get('success'):
            print("✓ List operation completed successfully")
            return True
        else:
            print("✗ List operation failed")
            return False
            
    except Exception as e:
        print(f"✗ Test failed: {e}")
        return False
    finally:
        test.teardown()

if __name__ == "__main__":
    vpn_type = sys.argv[1] if len(sys.argv) > 1 else None
    
    success = test_list(vpn_type)
    sys.exit(0 if success else 1)
