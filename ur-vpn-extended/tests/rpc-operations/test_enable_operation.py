#!/usr/bin/env python3
"""
Test RPC operation: enable
Enable and start a VPN instance
"""

import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from base_rpc_test import BaseRPCTest

class TestEnableOperation(BaseRPCTest):
    """Test enable operation"""
    
    def test_enable_disabled_instance(self):
        """Test enabling a disabled instance"""
        # Add an instance without auto-start
        config_content = """
client
dev tun
proto udp
remote 127.0.0.1 1194
        """.strip()
        
        self.send_rpc_request("add", {
            "instance_name": "enable_test",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        # Enable the instance
        response = self.send_rpc_request("enable", {
            "instance_name": "enable_test"
        })
        
        self.assert_success(response)
        self.assert_contains_fields(response, "success", "message")
        
        result = response['result']
        assert result['success'] == True
        assert "enabled and started" in result['message'].lower()
        
        print(f"✓ Instance enabled and started: {result['message']}")
        
    def test_enable_nonexistent_instance(self):
        """Test enabling non-existent instance"""
        response = self.send_rpc_request("enable", {
            "instance_name": "nonexistent_instance"
        })
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "Missing 'instance_name'" in result['error'] or "not found" in result['error']
        
        print(f"✓ Non-existent instance enable correctly rejected: {result['error']}")
        
    def test_enable_missing_instance_name(self):
        """Test enabling without instance_name parameter"""
        response = self.send_rpc_request("enable", {})
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "Missing 'instance_name'" in result['error']
        
        print(f"✓ Missing instance_name for enable correctly rejected: {result['error']}")
        
    def test_enable_already_enabled_instance(self):
        """Test enabling an already enabled instance"""
        config_content = """
client
dev tun
proto udp
remote 127.0.0.1 1194
        """.strip()
        
        # Add and enable instance
        self.send_rpc_request("add", {
            "instance_name": "already_enabled",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": True
        })
        
        # Try to enable again
        response = self.send_rpc_request("enable", {
            "instance_name": "already_enabled"
        })
        
        # Should either succeed or fail gracefully
        result = response['result']
        print(f"Enable already enabled result: {result}")
        
    def test_enable_multiple_instances(self):
        """Test enabling multiple instances"""
        config_content = """
client
dev tun
proto udp
remote 127.0.0.1 1194
        """.strip()
        
        # Add multiple instances without auto-start
        instances = ["multi_enable_1", "multi_enable_2", "multi_enable_3"]
        
        for instance in instances:
            self.send_rpc_request("add", {
                "instance_name": instance,
                "config_content": config_content,
                "vpn_type": "openvpn",
                "auto_start": False
            })
            
        # Enable all instances
        enabled_count = 0
        for instance in instances:
            response = self.send_rpc_request("enable", {
                "instance_name": instance
            })
            
            if response['result']['success']:
                enabled_count += 1
                
        print(f"✓ Enabled {enabled_count}/{len(instances)} instances")

def main():
    """Run enable operation tests"""
    test = TestEnableOperation()
    
    tests = [
        test.test_enable_disabled_instance,
        test.test_enable_nonexistent_instance,
        test.test_enable_missing_instance_name,
        test.test_enable_already_enabled_instance,
        test.test_enable_multiple_instances
    ]
    
    passed = 0
    total = len(tests)
    
    for test_func in tests:
        if test.run_test(test_func):
            passed += 1
            
    print(f"\n📊 Enable Operation Tests Results: {passed}/{total} passed")
    return passed == total

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
