#!/usr/bin/env python3
"""
Test RPC operation: add-custom-route
Add a custom routing rule
"""

import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from base_rpc_test import BaseRPCTest

class TestAddCustomRouteOperation(BaseRPCTest):
    """Test add-custom-route operation"""
    
    def test_add_custom_route(self):
        """Test adding a custom routing rule"""
        response = self.send_rpc_request("add-custom-route", {
            "id": "custom_route_001",
            "name": "Test Route",
            "vpn_instance": "test_instance",
            "destination": "192.168.1.0/24",
            "gateway": "VPN Server",
            "priority": 100,
            "enabled": True,
            "description": "Test custom route"
        })
        
        self.assert_success(response)
        self.assert_contains_fields(response, "success", "message")
        
        result = response['result']
        assert result['success'] == True
        assert "added successfully" in result['message']
        
        print(f"✓ Custom route added: {result['message']}")
        
    def test_add_custom_route_minimal(self):
        """Test adding a custom route with minimal required fields"""
        response = self.send_rpc_request("add-custom-route", {
            "id": "minimal_route_001",
            "vpn_instance": "test_instance",
            "destination": "10.0.0.0/8"
        })
        
        self.assert_success(response)
        
        result = response['result']
        assert result['success'] == True
        
        print(f"✓ Minimal custom route added: {result['message']}")
        
    def test_add_custom_route_missing_id(self):
        """Test adding custom route without id"""
        response = self.send_rpc_request("add-custom-route", {
            "vpn_instance": "test_instance",
            "destination": "192.168.1.0/24"
        })
        
        self.assert_success(response, expected_success=False)
        assert "Missing required fields" in response['result']['error']
        
        print(f"✓ Missing id correctly rejected: {response['result']['error']}")
        
    def test_add_custom_route_missing_vpn_instance(self):
        """Test adding custom route without vpn_instance"""
        response = self.send_rpc_request("add-custom-route", {
            "id": "test_route",
            "destination": "192.168.1.0/24"
        })
        
        self.assert_success(response, expected_success=False)
        assert "Missing required fields" in response['result']['error']
        
        print(f"✓ Missing vpn_instance correctly rejected: {response['result']['error']}")
        
    def test_add_custom_route_missing_destination(self):
        """Test adding custom route without destination"""
        response = self.send_rpc_request("add-custom-route", {
            "id": "test_route",
            "vpn_instance": "test_instance"
        })
        
        self.assert_success(response, expected_success=False)
        assert "Missing required fields" in response['result']['error']
        
        print(f"✓ Missing destination correctly rejected: {response['result']['error']}")
        
    def test_add_duplicate_custom_route(self):
        """Test adding duplicate custom route ID"""
        route_data = {
            "id": "duplicate_route_test",
            "vpn_instance": "test_instance",
            "destination": "192.168.1.0/24"
        }
        
        # Add first route
        self.send_rpc_request("add-custom-route", route_data)
        
        # Try to add duplicate
        response = self.send_rpc_request("add-custom-route", route_data)
        
        # Should fail
        self.assert_success(response, expected_success=False)
        
        print(f"✓ Duplicate route correctly rejected: {response['result']['error']}")
        
    def test_add_custom_route_all_parameters(self):
        """Test adding custom route with all possible parameters"""
        response = self.send_rpc_request("add-custom-route", {
            "id": "full_route_001",
            "name": "Complete Route",
            "vpn_instance": "test_instance",
            "vpn_profile": "default",
            "source_type": "IP",
            "source_value": "192.168.1.100",
            "destination": "172.16.0.0/16",
            "gateway": "Custom Gateway",
            "protocol": "tcp",
            "type": "split_tunnel",
            "priority": 50,
            "enabled": True,
            "log_traffic": True,
            "apply_to_existing": True,
            "description": "Complete test route",
            "created_date": "2023-01-01",
            "last_modified": "2023-01-01",
            "is_automatic": False,
            "user_modified": True
        })
        
        self.assert_success(response)
        
        result = response['result']
        assert result['success'] == True
        
        print(f"✓ Full parameter custom route added: {result['message']}")

def main():
    """Run add-custom-route operation tests"""
    test = TestAddCustomRouteOperation()
    
    tests = [
        test.test_add_custom_route,
        test.test_add_custom_route_minimal,
        test.test_add_custom_route_missing_id,
        test.test_add_custom_route_missing_vpn_instance,
        test.test_add_custom_route_missing_destination,
        test.test_add_duplicate_custom_route,
        test.test_add_custom_route_all_parameters
    ]
    
    passed = 0
    total = len(tests)
    
    for test_func in tests:
        if test.run_test(test_func):
            passed += 1
            
    print(f"\n📊 Add Custom Route Operation Tests Results: {passed}/{total} passed")
    return passed == total

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
