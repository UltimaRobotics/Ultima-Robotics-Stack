
#!/usr/bin/env python3
"""
Test RPC license update operation
"""

import sys
from rpc_test_base import RpcTestClient, print_result


def test_update_license():
    """Test license update via RPC"""
    print("\n" + "="*60)
    print("Testing: License Update via RPC")
    print("="*60 + "\n")
    
    client = RpcTestClient()
    
    if not client.connect():
        print("✗ Failed to connect to MQTT broker")
        return False
    
    try:
        # Test 1: Update license expiry
        print("\nTest 1: Update license expiry date")
        params = {
            "input_file": "./tests/output/rpc_generated_license.lic",
            "output_file": "./tests/output/rpc_updated_license.lic",
            "new_expiry": "2026-12-31T23:59:59Z"
        }
        
        response = client.call_operation("update", params, timeout=15.0)
        print_result("UPDATE (Expiry)", response)
        
        # Test 2: Update license type
        print("\nTest 2: Update license type")
        params = {
            "input_file": "./tests/output/rpc_generated_license.lic",
            "output_file": "./tests/output/rpc_updated_type.lic",
            "licence_type": "ExtendedOpenLicence"
        }
        
        response = client.call_operation("update", params, timeout=15.0)
        print_result("UPDATE (Type)", response)
        
        # Test 3: Update multiple fields
        print("\nTest 3: Update multiple fields")
        params = {
            "input_file": "./tests/output/rpc_generated_license.lic",
            "output_file": "./tests/output/rpc_updated_multi.lic",
            "new_expiry": "2027-06-30T23:59:59Z",
            "customer_name": "Updated Customer",
            "license_tier": "Enterprise"
        }
        
        response = client.call_operation("update", params, timeout=15.0)
        print_result("UPDATE (Multiple)", response)
        
        return True
        
    finally:
        client.disconnect()


if __name__ == "__main__":
    success = test_update_license()
    sys.exit(0 if success else 1)
