#!/usr/bin/env python3
"""
Test RPC operation: parse
Parse VPN configuration without making system changes
"""

import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from base_rpc_test import BaseRPCTest

class TestParseOperation(BaseRPCTest):
    """Test parse operation"""
    
    def test_parse_valid_config(self):
        """Test parsing a valid OpenVPN configuration"""
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
        
        response = self.send_rpc_request("parse", {
            "config_content": config_content
        })
        
        self.assert_success(response)
        self.assert_contains_fields(response, "success", "message", "parsed_config")
        
        result = response['result']
        assert result['success'] == True
        assert "parsed successfully" in result['message']
        assert result['parsed_config']['config_provided'] == True
        assert result['parsed_config']['config_length'] > 0
        
        print(f"✓ Parse successful: {result['message']}")
        print(f"✓ Config length: {result['parsed_config']['config_length']}")
        
    def test_parse_empty_config(self):
        """Test parsing empty configuration"""
        response = self.send_rpc_request("parse", {
            "config_content": ""
        })
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "Missing 'config_content'" in result['error']
        
        print(f"✓ Empty config correctly rejected: {result['error']}")
        
    def test_parse_missing_config_field(self):
        """Test parsing without config_content field"""
        response = self.send_rpc_request("parse", {})
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "Missing 'config_content'" in result['error']
        
        print(f"✓ Missing config field correctly rejected: {result['error']}")
        
    def test_parse_large_config(self):
        """Test parsing a large configuration"""
        # Generate a large config
        config_lines = ["client", "dev tun", "proto udp"]
        for i in range(1000):
            config_lines.append(f"# Route {i}")
            config_lines.append(f"route 192.168.{i//256}.{i%256} 255.255.255.0")
        config_content = "\n".join(config_lines)
        
        response = self.send_rpc_request("parse", {
            "config_content": config_content
        })
        
        self.assert_success(response)
        
        result = response['result']
        assert result['success'] == True
        assert result['parsed_config']['config_length'] == len(config_content)
        
        print(f"✓ Large config parsed successfully: {result['parsed_config']['config_length']} characters")

def main():
    """Run parse operation tests"""
    test = TestParseOperation()
    
    tests = [
        test.test_parse_valid_config,
        test.test_parse_empty_config,
        test.test_parse_missing_config_field,
        test.test_parse_large_config
    ]
    
    passed = 0
    total = len(tests)
    
    for test_func in tests:
        if test.run_test(test_func):
            passed += 1
            
    print(f"\n📊 Parse Operation Tests Results: {passed}/{total} passed")
    return passed == total

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
