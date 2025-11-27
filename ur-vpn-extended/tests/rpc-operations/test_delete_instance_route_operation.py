#!/usr/bin/env python3
"""
Test RPC operation: delete-instance-route
Delete a route rule from a specific instance
"""

import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from base_rpc_test import BaseRPCTest

class TestDeleteInstanceRouteOperation(BaseRPCTest):
    """Test delete-instance-route operation"""
    
    def test_delete_instance_route(self):
        """Test deleting a route from a specific instance"""
        # Add an instance and route
        config_content = """
client
dev tun
proto udp
remote 127.0.0.1 1194
        """.strip()
        
        self.send_rpc_request("add", {
            "instance_name": "delete_route_instance",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        route_rule = {
            "id": "delete_route_test",
            "destination": "192.168.2.0/24"
        }
        
        self.send_rpc_request("add-instance-route", {
            "instance_name": "delete_route_instance",
            "route_rule": route_rule
        })
        
        # Delete the route
        response = self.send_rpc_request("delete-instance-route", {
            "instance_name": "delete_route_instance",
            "rule_id": "delete_route_test"
        })
        
        self.assert_success(response)
        
        result = response['result']
        assert result['success'] == True
        
        print(f"✓ Instance route deleted successfully")
        
    def test_delete_missing_instance_name(self):
        """Test deleting route without instance_name"""
        response = self.send_rpc_request("delete-instance-route", {
            "rule_id": "test_route"
        })
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "Missing 'instance_name'" in result['error']
        
        print(f"✓ Missing instance_name correctly rejected: {result['error']}")
        
    def test_delete_missing_rule_id(self):
        """Test deleting route without rule_id"""
        response = self.send_rpc_request("delete-instance-route", {
            "instance_name": "test_instance"
        })
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "Missing 'rule_id'" in result['error']
        
        print(f"✓ Missing rule_id correctly rejected: {result['error']}")
        
    def test_delete_nonexistent_instance_route(self):
        """Test deleting non-existent route from instance"""
        response = self.send_rpc_request("delete-instance-route", {
            "instance_name": "test_instance",
            "rule_id": "nonexistent_route"
        })
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "Rule not found" in result['error']
        
        print(f"✓ Non-existent route deletion correctly rejected: {result['error']}")
        
    def test_delete_route_from_nonexistent_instance(self):
        """Test deleting route from non-existent instance"""
        response = self.send_rpc_request("delete-instance-route", {
            "instance_name": "nonexistent_instance",
            "rule_id": "test_route"
        })
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        
        print(f"✓ Route from non-existent instance correctly rejected: {result['error']}")
        
    def test_delete_multiple_routes_from_instance(self):
        """Test deleting multiple routes from instance"""
        # Add instance
        config_content = "client\ndev tun\nproto udp\nremote 127.0.0.1 1194"
        
        self.send_rpc_request("add", {
            "instance_name": "multi_delete_instance",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        # Add multiple routes
        routes = [
            {"id": "multi_delete_1", "destination": "192.168.1.0/24"},
            {"id": "multi_delete_2", "destination": "192.168.2.0/24"},
            {"id": "multi_delete_3", "destination": "192.168.3.0/24"}
        ]
        
        for route in routes:
            self.send_rpc_request("add-instance-route", {
                "instance_name": "multi_delete_instance",
                "route_rule": route
            })
        
        # Delete all routes
        deleted_count = 0
        for route in routes:
            response = self.send_rpc_request("delete-instance-route", {
                "instance_name": "multi_delete_instance",
                "rule_id": route["id"]
            })
            
            if response['result']['success']:
                deleted_count += 1
                
        print(f"✓ Deleted {deleted_count}/{len(routes)} routes from instance")

def main():
    """Run delete-instance-route operation tests"""
    test = TestDeleteInstanceRouteOperation()
    
    tests = [
        test.test_delete_instance_route,
        test.test_delete_missing_instance_name,
        test.test_delete_missing_rule_id,
        test.test_delete_nonexistent_instance_route,
        test.test_delete_route_from_nonexistent_instance,
        test.test_delete_multiple_routes_from_instance
    ]
    
    passed = 0
    total = len(tests)
    
    for test_func in tests:
        if test.run_test(test_func):
            passed += 1
            
    print(f"\n📊 Delete Instance Route Operation Tests Results: {passed}/{total} passed")
    return passed == total

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
