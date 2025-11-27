#!/usr/bin/env python3
"""
Test RPC operation: disable
Disable and stop a VPN instance
"""

import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from base_rpc_test import BaseRPCTest

class TestDisableOperation(BaseRPCTest):
    """Test disable operation"""
    
    def test_disable_enabled_instance(self):
        """Test disabling an enabled instance"""
        # Add and start an instance
        config_content = """
client
dev tun
proto udp
remote 127.0.0.1 1194
        """.strip()
        
        self.send_rpc_request("add", {
            "instance_name": "disable_test",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": True
        })
        
        # Disable the instance
        response = self.send_rpc_request("disable", {
            "instance_name": "disable_test"
        })
        
        self.assert_success(response)
        self.assert_contains_fields(response, "success", "message")
        
        result = response['result']
        assert result['success'] == True
        assert "disabled and stopped" in result['message'].lower()
        
        print(f"✓ Instance disabled and stopped: {result['message']}")
        
    def test_disable_nonexistent_instance(self):
        """Test disabling non-existent instance"""
        response = self.send_rpc_request("disable", {
            "instance_name": "nonexistent_instance"
        })
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "Missing 'instance_name'" in result['error'] or "not found" in result['error']
        
        print(f"✓ Non-existent instance disable correctly rejected: {result['error']}")
        
    def test_disable_missing_instance_name(self):
        """Test disabling without instance_name parameter"""
        response = self.send_rpc_request("disable", {})
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "Missing 'instance_name'" in result['error']
        
        print(f"✓ Missing instance_name for disable correctly rejected: {result['error']}")
        
    def test_disable_already_disabled_instance(self):
        """Test disabling an already disabled instance"""
        config_content = """
client
dev tun
proto udp
remote 127.0.0.1 1194
        """.strip()
        
        # Add instance without auto-start
        self.send_rpc_request("add", {
            "instance_name": "already_disabled",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        # Try to disable
        response = self.send_rpc_request("disable", {
            "instance_name": "already_disabled"
        })
        
        # Should either succeed or fail gracefully
        result = response['result']
        print(f"Disable already disabled result: {result}")
        
    def test_disable_multiple_instances(self):
        """Test disabling multiple instances"""
        config_content = """
client
dev tun
proto udp
remote 127.0.0.1 1194
        """.strip()
        
        # Add and start multiple instances
        instances = ["multi_disable_1", "multi_disable_2", "multi_disable_3"]
        
        for instance in instances:
            self.send_rpc_request("add", {
                "instance_name": instance,
                "config_content": config_content,
                "vpn_type": "openvpn",
                "auto_start": True
            })
            
        # Disable all instances
        disabled_count = 0
        for instance in instances:
            response = self.send_rpc_request("disable", {
                "instance_name": instance
            })
            
            if response['result']['success']:
                disabled_count += 1
                
        print(f"✓ Disabled {disabled_count}/{len(instances)} instances")

def main():
    """Run disable operation tests"""
    test = TestDisableOperation()
    
    tests = [
        test.test_disable_enabled_instance,
        test.test_disable_nonexistent_instance,
        test.test_disable_missing_instance_name,
        test.test_disable_already_disabled_instance,
        test.test_disable_multiple_instances
    ]
    
    passed = 0
    total = len(tests)
    
    for test_func in tests:
        if test.run_test(test_func):
            passed += 1
            
    print(f"\n📊 Disable Operation Tests Results: {passed}/{total} passed")
    return passed == total

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
