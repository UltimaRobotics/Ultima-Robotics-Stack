#!/usr/bin/env python3
"""
Test RPC operation: delete
Delete an existing VPN instance
"""

import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from base_rpc_test import BaseRPCTest

class TestDeleteOperation(BaseRPCTest):
    """Test delete operation"""
    
    def test_delete_existing_instance(self):
        """Test deleting an existing instance"""
        # First add an instance to delete
        config_content = """
client
dev tun
proto udp
remote 10.0.0.1 1194
        """.strip()
        
        add_response = self.send_rpc_request("add", {
            "instance_name": "delete_test_instance",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        # Now delete it
        response = self.send_rpc_request("delete", {
            "instance_name": "delete_test_instance"
        })
        
        self.assert_success(response)
        self.assert_contains_fields(response, "success", "message")
        
        result = response['result']
        assert result['success'] == True
        assert "deleted successfully" in result['message']
        
        print(f"✓ Instance deleted successfully: {result['message']}")
        
    def test_delete_nonexistent_instance(self):
        """Test deleting a non-existent instance"""
        response = self.send_rpc_request("delete", {
            "instance_name": "nonexistent_instance"
        })
        
        # Should fail
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        
        print(f"✓ Non-existent instance correctly rejected: {result['error']}")
        
    def test_delete_missing_instance_name(self):
        """Test deleting without instance_name parameter"""
        response = self.send_rpc_request("delete", {})
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "Missing 'instance_name'" in result['error']
        
        print(f"✓ Missing instance_name correctly rejected: {result['error']}")
        
    def test_delete_empty_instance_name(self):
        """Test deleting with empty instance_name"""
        response = self.send_rpc_request("delete", {
            "instance_name": ""
        })
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "Missing 'instance_name'" in result['error']
        
        print(f"✓ Empty instance_name correctly rejected: {result['error']}")
        
    def test_delete_running_instance(self):
        """Test deleting a running instance"""
        # Add and start an instance
        config_content = """
client
dev tun
proto udp
remote 127.0.0.1 1194
        """.strip()
        
        # Add instance
        self.send_rpc_request("add", {
            "instance_name": "running_delete_test",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        # Start instance
        start_response = self.send_rpc_request("start", {
            "instance_name": "running_delete_test"
        })
        
        # Delete running instance
        response = self.send_rpc_request("delete", {
            "instance_name": "running_delete_test"
        })
        
        self.assert_success(response)
        
        result = response['result']
        assert result['success'] == True
        assert "deleted successfully" in result['message']
        
        print(f"✓ Running instance deleted successfully: {result['message']}")
        
    def test_delete_multiple_instances(self):
        """Test deleting multiple instances"""
        config_content = "client\ndev tun\nproto udp\nremote 10.0.0.1 1194"
        
        # Add multiple instances
        instances = ["multi_delete_1", "multi_delete_2", "multi_delete_3"]
        
        for instance in instances:
            self.send_rpc_request("add", {
                "instance_name": instance,
                "config_content": config_content,
                "vpn_type": "openvpn",
                "auto_start": False
            })
            
        # Delete all instances
        deleted_count = 0
        for instance in instances:
            response = self.send_rpc_request("delete", {
                "instance_name": instance
            })
            
            if response['result']['success']:
                deleted_count += 1
                
        assert deleted_count == len(instances), f"Expected {len(instances)} deletions, got {deleted_count}"
        
        print(f"✓ Successfully deleted {deleted_count} instances")

def main():
    """Run delete operation tests"""
    test = TestDeleteOperation()
    
    tests = [
        test.test_delete_existing_instance,
        test.test_delete_nonexistent_instance,
        test.test_delete_missing_instance_name,
        test.test_delete_empty_instance_name,
        test.test_delete_running_instance,
        test.test_delete_multiple_instances
    ]
    
    passed = 0
    total = len(tests)
    
    for test_func in tests:
        if test.run_test(test_func):
            passed += 1
            
    print(f"\n📊 Delete Operation Tests Results: {passed}/{total} passed")
    return passed == total

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
