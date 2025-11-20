
#!/usr/bin/env python3
"""
Test RPC get license info operation
"""

import sys
import json
from rpc_test_base import RpcTestClient, print_result


def test_get_license_info():
    """Test get license info via RPC"""
    print("\n" + "="*60)
    print("Testing: Get License Info via RPC")
    print("="*60 + "\n")
    
    client = RpcTestClient()
    
    if not client.connect():
        print("✗ Failed to connect to MQTT broker")
        return False
    
    try:
        # Test 1: Get info from generated license
        print("\nTest 1: Get license information")
        params = {
            "license_file": "./tests/output/rpc_generated_license.lic"
        }
        
        response = client.call_operation("get_license_info", params, timeout=15.0)
        print_result("GET_LICENSE_INFO", response)
        
        if response and "result" in response:
            result = response["result"]
            if isinstance(result, dict) and result.get("data"):
                print("\nExtracted License Information:")
                data = result.get("data", {})
                for key, value in data.items():
                    print(f"  {key}: {value}")
        
        # Test 2: Get info from updated license
        print("\nTest 2: Get info from updated license")
        params = {
            "license_file": "./tests/output/rpc_updated_license.lic"
        }
        
        response = client.call_operation("get_license_info", params, timeout=15.0)
        print_result("GET_LICENSE_INFO (Updated)", response)
        
        return True
        
    finally:
        client.disconnect()


if __name__ == "__main__":
    success = test_get_license_info()
    sys.exit(0 if success else 1)
