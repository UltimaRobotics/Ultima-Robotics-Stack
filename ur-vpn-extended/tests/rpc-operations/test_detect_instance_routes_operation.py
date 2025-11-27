#!/usr/bin/env python3
"""
Test RPC operation: detect-instance-routes
Detect routes for a specific instance
"""

import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from base_rpc_test import BaseRPCTest

class TestDetectInstanceRoutesOperation(BaseRPCTest):
    """Test detect-instance-routes operation"""
    
    def test_detect_instance_routes(self):
        """Test detecting routes for a specific instance"""
        # Add an instance
        config_content = """
client
dev tun
proto udp
remote 127.0.0.1 1194
route 192.168.1.0 255.255.255.0
route 10.0.0.0 255.0.0.0
        """.strip()
        
        self.send_rpc_request("add", {
            "instance_name": "detect_routes_instance",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        # Detect routes
        response = self.send_rpc_request("detect-instance-routes", {
            "instance_name": "detect_routes_instance"
        })
        
        self.assert_success(response)
        self.assert_contains_fields(response, "success", "detected_routes")
        
        result = response['result']
        assert result['success'] == True
        assert "detected_routes" in result
        
        detected_routes = result['detected_routes']
        assert isinstance(detected_routes, int), "Detected routes should be an integer"
        assert detected_routes >= 0, "Detected routes should be non-negative"
        
        print(f"✓ Detected {detected_routes} routes for instance")
        
    def test_detect_missing_instance_name(self):
        """Test detecting routes without instance_name"""
        response = self.send_rpc_request("detect-instance-routes", {})
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "Missing 'instance_name'" in result['error']
        
        print(f"✓ Missing instance_name correctly rejected: {result['error']}")
        
    def test_detect_empty_instance_name(self):
        """Test detecting routes with empty instance_name"""
        response = self.send_rpc_request("detect-instance-routes", {
            "instance_name": ""
        })
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "Missing 'instance_name'" in result['error']
        
        print(f"✓ Empty instance_name correctly rejected: {result['error']}")
        
    def test_detect_nonexistent_instance_routes(self):
        """Test detecting routes for non-existent instance"""
        response = self.send_rpc_request("detect-instance-routes", {
            "instance_name": "nonexistent_instance"
        })
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        
        print(f"✓ Non-existent instance routes detect correctly rejected: {result['error']}")
        
    def test_detect_routes_simple_config(self):
        """Test detecting routes for instance with simple config"""
        # Add instance with simple config (no routes)
        config_content = """
client
dev tun
proto udp
remote 127.0.0.1 1194
        """.strip()
        
        self.send_rpc_request("add", {
            "instance_name": "detect_simple_instance",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        # Detect routes
        response = self.send_rpc_request("detect-instance-routes", {
            "instance_name": "detect_simple_instance"
        })
        
        self.assert_success(response)
        
        result = response['result']
        detected_routes = result['detected_routes']
        assert isinstance(detected_routes, int), "Detected routes should be an integer"
        
        print(f"✓ Detected {detected_routes} routes for simple config")
        
    def test_detect_routes_complex_config(self):
        """Test detecting routes for instance with complex config"""
        # Add instance with many routes
        config_lines = [
            "client",
            "dev tun",
            "proto udp",
            "remote 127.0.0.1 1194"
        ]
        
        # Add many route lines
        for i in range(10):
            config_lines.append(f"route 192.168.{i}.0 255.255.255.0")
        
        config_content = "\n".join(config_lines)
        
        self.send_rpc_request("add", {
            "instance_name": "detect_complex_instance",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        # Detect routes
        response = self.send_rpc_request("detect-instance-routes", {
            "instance_name": "detect_complex_instance"
        })
        
        self.assert_success(response)
        
        result = response['result']
        detected_routes = result['detected_routes']
        assert isinstance(detected_routes, int), "Detected routes should be an integer"
        assert detected_routes >= 10, "Should detect at least 10 routes"
        
        print(f"✓ Detected {detected_routes} routes for complex config")
        
    def test_detect_routes_wireguard_config(self):
        """Test detecting routes for WireGuard instance"""
        # Add WireGuard instance
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
            "instance_name": "detect_wg_instance",
            "config_content": config_content,
            "vpn_type": "wireguard",
            "auto_start": False
        })
        
        # Detect routes
        response = self.send_rpc_request("detect-instance-routes", {
            "instance_name": "detect_wg_instance"
        })
        
        self.assert_success(response)
        
        result = response['result']
        detected_routes = result['detected_routes']
        assert isinstance(detected_routes, int), "Detected routes should be an integer"
        
        print(f"✓ Detected {detected_routes} routes for WireGuard instance")
        
    def test_detect_routes_consistency(self):
        """Test that detect-instance-routes returns consistent results"""
        # Add instance
        config_content = """
client
dev tun
proto udp
remote 127.0.0.1 1194
route 192.168.1.0 255.255.255.0
        """.strip()
        
        self.send_rpc_request("add", {
            "instance_name": "detect_consistency_instance",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        # Detect routes twice
        response1 = self.send_rpc_request("detect-instance-routes", {
            "instance_name": "detect_consistency_instance"
        })
        
        response2 = self.send_rpc_request("detect-instance-routes", {
            "instance_name": "detect_consistency_instance"
        })
        
        # Both should succeed and return same count
        assert response1['result']['success']
        assert response2['result']['success']
        
        routes1 = response1['result']['detected_routes']
        routes2 = response2['result']['detected_routes']
        
        assert routes1 == routes2, "Route detection should be consistent"
        
        print(f"✓ Route detection consistency verified: {routes1} routes")

def main():
    """Run detect-instance-routes operation tests"""
    test = TestDetectInstanceRoutesOperation()
    
    tests = [
        test.test_detect_instance_routes,
        test.test_detect_missing_instance_name,
        test.test_detect_empty_instance_name,
        test.test_detect_nonexistent_instance_routes,
        test.test_detect_routes_simple_config,
        test.test_detect_routes_complex_config,
        test.test_detect_routes_wireguard_config,
        test.test_detect_routes_consistency
    ]
    
    passed = 0
    total = len(tests)
    
    for test_func in tests:
        if test.run_test(test_func):
            passed += 1
            
    print(f"\n📊 Detect Instance Routes Operation Tests Results: {passed}/{total} passed")
    return passed == total

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
