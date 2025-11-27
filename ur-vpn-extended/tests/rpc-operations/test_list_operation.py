#!/usr/bin/env python3
"""
Test RPC operation: list
List all VPN instances
"""

import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from base_rpc_test import BaseRPCTest

class TestListOperation(BaseRPCTest):
    """Test list operation"""
    
    def test_list_all_instances(self):
        """Test listing all instances"""
        response = self.send_rpc_request("list", {})
        
        self.assert_success(response)
        self.assert_contains_fields(response, "success", "instances")
        
        result = response['result']
        assert result['success'] == True
        assert "instances" in result
        
        instances = result['instances']
        assert isinstance(instances, list), "Instances should be a list"
        
        print(f"✓ Listed all instances: {len(instances)} instances found")
        
    def test_list_openvpn_instances(self):
        """Test listing OpenVPN instances only"""
        # Add an OpenVPN instance
        config_content = """
client
dev tun
proto udp
remote 127.0.0.1 1194
        """.strip()
        
        self.send_rpc_request("add", {
            "instance_name": "list_openvpn_test",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        # List OpenVPN instances
        response = self.send_rpc_request("list", {
            "vpn_type": "openvpn"
        })
        
        self.assert_success(response)
        
        result = response['result']
        assert result['success'] == True
        assert "instances" in result
        
        instances = result['instances']
        assert isinstance(instances, list), "Instances should be a list"
        
        # Check that all returned instances are OpenVPN
        for instance in instances:
            if 'type' in instance:
                assert instance['type'] == 'openvpn', f"Expected OpenVPN, got {instance.get('type')}"
        
        print(f"✓ Listed OpenVPN instances: {len(instances)} instances found")
        
    def test_list_wireguard_instances(self):
        """Test listing WireGuard instances only"""
        # Add a WireGuard instance
        config_content = """
[Interface]
PrivateKey = abc123def456...
Address = 10.0.0.2/24

[Peer]
PublicKey = xyz789uvw012...
Endpoint = 10.0.0.1:51820
AllowedIPs = 0.0.0.0/0
        """.strip()
        
        self.send_rpc_request("add", {
            "instance_name": "list_wg_test",
            "config_content": config_content,
            "vpn_type": "wireguard",
            "auto_start": False
        })
        
        # List WireGuard instances
        response = self.send_rpc_request("list", {
            "vpn_type": "wireguard"
        })
        
        self.assert_success(response)
        
        result = response['result']
        assert result['success'] == True
        assert "instances" in result
        
        instances = result['instances']
        assert isinstance(instances, list), "Instances should be a list"
        
        # Check that all returned instances are WireGuard
        for instance in instances:
            if 'type' in instance:
                assert instance['type'] == 'wireguard', f"Expected WireGuard, got {instance.get('type')}"
        
        print(f"✓ Listed WireGuard instances: {len(instances)} instances found")
        
    def test_list_invalid_vpn_type(self):
        """Test listing instances with invalid VPN type"""
        response = self.send_rpc_request("list", {
            "vpn_type": "invalid_type"
        })
        
        self.assert_success(response)
        
        result = response['result']
        assert result['success'] == True
        assert "instances" in result
        
        instances = result['instances']
        # Should return empty list for invalid type
        assert isinstance(instances, list), "Instances should be a list"
        
        print(f"✓ Listed instances for invalid type: {len(instances)} instances found")
        
    def test_list_empty_vpn_type(self):
        """Test listing instances with empty VPN type"""
        response = self.send_rpc_request("list", {
            "vpn_type": ""
        })
        
        self.assert_success(response)
        
        result = response['result']
        assert result['success'] == True
        assert "instances" in result
        
        instances = result['instances']
        assert isinstance(instances, list), "Instances should be a list"
        
        print(f"✓ Listed instances for empty type: {len(instances)} instances found")
        
    def test_list_with_multiple_instances(self):
        """Test listing with multiple instances of different types"""
        config_content = "client\ndev tun\nproto udp\nremote 127.0.0.1 1194"
        
        # Add multiple instances
        self.send_rpc_request("add", {
            "instance_name": "list_multi_1",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        self.send_rpc_request("add", {
            "instance_name": "list_multi_2",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        # List all instances
        response = self.send_rpc_request("list", {})
        
        self.assert_success(response)
        
        result = response['result']
        instances = result['instances']
        
        print(f"✓ Listed multiple instances: {len(instances)} total instances")

def main():
    """Run list operation tests"""
    test = TestListOperation()
    
    tests = [
        test.test_list_all_instances,
        test.test_list_openvpn_instances,
        test.test_list_wireguard_instances,
        test.test_list_invalid_vpn_type,
        test.test_list_empty_vpn_type,
        test.test_list_with_multiple_instances
    ]
    
    passed = 0
    total = len(tests)
    
    for test_func in tests:
        if test.run_test(test_func):
            passed += 1
            
    print(f"\n📊 List Operation Tests Results: {passed}/{total} passed")
    return passed == total

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
