#!/usr/bin/env python3
"""
Test RPC operation: stats
Get aggregated statistics for all VPN instances
Usage: python test_stats_operation.py
"""

import sys
import os
import json
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from base_rpc_test import BaseRPCTest

def print_response(response):
    """Pretty print the RPC response"""
    print(f"Response:\n{json.dumps(response, indent=2)}")

def test_stats():
    """Test getting aggregated VPN statistics via RPC"""
    test = BaseRPCTest()
    
    try:
        if not test.setup():
            print("✗ Test setup failed")
            return False
            
        print("Testing: Get Aggregated VPN Statistics")
        
        params = {}
        
        response = test.send_rpc_request("stats", params)
        print_response(response)
        
        # Basic validation
        if 'result' in response and response['result'].get('success'):
            print("✓ Stats operation completed successfully")
            return True
        else:
            print("✗ Stats operation failed")
            return False
            
    except Exception as e:
        print(f"✗ Test failed: {e}")
        return False
    finally:
        test.teardown()

if __name__ == "__main__":
    success = test_stats()
    sys.exit(0 if success else 1)
