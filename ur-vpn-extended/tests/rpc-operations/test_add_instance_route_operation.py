#!/usr/bin/env python3
"""
Test RPC operation: add-instance-route
Add a route rule to a specific instance
"""

import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from base_rpc_test import BaseRPCTest

class TestAddInstanceRouteOperation(BaseRPCTest):
    """Test add-instance-route operation"""
    
    def test_add_instance_route(self):
        """Test adding a route to a specific instance"""
        # Add an instance first
        config_content = """
client
dev tun
proto udp
remote 127.0.0.1 1194
        """.strip()
        
        self.send_rpc_request("add", {
            "instance_name": "add_route_instance",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        # Add route to instance
        route_rule = {
            "id": "instance_route_001",
            "destination": "192.168.1.0/24",
            "type": "tunnel_all",
            "priority": 100
        }
        
        response = self.send_rpc_request("add-instance-route", {
            "instance_name": "add_route_instance",
            "route_rule": route_rule
        })
        
        self.assert_success(response)
        
        result = response['result']
        assert result['success'] == True
        
        print(f"✓ Instance route added successfully")
        
    def test_add_missing_instance_name(self):
        """Test adding route without instance_name"""
        route_rule = {
            "id": "test_route",
            "destination": "192.168.1.0/24"
        }
        
        response = self.send_rpc_request("add-instance-route", {
            "route_rule": route_rule
        })
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "Missing 'instance_name'" in result['error']
        
        print(f"✓ Missing instance_name correctly rejected: {result['error']}")
        
    def test_add_missing_route_rule(self):
        """Test adding route without route_rule"""
        response = self.send_rpc_request("add-instance-route", {
            "instance_name": "test_instance"
        })
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "Missing 'route_rule'" in result['error']
        
        print(f"✓ Missing route_rule correctly rejected: {result['error']}")
        
    def test_add_route_nonexistent_instance(self):
        """Test adding route to non-existent instance"""
        route_rule = {
            "id": "test_route",
            "destination": "192.168.1.0/24"
        }
        
        response = self.send_rpc_request("add-instance-route", {
            "instance_name": "nonexistent_instance",
            "route_rule": route_rule
        })
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        
        print(f"✓ Non-existent instance correctly rejected: {result['error']}")
        
    def test_add_duplicate_route_to_instance(self):
        """Test adding duplicate route to same instance"""
        # Add instance
        config_content = "client\ndev tun\nproto udp\nremote 127.0.0.1 1194"
        
        self.send_rpc_request("add", {
            "instance_name": "duplicate_route_instance",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        route_rule = {
            "id": "duplicate_route",
            "destination": "192.168.1.0/24"
        }
        
        # Add first route
        response1 = self.send_rpc_request("add-instance-route", {
            "instance_name": "duplicate_route_instance",
            "route_rule": route_rule
        })
        assert response1['result']['success']
        
        # Try to add duplicate
        response2 = self.send_rpc_request("add-instance-route", {
            "instance_name": "duplicate_route_instance",
            "route_rule": route_rule
        })
        
        # Should fail
        self.assert_success(response2, expected_success=False)
        
        print(f"✓ Duplicate route correctly rejected: {response2['result']['error']}")
        
    def test_add_multiple_routes_to_instance(self):
        """Test adding multiple routes to same instance"""
        # Add instance
        config_content = "client\ndev tun\nproto udp\nremote 127.0.0.1 1194"
        
        self.send_rpc_request("add", {
            "instance_name": "multi_route_instance",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        # Add multiple routes
        routes = [
            {"id": "multi_route_1", "destination": "192.168.1.0/24", "type": "tunnel_all"},
            {"id": "multi_route_2", "destination": "10.0.0.0/8", "type": "split_tunnel"},
            {"id": "multi_route_3", "destination": "172.16.0.0/16", "type": "full_tunnel"}
        ]
        
        added_count = 0
        for route in routes:
            response = self.send_rpc_request("add-instance-route", {
                "instance_name": "multi_route_instance",
                "route_rule": route
            })
            
            if response['result']['success']:
                added_count += 1
                
        print(f"✓ Added {added_count}/{len(routes)} routes to instance")

def main():
    """Run add-instance-route operation tests"""
    test = TestAddInstanceRouteOperation()
    
    tests = [
        test.test_add_instance_route,
        test.test_add_missing_instance_name,
        test.test_add_missing_route_rule,
        test.test_add_route_nonexistent_instance,
        test.test_add_duplicate_route_to_instance,
        test.test_add_multiple_routes_to_instance
    ]
    
    passed = 0
    total = len(tests)
    
    for test_func in tests:
        if test.run_test(test_func):
            passed += 1
            
    print(f"\n📊 Add Instance Route Operation Tests Results: {passed}/{total} passed")
    return passed == total

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
