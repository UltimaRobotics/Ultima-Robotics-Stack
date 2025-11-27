#!/usr/bin/env python3
"""
Test RPC operation: get-instance-routes
Get routing rules for a specific instance
"""

import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from base_rpc_test import BaseRPCTest

class TestGetInstanceRoutesOperation(BaseRPCTest):
    """Test get-instance-routes operation"""
    
    def test_get_instance_routes(self):
        """Test getting routes for a specific instance"""
        # Add an instance first
        config_content = """
client
dev tun
proto udp
remote 127.0.0.1 1194
        """.strip()
        
        self.send_rpc_request("add", {
            "instance_name": "route_test_instance",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        # Get instance routes
        response = self.send_rpc_request("get-instance-routes", {
            "instance_name": "route_test_instance"
        })
        
        self.assert_success(response)
        self.assert_contains_fields(response, "success", "routing_rules")
        
        result = response['result']
        assert result['success'] == True
        assert "routing_rules" in result
        
        routing_rules = result['routing_rules']
        assert isinstance(routing_rules, list), "Routing rules should be a list"
        
        print(f"✓ Retrieved instance routes: {len(routing_rules)} routes found")
        
    def test_get_nonexistent_instance_routes(self):
        """Test getting routes for non-existent instance"""
        response = self.send_rpc_request("get-instance-routes", {
            "instance_name": "nonexistent_instance"
        })
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "error" in result
        
        print(f"✓ Non-existent instance routes correctly rejected: {result['error']}")
        
    def test_get_missing_instance_name(self):
        """Test getting routes without instance_name"""
        response = self.send_rpc_request("get-instance-routes", {})
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "Missing 'instance_name'" in result['error']
        
        print(f"✓ Missing instance_name correctly rejected: {result['error']}")
        
    def test_get_empty_instance_name(self):
        """Test getting routes with empty instance_name"""
        response = self.send_rpc_request("get-instance-routes", {
            "instance_name": ""
        })
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "Missing 'instance_name'" in result['error']
        
        print(f"✓ Empty instance_name correctly rejected: {result['error']}")
        
    def test_get_instance_routes_with_added_routes(self):
        """Test getting routes for instance with added routes"""
        # Add an instance
        config_content = """
client
dev tun
proto udp
remote 127.0.0.1 1194
        """.strip()
        
        self.send_rpc_request("add", {
            "instance_name": "routes_with_added",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        # Add some routes to the instance
        routes = [
            {"id": "instance_route_1", "destination": "192.168.1.0/24", "type": "tunnel_all"},
            {"id": "instance_route_2", "destination": "10.0.0.0/8", "type": "split_tunnel"}
        ]
        
        for route in routes:
            self.send_rpc_request("add-instance-route", {
                "instance_name": "routes_with_added",
                "route_rule": route
            })
        
        # Get instance routes
        response = self.send_rpc_request("get-instance-routes", {
            "instance_name": "routes_with_added"
        })
        
        self.assert_success(response)
        
        result = response['result']
        routing_rules = result['routing_rules']
        assert isinstance(routing_rules, list), "Routing rules should be a list"
        
        print(f"✓ Retrieved instance routes with added routes: {len(routing_rules)} routes found")
        
    def test_get_instance_routes_structure(self):
        """Test that instance routes have proper structure"""
        # Add instance
        config_content = "client\ndev tun\nproto udp\nremote 127.0.0.1 1194"
        
        self.send_rpc_request("add", {
            "instance_name": "structure_test_instance",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        # Add a route with full structure
        full_route = {
            "id": "structure_route",
            "destination": "172.16.0.0/16",
            "type": "tunnel_all",
            "priority": 100,
            "enabled": True
        }
        
        self.send_rpc_request("add-instance-route", {
            "instance_name": "structure_test_instance",
            "route_rule": full_route
        })
        
        # Get routes
        response = self.send_rpc_request("get-instance-routes", {
            "instance_name": "structure_test_instance"
        })
        
        self.assert_success(response)
        
        result = response['result']
        routing_rules = result['routing_rules']
        
        # Find our route
        found_route = None
        for route in routing_rules:
            if route.get('id') == 'structure_route':
                found_route = route
                break
                
        if found_route:
            assert isinstance(found_route, dict), "Route should be a dictionary"
            expected_fields = ['id', 'destination']
            for field in expected_fields:
                assert field in found_route, f"Route should have {field} field"
        
        print(f"✓ Instance routes have proper structure")

def main():
    """Run get-instance-routes operation tests"""
    test = TestGetInstanceRoutesOperation()
    
    tests = [
        test.test_get_instance_routes,
        test.test_get_nonexistent_instance_routes,
        test.test_get_missing_instance_name,
        test.test_get_empty_instance_name,
        test.test_get_instance_routes_with_added_routes,
        test.test_get_instance_routes_structure
    ]
    
    passed = 0
    total = len(tests)
    
    for test_func in tests:
        if test.run_test(test_func):
            passed += 1
            
    print(f"\n📊 Get Instance Routes Operation Tests Results: {passed}/{total} passed")
    return passed == total

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
