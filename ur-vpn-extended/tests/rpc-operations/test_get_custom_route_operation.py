#!/usr/bin/env python3
"""
Test RPC operation: get-custom-route
Get a specific custom routing rule
"""

import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from base_rpc_test import BaseRPCTest

class TestGetCustomRouteOperation(BaseRPCTest):
    """Test get-custom-route operation"""
    
    def test_get_custom_route(self):
        """Test getting a specific custom routing rule"""
        # Add a route first
        route_data = {
            "id": "get_test_route",
            "name": "Get Test Route",
            "vpn_instance": "test_instance",
            "destination": "172.16.0.0/16",
            "priority": 200,
            "enabled": True,
            "description": "Test getting specific route"
        }
        
        self.send_rpc_request("add-custom-route", route_data)
        
        # Get the route
        response = self.send_rpc_request("get-custom-route", {
            "id": "get_test_route"
        })
        
        self.assert_success(response)
        self.assert_contains_fields(response, "success", "routing_rule")
        
        result = response['result']
        assert result['success'] == True
        assert "routing_rule" in result
        
        routing_rule = result['routing_rule']
        assert isinstance(routing_rule, dict), "Routing rule should be a dictionary"
        assert routing_rule['id'] == "get_test_route"
        assert routing_rule['name'] == "Get Test Route"
        assert routing_rule['destination'] == "172.16.0.0/16"
        
        print(f"✓ Retrieved custom route: {routing_rule}")
        
    def test_get_nonexistent_custom_route(self):
        """Test getting a non-existent custom route"""
        response = self.send_rpc_request("get-custom-route", {
            "id": "nonexistent_route"
        })
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "error" in result
        
        print(f"✓ Non-existent route correctly rejected: {result['error']}")
        
    def test_get_missing_id(self):
        """Test getting custom route without id"""
        response = self.send_rpc_request("get-custom-route", {})
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "Missing 'id' field" in result['error']
        
        print(f"✓ Missing id correctly rejected: {result['error']}")
        
    def test_get_empty_id(self):
        """Test getting custom route with empty id"""
        response = self.send_rpc_request("get-custom-route", {
            "id": ""
        })
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "Missing 'id' field" in result['error']
        
        print(f"✓ Empty id correctly rejected: {result['error']}")
        
    def test_get_custom_route_full_structure(self):
        """Test getting a custom route with all fields"""
        # Add a route with all possible fields
        full_route = {
            "id": "full_structure_route",
            "name": "Full Structure Route",
            "vpn_instance": "test_instance",
            "vpn_profile": "default",
            "source_type": "IP",
            "source_value": "192.168.1.100",
            "destination": "10.0.0.0/8",
            "gateway": "Custom Gateway",
            "protocol": "tcp",
            "type": "split_tunnel",
            "priority": 50,
            "enabled": True,
            "log_traffic": True,
            "apply_to_existing": False,
            "description": "Full structure test",
            "created_date": "2023-01-01",
            "last_modified": "2023-01-01",
            "is_automatic": False,
            "user_modified": True
        }
        
        self.send_rpc_request("add-custom-route", full_route)
        
        # Get the route
        response = self.send_rpc_request("get-custom-route", {
            "id": "full_structure_route"
        })
        
        self.assert_success(response)
        
        result = response['result']
        routing_rule = result['routing_rule']
        
        # Check that all fields are present
        expected_fields = [
            'id', 'name', 'vpn_instance', 'destination', 'priority', 'enabled'
        ]
        
        for field in expected_fields:
            assert field in routing_rule, f"Route should have {field} field"
        
        print(f"✓ Retrieved full structure custom route with {len(routing_rule)} fields")
        
    def test_get_custom_route_case_sensitivity(self):
        """Test getting custom route with case-sensitive ID"""
        # Add route with mixed case ID
        mixed_case_route = {
            "id": "MixedCase_Route_ID",
            "vpn_instance": "test_instance",
            "destination": "192.168.200.0/24"
        }
        
        self.send_rpc_request("add-custom-route", mixed_case_route)
        
        # Get with exact case
        response1 = self.send_rpc_request("get-custom-route", {
            "id": "MixedCase_Route_ID"
        })
        assert response1['result']['success']
        
        # Try with different case (should fail)
        response2 = self.send_rpc_request("get-custom-route", {
            "id": "mixedcase_route_id"
        })
        assert not response2['result']['success']
        
        print(f"✓ Custom route ID case sensitivity verified")

def main():
    """Run get-custom-route operation tests"""
    test = TestGetCustomRouteOperation()
    
    tests = [
        test.test_get_custom_route,
        test.test_get_nonexistent_custom_route,
        test.test_get_missing_id,
        test.test_get_empty_id,
        test.test_get_custom_route_full_structure,
        test.test_get_custom_route_case_sensitivity
    ]
    
    passed = 0
    total = len(tests)
    
    for test_func in tests:
        if test.run_test(test_func):
            passed += 1
            
    print(f"\n📊 Get Custom Route Operation Tests Results: {passed}/{total} passed")
    return passed == total

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
