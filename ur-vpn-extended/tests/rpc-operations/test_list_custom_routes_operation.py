#!/usr/bin/env python3
"""
Test RPC operation: list-custom-routes
List all custom routing rules
"""

import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from base_rpc_test import BaseRPCTest

class TestListCustomRoutesOperation(BaseRPCTest):
    """Test list-custom-routes operation"""
    
    def test_list_custom_routes_empty(self):
        """Test listing custom routes when none exist"""
        response = self.send_rpc_request("list-custom-routes", {})
        
        self.assert_success(response)
        self.assert_contains_fields(response, "success", "routing_rules")
        
        result = response['result']
        assert result['success'] == True
        assert "routing_rules" in result
        
        routing_rules = result['routing_rules']
        assert isinstance(routing_rules, list), "Routing rules should be a list"
        
        print(f"✓ Listed custom routes (empty): {len(routing_rules)} routes found")
        
    def test_list_custom_routes_with_data(self):
        """Test listing custom routes when some exist"""
        # Add some routes first
        routes = [
            {
                "id": "list_test_1",
                "vpn_instance": "test_instance",
                "destination": "192.168.1.0/24"
            },
            {
                "id": "list_test_2",
                "vpn_instance": "test_instance",
                "destination": "10.0.0.0/8"
            },
            {
                "id": "list_test_3",
                "vpn_instance": "test_instance",
                "destination": "172.16.0.0/16"
            }
        ]
        
        for route in routes:
            self.send_rpc_request("add-custom-route", route)
        
        # List routes
        response = self.send_rpc_request("list-custom-routes", {})
        
        self.assert_success(response)
        
        result = response['result']
        assert result['success'] == True
        assert "routing_rules" in result
        
        routing_rules = result['routing_rules']
        assert isinstance(routing_rules, list), "Routing rules should be a list"
        assert len(routing_rules) >= len(routes), "Should have at least the routes we added"
        
        print(f"✓ Listed custom routes with data: {len(routing_rules)} routes found")
        
    def test_list_custom_routes_with_parameters(self):
        """Test listing custom routes with extra parameters"""
        # Add a route first
        self.send_rpc_request("add-custom-route", {
            "id": "param_test_route",
            "vpn_instance": "test_instance",
            "destination": "192.168.50.0/24"
        })
        
        # List with extra parameters (should be ignored)
        response = self.send_rpc_request("list-custom-routes", {
            "filter": "enabled",
            "sort": "priority",
            "limit": 10
        })
        
        self.assert_success(response)
        
        result = response['result']
        assert result['success'] == True
        assert "routing_rules" in result
        
        routing_rules = result['routing_rules']
        assert isinstance(routing_rules, list), "Routing rules should be a list"
        
        print(f"✓ Listed custom routes with parameters: {len(routing_rules)} routes found")
        
    def test_list_custom_routes_structure(self):
        """Test that listed custom routes have proper structure"""
        # Add a route with all fields
        full_route = {
            "id": "structure_test_route",
            "name": "Structure Test",
            "vpn_instance": "test_instance",
            "destination": "192.168.100.0/24",
            "priority": 100,
            "enabled": True,
            "description": "Test route structure"
        }
        
        self.send_rpc_request("add-custom-route", full_route)
        
        # List routes
        response = self.send_rpc_request("list-custom-routes", {})
        
        self.assert_success(response)
        
        result = response['result']
        routing_rules = result['routing_rules']
        
        # Find our route in the list
        found_route = None
        for route in routing_rules:
            if route.get('id') == 'structure_test_route':
                found_route = route
                break
                
        assert found_route is not None, "Should find our test route in the list"
        assert isinstance(found_route, dict), "Route should be a dictionary"
        
        # Check that it has expected fields
        expected_fields = ['id', 'vpn_instance', 'destination']
        for field in expected_fields:
            assert field in found_route, f"Route should have {field} field"
        
        print(f"✓ Custom routes have proper structure")
        
    def test_list_custom_routes_consistency(self):
        """Test that list-custom-routes returns consistent data"""
        # List routes twice
        response1 = self.send_rpc_request("list-custom-routes", {})
        response2 = self.send_rpc_request("list-custom-routes", {})
        
        # Both should succeed
        assert response1['result']['success']
        assert response2['result']['success']
        
        rules1 = response1['result']['routing_rules']
        rules2 = response2['result']['routing_rules']
        
        # Should return same number of routes
        assert len(rules1) == len(rules2), "Route count should be consistent"
        
        print(f"✓ List custom routes consistency check passed")

def main():
    """Run list-custom-routes operation tests"""
    test = TestListCustomRoutesOperation()
    
    tests = [
        test.test_list_custom_routes_empty,
        test.test_list_custom_routes_with_data,
        test.test_list_custom_routes_with_parameters,
        test.test_list_custom_routes_structure,
        test.test_list_custom_routes_consistency
    ]
    
    passed = 0
    total = len(tests)
    
    for test_func in tests:
        if test.run_test(test_func):
            passed += 1
            
    print(f"\n📊 List Custom Routes Operation Tests Results: {passed}/{total} passed")
    return passed == total

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
