
#!/usr/bin/env python3
"""
Test RPC update license definitions operation
"""

import sys
import json
from rpc_test_base import RpcTestClient, print_result


def test_update_license_definitions():
    """Test update license definitions via RPC"""
    print("\n" + "="*60)
    print("Testing: Update License Definitions via RPC")
    print("="*60 + "\n")
    
    client = RpcTestClient()
    
    if not client.connect():
        print("✗ Failed to connect to MQTT broker")
        return False
    
    try:
        # Test: Update license definitions with JSON
        print("\nTest: Update license definitions")
        
        # Sample definitions JSON
        definitions = {
            "license_types": {
                "UltimaOpenLicence": {
                    "features": ["basic_access", "standard_support"],
                    "description": "Standard open license"
                },
                "ExtendedOpenLicence": {
                    "features": ["basic_access", "standard_support", "advanced_features"],
                    "description": "Extended license with additional features"
                }
            }
        }
        
        params = {
            "definitions_json": json.dumps(definitions)
        }
        
        response = client.call_operation("update_license_definitions", params, timeout=15.0)
        print_result("UPDATE_LICENSE_DEFINITIONS", response)
        
        return True
        
    finally:
        client.disconnect()


if __name__ == "__main__":
    success = test_update_license_definitions()
    sys.exit(0 if success else 1)
