#!/usr/bin/env python3
"""
Test RPC operation: start
Start a VPN instance
"""

import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from base_rpc_test import BaseRPCTest

class TestStartOperation(BaseRPCTest):
    """Test start operation"""
    
    def test_start_stopped_instance(self):
        """Test starting a stopped instance"""
        # Add an instance without auto-start
        config_content = """
client
dev tun
proto udp
remote 127.0.0.1 1194
        """.strip()
        
        self.send_rpc_request("add", {
            "instance_name": "start_test",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        # Start the instance
        response = self.send_rpc_request("start", {
            "instance_name": "start_test"
        })
        
        self.assert_success(response)
        self.assert_contains_fields(response, "success", "message")
        
        result = response['result']
        assert result['success'] == True
        assert "started" in result['message'].lower()
        
        print(f"✓ Instance started: {result['message']}")
        
    def test_start_nonexistent_instance(self):
        """Test starting non-existent instance"""
        response = self.send_rpc_request("start", {
            "instance_name": "nonexistent_instance"
        })
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        
        print(f"✓ Non-existent instance start correctly rejected: {result['error']}")
        
    def test_start_missing_instance_name(self):
        """Test starting without instance_name parameter"""
        response = self.send_rpc_request("start", {})
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "Missing 'instance_name'" in result['error']
        
        print(f"✓ Missing instance_name correctly rejected: {result['error']}")
        
    def test_start_already_running_instance(self):
        """Test starting an already running instance"""
        config_content = """
client
dev tun
proto udp
remote 127.0.0.1 1194
        """.strip()
        
        # Add and start instance
        self.send_rpc_request("add", {
            "instance_name": "already_running",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": True
        })
        
        # Try to start again
        response = self.send_rpc_request("start", {
            "instance_name": "already_running"
        })
        
        # Should either succeed or fail gracefully
        result = response['result']
        print(f"Start already running result: {result}")
        
    def test_start_multiple_instances(self):
        """Test starting multiple instances"""
        config_content = """
client
dev tun
proto udp
remote 127.0.0.1 1194
        """.strip()
        
        # Add multiple instances without auto-start
        instances = ["multi_start_1", "multi_start_2", "multi_start_3"]
        
        for instance in instances:
            self.send_rpc_request("add", {
                "instance_name": instance,
                "config_content": config_content,
                "vpn_type": "openvpn",
                "auto_start": False
            })
            
        # Start all instances
        started_count = 0
        for instance in instances:
            response = self.send_rpc_request("start", {
                "instance_name": instance
            })
            
            if response['result']['success']:
                started_count += 1
                
        print(f"✓ Started {started_count}/{len(instances)} instances")

def main():
    """Run start operation tests"""
    test = TestStartOperation()
    
    tests = [
        test.test_start_stopped_instance,
        test.test_start_nonexistent_instance,
        test.test_start_missing_instance_name,
        test.test_start_already_running_instance,
        test.test_start_multiple_instances
    ]
    
    passed = 0
    total = len(tests)
    
    for test_func in tests:
        if test.run_test(test_func):
            passed += 1
            
    print(f"\n📊 Start Operation Tests Results: {passed}/{total} passed")
    return passed == total

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
