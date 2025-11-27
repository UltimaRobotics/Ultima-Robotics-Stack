#!/usr/bin/env python3
"""
Test RPC operation: add
Add a new VPN instance
"""

import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from base_rpc_test import BaseRPCTest

class TestAddOperation(BaseRPCTest):
    """Test add operation"""
    
    def test_add_openvpn_instance(self):
        """Test adding an OpenVPN instance"""
        config_content = """
client
dev tun
proto udp
remote 10.0.0.1 1194
resolv-retry infinite
nobind
persist-key
persist-tun
ca ca.crt
cert client.crt
key client.key
verb 3
        """.strip()
        
        response = self.send_rpc_request("add", {
            "instance_name": "test_ovpn0",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": False  # Don't auto-start for testing
        })
        
        self.assert_success(response)
        self.assert_contains_fields(response, "success", "message")
        
        result = response['result']
        assert result['success'] == True
        assert "added successfully" in result['message']
        
        print(f"✓ OpenVPN instance added: {result['message']}")
        
    def test_add_wireguard_instance(self):
        """Test adding a WireGuard instance"""
        config_content = """
[Interface]
PrivateKey = abc123def456...
Address = 10.0.0.2/24
DNS = 8.8.8.8

[Peer]
PublicKey = xyz789uvw012...
Endpoint = 10.0.0.1:51820
AllowedIPs = 0.0.0.0/0
        """.strip()
        
        response = self.send_rpc_request("add", {
            "instance_name": "test_wg0",
            "config_content": config_content,
            "vpn_type": "wireguard",
            "auto_start": False
        })
        
        self.assert_success(response)
        
        result = response['result']
        assert result['success'] == True
        assert "added successfully" in result['message']
        
        print(f"✓ WireGuard instance added: {result['message']}")
        
    def test_add_missing_instance_name(self):
        """Test adding instance without instance_name"""
        config_content = "client\ndev tun\nproto udp\nremote 10.0.0.1 1194"
        
        response = self.send_rpc_request("add", {
            "config_content": config_content,
            "vpn_type": "openvpn"
        })
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "Missing 'instance_name'" in result['error']
        
        print(f"✓ Missing instance_name correctly rejected: {result['error']}")
        
    def test_add_missing_config_content(self):
        """Test adding instance without config_content"""
        response = self.send_rpc_request("add", {
            "instance_name": "test_instance",
            "vpn_type": "openvpn"
        })
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "Missing 'config_content'" in result['error']
        
        print(f"✓ Missing config_content correctly rejected: {result['error']}")
        
    def test_add_duplicate_instance(self):
        """Test adding duplicate instance name"""
        config_content = "client\ndev tun\nproto udp\nremote 10.0.0.1 1194"
        
        # Add first instance
        response1 = self.send_rpc_request("add", {
            "instance_name": "duplicate_test",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        # Try to add duplicate
        response2 = self.send_rpc_request("add", {
            "instance_name": "duplicate_test",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        # Second addition should fail
        self.assert_success(response2, expected_success=False)
        
        result = response2['result']
        assert result['success'] == False
        
        print(f"✓ Duplicate instance correctly rejected: {result['error']}")
        
    def test_add_with_auto_start(self):
        """Test adding instance with auto_start enabled"""
        config_content = """
client
dev tun
proto udp
remote 127.0.0.1 1194
auth-user-pass
        """.strip()
        
        response = self.send_rpc_request("add", {
            "instance_name": "auto_start_test",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": True
        })
        
        self.assert_success(response)
        
        result = response['result']
        assert result['success'] == True
        assert "added and started successfully" in result['message']
        
        print(f"✓ Instance added and auto-started: {result['message']}")

def main():
    """Run add operation tests"""
    test = TestAddOperation()
    
    tests = [
        test.test_add_openvpn_instance,
        test.test_add_wireguard_instance,
        test.test_add_missing_instance_name,
        test.test_add_missing_config_content,
        test.test_add_duplicate_instance,
        test.test_add_with_auto_start
    ]
    
    passed = 0
    total = len(tests)
    
    for test_func in tests:
        if test.run_test(test_func):
            passed += 1
            
    print(f"\n📊 Add Operation Tests Results: {passed}/{total} passed")
    return passed == total

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
