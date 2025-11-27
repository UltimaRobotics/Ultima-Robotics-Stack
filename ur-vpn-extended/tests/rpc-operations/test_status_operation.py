#!/usr/bin/env python3
"""
Test RPC operation: status
Get VPN instance status
"""

import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from base_rpc_test import BaseRPCTest

class TestStatusOperation(BaseRPCTest):
    """Test status operation"""
    
    def test_status_all_instances(self):
        """Test getting status of all instances"""
        response = self.send_rpc_request("status", {})
        
        self.assert_success(response)
        self.assert_contains_fields(response, "success", "instances")
        
        result = response['result']
        assert result['success'] == True
        assert "instances" in result
        
        instances = result['instances']
        assert isinstance(instances, list), "Instances should be a list"
        
        print(f"✓ Retrieved status for all instances: {len(instances)} instances found")
        
    def test_status_specific_instance(self):
        """Test getting status of a specific instance"""
        # Add an instance first
        config_content = """
client
dev tun
proto udp
remote 127.0.0.1 1194
        """.strip()
        
        self.send_rpc_request("add", {
            "instance_name": "status_test_instance",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        # Get status of specific instance
        response = self.send_rpc_request("status", {
            "instance_name": "status_test_instance"
        })
        
        self.assert_success(response)
        self.assert_contains_fields(response, "success", "status")
        
        result = response['result']
        assert result['success'] == True
        assert "status" in result
        
        status = result['status']
        assert isinstance(status, dict), "Status should be a dictionary"
        
        print(f"✓ Retrieved status for specific instance: {status}")
        
    def test_status_nonexistent_instance(self):
        """Test getting status of non-existent instance"""
        response = self.send_rpc_request("status", {
            "instance_name": "nonexistent_instance"
        })
        
        # Should succeed but return error in status
        result = response['result']
        print(f"Non-existent instance status result: {result}")
        
    def test_status_empty_instance_name(self):
        """Test getting status with empty instance_name"""
        response = self.send_rpc_request("status", {
            "instance_name": ""
        })
        
        # Should return all instances when empty
        result = response['result']
        if result['success']:
            assert "instances" in result
            print(f"✓ Empty instance_name returns all instances")
        else:
            print(f"Empty instance_name result: {result}")
            
    def test_status_with_running_instance(self):
        """Test status of a running instance"""
        # Add and start an instance
        config_content = """
client
dev tun
proto udp
remote 127.0.0.1 1194
        """.strip()
        
        self.send_rpc_request("add", {
            "instance_name": "running_status_test",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": True
        })
        
        # Get status
        response = self.send_rpc_request("status", {
            "instance_name": "running_status_test"
        })
        
        self.assert_success(response)
        
        result = response['result']
        status = result['status']
        
        # Status should contain information about the running instance
        assert isinstance(status, dict), "Status should be a dictionary"
        
        print(f"✓ Retrieved status for running instance: {status}")

def main():
    """Run status operation tests"""
    test = TestStatusOperation()
    
    tests = [
        test.test_status_all_instances,
        test.test_status_specific_instance,
        test.test_status_nonexistent_instance,
        test.test_status_empty_instance_name,
        test.test_status_with_running_instance
    ]
    
    passed = 0
    total = len(tests)
    
    for test_func in tests:
        if test.run_test(test_func):
            passed += 1
            
    print(f"\n📊 Status Operation Tests Results: {passed}/{total} passed")
    return passed == total

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
