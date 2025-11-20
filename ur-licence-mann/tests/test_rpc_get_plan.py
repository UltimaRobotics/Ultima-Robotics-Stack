
#!/usr/bin/env python3
"""
Test RPC get license plan operation
"""

import sys
from rpc_test_base import RpcTestClient, print_result


def test_get_license_plan():
    """Test get license plan via RPC"""
    print("\n" + "="*60)
    print("Testing: Get License Plan via RPC")
    print("="*60 + "\n")
    
    client = RpcTestClient()
    
    if not client.connect():
        print("✗ Failed to connect to MQTT broker")
        return False
    
    try:
        # Test 1: Get plan from license
        print("\nTest 1: Get license plan")
        params = {
            "license_file": "./tests/output/rpc_generated_license.lic"
        }
        
        response = client.call_operation("get_license_plan", params, timeout=15.0)
        print_result("GET_LICENSE_PLAN", response)
        
        # Test 2: Get plan from premium license
        print("\nTest 2: Get plan from premium license")
        params = {
            "license_file": "./tests/output/rpc_generated_premium.lic"
        }
        
        response = client.call_operation("get_license_plan", params, timeout=15.0)
        print_result("GET_LICENSE_PLAN (Premium)", response)
        
        return True
        
    finally:
        client.disconnect()


if __name__ == "__main__":
    success = test_get_license_plan()
    sys.exit(0 if success else 1)
