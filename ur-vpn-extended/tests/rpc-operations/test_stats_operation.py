#!/usr/bin/env python3
"""
Test RPC operation: stats
Get aggregated statistics for all VPN instances
"""

import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from base_rpc_test import BaseRPCTest

class TestStatsOperation(BaseRPCTest):
    """Test stats operation"""
    
    def test_stats_operation(self):
        """Test getting aggregated statistics"""
        response = self.send_rpc_request("stats", {})
        
        self.assert_success(response)
        self.assert_contains_fields(response, "success", "stats")
        
        result = response['result']
        assert result['success'] == True
        assert "stats" in result
        
        stats = result['stats']
        assert isinstance(stats, dict), "Stats should be a dictionary"
        
        print(f"✓ Retrieved aggregated statistics: {stats}")
        
    def test_stats_with_instances(self):
        """Test getting statistics when instances exist"""
        # Add some instances first
        config_content = """
client
dev tun
proto udp
remote 127.0.0.1 1194
        """.strip()
        
        self.send_rpc_request("add", {
            "instance_name": "stats_test_1",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        self.send_rpc_request("add", {
            "instance_name": "stats_test_2",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": True
        })
        
        # Get stats
        response = self.send_rpc_request("stats", {})
        
        self.assert_success(response)
        
        result = response['result']
        stats = result['stats']
        assert isinstance(stats, dict), "Stats should be a dictionary"
        
        print(f"✓ Retrieved statistics with multiple instances: {stats}")
        
    def test_stats_with_parameters(self):
        """Test stats operation with additional parameters"""
        # Stats should ignore extra parameters
        response = self.send_rpc_request("stats", {
            "detailed": True,
            "format": "json"
        })
        
        self.assert_success(response)
        
        result = response['result']
        assert result['success'] == True
        assert "stats" in result
        
        stats = result['stats']
        assert isinstance(stats, dict), "Stats should be a dictionary"
        
        print(f"✓ Stats operation with parameters: {stats}")
        
    def test_stats_empty_system(self):
        """Test stats on system with no instances"""
        # Stats should work even with no instances
        response = self.send_rpc_request("stats", {})
        
        self.assert_success(response)
        
        result = response['result']
        assert result['success'] == True
        assert "stats" in result
        
        stats = result['stats']
        assert isinstance(stats, dict), "Stats should be a dictionary"
        
        print(f"✓ Stats operation on empty system: {stats}")
        
    def test_stats_consistency(self):
        """Test that stats returns consistent data"""
        # Get stats twice
        response1 = self.send_rpc_request("stats", {})
        response2 = self.send_rpc_request("stats", {})
        
        # Both should succeed
        assert response1['result']['success']
        assert response2['result']['success']
        
        stats1 = response1['result']['stats']
        stats2 = response2['result']['stats']
        
        # Stats should be consistent (same structure)
        assert isinstance(stats1, dict), "First stats should be dictionary"
        assert isinstance(stats2, dict), "Second stats should be dictionary"
        
        print(f"✓ Stats consistency check passed")

def main():
    """Run stats operation tests"""
    test = TestStatsOperation()
    
    tests = [
        test.test_stats_operation,
        test.test_stats_with_instances,
        test.test_stats_with_parameters,
        test.test_stats_empty_system,
        test.test_stats_consistency
    ]
    
    passed = 0
    total = len(tests)
    
    for test_func in tests:
        if test.run_test(test_func):
            passed += 1
            
    print(f"\n📊 Stats Operation Tests Results: {passed}/{total} passed")
    return passed == total

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
