
#!/usr/bin/env python3
"""
Test RPC license verification operation
"""

import sys
from rpc_test_base import RpcTestClient, print_result


def test_verify_license():
    """Test license verification via RPC"""
    print("\n" + "="*60)
    print("Testing: License Verification via RPC")
    print("="*60 + "\n")
    
    client = RpcTestClient()
    
    if not client.connect():
        print("✗ Failed to connect to MQTT broker")
        return False
    
    try:
        # Test 1: Verify valid license
        print("\nTest 1: Verify valid license")
        params = {
            "license_file": "./tests/output/rpc_generated_license.lic",
            "check_expiry": "true"
        }
        
        response = client.call_operation("verify", params, timeout=15.0)
        print_result("VERIFY (Valid)", response)
        
        # Test 2: Verify without expiry check
        print("\nTest 2: Verify without expiry check")
        params = {
            "license_file": "./tests/output/rpc_generated_license.lic",
            "check_expiry": "false"
        }
        
        response = client.call_operation("verify", params, timeout=15.0)
        print_result("VERIFY (No Expiry Check)", response)
        
        # Test 3: Verify non-existent license (should fail)
        print("\nTest 3: Verify non-existent license (expected to fail)")
        params = {
            "license_file": "./tests/output/nonexistent.lic"
        }
        
        response = client.call_operation("verify", params, timeout=15.0)
        print_result("VERIFY (Non-existent)", response)
        
        return True
        
    finally:
        client.disconnect()


if __name__ == "__main__":
    success = test_verify_license()
    sys.exit(0 if success else 1)
