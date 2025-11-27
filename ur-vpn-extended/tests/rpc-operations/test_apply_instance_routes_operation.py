#!/usr/bin/env python3
"""
Test RPC operation: apply-instance-routes
Apply routing rules for a specific instance
"""

import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from base_rpc_test import BaseRPCTest

class TestApplyInstanceRoutesOperation(BaseRPCTest):
    """Test apply-instance-routes operation"""
    
    def test_apply_instance_routes(self):
        """Test applying routes for a specific instance"""
        # Add an instance
        config_content = """
client
dev tun
proto udp
remote 127.0.0.1 1194
        """.strip()
        
        self.send_rpc_request("add", {
            "instance_name": "apply_routes_instance",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        # Apply routes
        response = self.send_rpc_request("apply-instance-routes", {
            "instance_name": "apply_routes_instance"
        })
        
        self.assert_success(response)
        
        result = response['result']
        assert result['success'] == True
        
        print(f"✓ Instance routes applied successfully")
        
    def test_apply_missing_instance_name(self):
        """Test applying routes without instance_name"""
        response = self.send_rpc_request("apply-instance-routes", {})
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "Missing 'instance_name'" in result['error']
        
        print(f"✓ Missing instance_name correctly rejected: {result['error']}")
        
    def test_apply_empty_instance_name(self):
        """Test applying routes with empty instance_name"""
        response = self.send_rpc_request("apply-instance-routes", {
            "instance_name": ""
        })
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "Missing 'instance_name'" in result['error']
        
        print(f"✓ Empty instance_name correctly rejected: {result['error']}")
        
    def test_apply_nonexistent_instance_routes(self):
        """Test applying routes for non-existent instance"""
        response = self.send_rpc_request("apply-instance-routes", {
            "instance_name": "nonexistent_instance"
        })
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        
        print(f"✓ Non-existent instance routes apply correctly rejected: {result['error']}")
        
    def test_apply_routes_with_existing_routes(self):
        """Test applying routes for instance with existing routes"""
        # Add instance
        config_content = "client\ndev tun\nproto udp\nremote 127.0.0.1 1194"
        
        self.send_rpc_request("add", {
            "instance_name": "apply_existing_routes",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        # Add some routes
        routes = [
            {"id": "apply_route_1", "destination": "192.168.1.0/24"},
            {"id": "apply_route_2", "destination": "10.0.0.0/8"}
        ]
        
        for route in routes:
            self.send_rpc_request("add-instance-route", {
                "instance_name": "apply_existing_routes",
                "route_rule": route
            })
        
        # Apply routes
        response = self.send_rpc_request("apply-instance-routes", {
            "instance_name": "apply_existing_routes"
        })
        
        self.assert_success(response)
        
        result = response['result']
        assert result['success'] == True
        
        print(f"✓ Routes applied for instance with existing routes")
        
    def test_apply_routes_multiple_times(self):
        """Test applying routes multiple times"""
        # Add instance
        config_content = "client\ndev tun\nproto udp\nremote 127.0.0.1 1194"
        
        self.send_rpc_request("add", {
            "instance_name": "apply_multiple_times",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        # Apply routes multiple times
        for i in range(3):
            response = self.send_rpc_request("apply-instance-routes", {
                "instance_name": "apply_multiple_times"
            })
            
            if response['result']['success']:
                print(f"✓ Route application {i+1} successful")
            else:
                print(f"⚠️ Route application {i+1} failed: {response['result']}")
        
    def test_apply_routes_with_parameters(self):
        """Test applying routes with extra parameters"""
        # Add instance
        config_content = "client\ndev tun\nproto udp\nremote 127.0.0.1 1194"
        
        self.send_rpc_request("add", {
            "instance_name": "apply_with_params",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        # Apply routes with extra parameters
        response = self.send_rpc_request("apply-instance-routes", {
            "instance_name": "apply_with_params",
            "force": True,
            "dry_run": False
        })
        
        self.assert_success(response)
        
        result = response['result']
        assert result['success'] == True
        
        print(f"✓ Routes applied with extra parameters")

def main():
    """Run apply-instance-routes operation tests"""
    test = TestApplyInstanceRoutesOperation()
    
    tests = [
        test.test_apply_instance_routes,
        test.test_apply_missing_instance_name,
        test.test_apply_empty_instance_name,
        test.test_apply_nonexistent_instance_routes,
        test.test_apply_routes_with_existing_routes,
        test.test_apply_routes_multiple_times,
        test.test_apply_routes_with_parameters
    ]
    
    passed = 0
    total = len(tests)
    
    for test_func in tests:
        if test.run_test(test_func):
            passed += 1
            
    print(f"\n📊 Apply Instance Routes Operation Tests Results: {passed}/{total} passed")
    return passed == total

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
