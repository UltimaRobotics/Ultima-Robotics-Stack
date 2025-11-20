
#!/usr/bin/env python3
"""
Test RPC get license definitions operation
"""

import sys
from rpc_test_base import RpcTestClient, print_result


def test_get_license_definitions():
    """Test get license definitions via RPC"""
    print("\n" + "="*60)
    print("Testing: Get License Definitions via RPC")
    print("="*60 + "\n")
    
    client = RpcTestClient()
    
    if not client.connect():
        print("✗ Failed to connect to MQTT broker")
        return False
    
    try:
        # Test: Get current license definitions
        print("\nTest: Get license definitions")
        params = {}
        
        response = client.call_operation("get_license_definitions", params, timeout=15.0)
        print_result("GET_LICENSE_DEFINITIONS", response)
        
        if response and "result" in response:
            result = response["result"]
            if isinstance(result, dict) and result.get("data"):
                print("\nLicense Definitions Retrieved:")
                data = result.get("data", {})
                # Print a summary
                if "license_types" in data or any("license" in k.lower() for k in data.keys()):
                    print("  ✓ License definitions found")
        
        return True
        
    finally:
        client.disconnect()


if __name__ == "__main__":
    success = test_get_license_definitions()
    sys.exit(0 if success else 1)
