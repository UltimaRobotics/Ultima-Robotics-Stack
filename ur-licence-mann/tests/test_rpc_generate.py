
#!/usr/bin/env python3
"""
Test RPC license generation operation
"""

import sys
import time
from rpc_test_base import RpcTestClient, print_result


def test_generate_license():
    """Test license generation via RPC"""
    print("\n" + "="*60)
    print("Testing: License Generation via RPC")
    print("="*60 + "\n")
    
    client = RpcTestClient()
    
    if not client.connect():
        print("✗ Failed to connect to MQTT broker")
        return False
    
    try:
        # Test 1: Generate basic license
        print("\nTest 1: Generate basic license")
        params = {
            "license_id": "TEST-LICENSE-001",
            "customer_name": "John Doe",
            "customer_email": "john.doe@example.com",
            "output": "./tests/output/rpc_generated_license.lic",
            "device_hardware_id": "test-hardware-id-12345",
            "device_mac": "00:11:22:33:44:55"
        }
        
        response = client.call_operation("generate", params, timeout=15.0)
        print_result("GENERATE", response)
        
        # Test 2: Generate with specific tier
        print("\nTest 2: Generate license with specific tier")
        params = {
            "license_id": "TEST-LICENSE-002",
            "customer_name": "Jane Smith",
            "customer_email": "jane.smith@example.com",
            "output": "./tests/output/rpc_generated_premium.lic",
            "license_tier": "Premium",
            "device_hardware_id": "test-hardware-id-67890"
        }
        
        response = client.call_operation("generate", params, timeout=15.0)
        print_result("GENERATE (Premium)", response)
        
        # Test 3: Generate with expiry date
        print("\nTest 3: Generate license with custom expiry")
        params = {
            "license_id": "TEST-LICENSE-003",
            "customer_name": "Bob Johnson",
            "customer_email": "bob.johnson@example.com",
            "output": "./tests/output/rpc_generated_expiry.lic",
            "valid_until": "2025-12-31T23:59:59Z",
            "device_hardware_id": "test-hardware-id-99999"
        }
        
        response = client.call_operation("generate", params, timeout=15.0)
        print_result("GENERATE (Custom Expiry)", response)
        
        return True
        
    finally:
        client.disconnect()


if __name__ == "__main__":
    success = test_generate_license()
    sys.exit(0 if success else 1)
