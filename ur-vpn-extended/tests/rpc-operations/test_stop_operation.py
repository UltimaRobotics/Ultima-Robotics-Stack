#!/usr/bin/env python3
"""
Test RPC operation: stop
Stop a VPN instance
"""

import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from base_rpc_test import BaseRPCTest

class TestStopOperation(BaseRPCTest):
    """Test stop operation"""
    
    def test_stop_running_instance(self):
        """Test stopping a running instance"""
        # Add and start an instance
        config_content = """
client
dev tun
proto udp
remote 127.0.0.1 1194
        """.strip()
        
        self.send_rpc_request("add", {
            "instance_name": "stop_test",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": True
        })
        
        # Stop the instance
        response = self.send_rpc_request("stop", {
            "instance_name": "stop_test"
        })
        
        self.assert_success(response)
        self.assert_contains_fields(response, "success", "message")
        
        result = response['result']
        assert result['success'] == True
        assert "stopped" in result['message'].lower()
        
        print(f"✓ Instance stopped: {result['message']}")
        
    def test_stop_nonexistent_instance(self):
        """Test stopping non-existent instance"""
        response = self.send_rpc_request("stop", {
            "instance_name": "nonexistent_instance"
        })
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        
        print(f"✓ Non-existent instance stop correctly rejected: {result['error']}")
        
    def test_stop_missing_instance_name(self):
        """Test stopping without instance_name parameter"""
        response = self.send_rpc_request("stop", {})
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "Missing 'instance_name'" in result['error']
        
        print(f"✓ Missing instance_name correctly rejected: {result['error']}")
        
    def test_stop_already_stopped_instance(self):
        """Test stopping an already stopped instance"""
        config_content = """
client
dev tun
proto udp
remote 127.0.0.1 1194
        """.strip()
        
        # Add instance without auto-start
        self.send_rpc_request("add", {
            "instance_name": "already_stopped",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        # Try to stop
        response = self.send_rpc_request("stop", {
            "instance_name": "already_stopped"
        })
        
        # Should either succeed or fail gracefully
        result = response['result']
        print(f"Stop already stopped result: {result}")
        
    def test_stop_multiple_instances(self):
        """Test stopping multiple instances"""
        config_content = """
client
dev tun
proto udp
remote 127.0.0.1 1194
        """.strip()
        
        # Add and start multiple instances
        instances = ["multi_stop_1", "multi_stop_2", "multi_stop_3"]
        
        for instance in instances:
            self.send_rpc_request("add", {
                "instance_name": instance,
                "config_content": config_content,
                "vpn_type": "openvpn",
                "auto_start": True
            })
            
        # Stop all instances
        stopped_count = 0
        for instance in instances:
            response = self.send_rpc_request("stop", {
                "instance_name": instance
            })
            
            if response['result']['success']:
                stopped_count += 1
                
        print(f"✓ Stopped {stopped_count}/{len(instances)} instances")

def main():
    """Run stop operation tests"""
    test = TestStopOperation()
    
    tests = [
        test.test_stop_running_instance,
        test.test_stop_nonexistent_instance,
        test.test_stop_missing_instance_name,
        test.test_stop_already_stopped_instance,
        test.test_stop_multiple_instances
    ]
    
    passed = 0
    total = len(tests)
    
    for test_func in tests:
        if test.run_test(test_func):
            passed += 1
            
    print(f"\n📊 Stop Operation Tests Results: {passed}/{total} passed")
    return passed == total

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
