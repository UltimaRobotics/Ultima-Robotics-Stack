#!/usr/bin/env python3
"""
Test RPC operation: update
Update an existing VPN instance configuration
"""

import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from base_rpc_test import BaseRPCTest

class TestUpdateOperation(BaseRPCTest):
    """Test update operation"""
    
    def test_update_existing_instance(self):
        """Test updating an existing instance"""
        # First add an instance
        original_config = """
client
dev tun
proto udp
remote 10.0.0.1 1194
        """.strip()
        
        self.send_rpc_request("add", {
            "instance_name": "update_test_instance",
            "config_content": original_config,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        # Update the instance with new config
        updated_config = """
client
dev tun
proto tcp
remote 10.0.0.2 443
auth-user-pass
verb 4
        """.strip()
        
        response = self.send_rpc_request("update", {
            "instance_name": "update_test_instance",
            "config_content": updated_config
        })
        
        self.assert_success(response)
        self.assert_contains_fields(response, "success", "message")
        
        result = response['result']
        assert result['success'] == True
        assert "updated and restarted successfully" in result['message']
        
        print(f"✓ Instance updated successfully: {result['message']}")
        
    def test_update_nonexistent_instance(self):
        """Test updating a non-existent instance"""
        config_content = """
client
dev tun
proto udp
remote 10.0.0.1 1194
        """.strip()
        
        response = self.send_rpc_request("update", {
            "instance_name": "nonexistent_instance",
            "config_content": config_content
        })
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        
        print(f"✓ Non-existent instance update correctly rejected: {result['error']}")
        
    def test_update_missing_instance_name(self):
        """Test updating without instance_name parameter"""
        config_content = "client\ndev tun\nproto udp\nremote 10.0.0.1 1194"
        
        response = self.send_rpc_request("update", {
            "config_content": config_content
        })
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "Missing 'instance_name'" in result['error']
        
        print(f"✓ Missing instance_name correctly rejected: {result['error']}")
        
    def test_update_missing_config_content(self):
        """Test updating without config_content parameter"""
        response = self.send_rpc_request("update", {
            "instance_name": "test_instance"
        })
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "Missing 'config_content'" in result['error']
        
        print(f"✓ Missing config_content correctly rejected: {result['error']}")
        
    def test_update_empty_config_content(self):
        """Test updating with empty config_content"""
        response = self.send_rpc_request("update", {
            "instance_name": "test_instance",
            "config_content": ""
        })
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "Missing 'config_content'" in result['error']
        
        print(f"✓ Empty config_content correctly rejected: {result['error']}")
        
    def test_update_running_instance(self):
        """Test updating a running instance"""
        # Add and start an instance
        original_config = """
client
dev tun
proto udp
remote 127.0.0.1 1194
        """.strip()
        
        self.send_rpc_request("add", {
            "instance_name": "running_update_test",
            "config_content": original_config,
            "vpn_type": "openvpn",
            "auto_start": True
        })
        
        # Update the running instance
        updated_config = """
client
dev tun
proto tcp
remote 127.0.0.1 443
        """.strip()
        
        response = self.send_rpc_request("update", {
            "instance_name": "running_update_test",
            "config_content": updated_config
        })
        
        self.assert_success(response)
        
        result = response['result']
        assert result['success'] == True
        assert "updated and restarted successfully" in result['message']
        
        print(f"✓ Running instance updated and restarted: {result['message']}")
        
    def test_update_large_configuration(self):
        """Test updating with large configuration"""
        # Add instance
        simple_config = "client\ndev tun\nproto udp\nremote 10.0.0.1 1194"
        
        self.send_rpc_request("add", {
            "instance_name": "large_update_test",
            "config_content": simple_config,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        # Create large configuration
        large_config_lines = ["client", "dev tun", "proto udp", "remote 10.0.0.1 1194"]
        for i in range(100):
            large_config_lines.append(f"# Route {i}")
            large_config_lines.append(f"route 192.168.{i//256}.{i%256} 255.255.255.0 net_gateway")
        
        large_config = "\n".join(large_config_lines)
        
        response = self.send_rpc_request("update", {
            "instance_name": "large_update_test",
            "config_content": large_config
        })
        
        self.assert_success(response)
        
        result = response['result']
        assert result['success'] == True
        
        print(f"✓ Large configuration updated successfully: {len(large_config)} characters")

def main():
    """Run update operation tests"""
    test = TestUpdateOperation()
    
    tests = [
        test.test_update_existing_instance,
        test.test_update_nonexistent_instance,
        test.test_update_missing_instance_name,
        test.test_update_missing_config_content,
        test.test_update_empty_config_content,
        test.test_update_running_instance,
        test.test_update_large_configuration
    ]
    
    passed = 0
    total = len(tests)
    
    for test_func in tests:
        if test.run_test(test_func):
            passed += 1
            
    print(f"\n📊 Update Operation Tests Results: {passed}/{total} passed")
    return passed == total

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
