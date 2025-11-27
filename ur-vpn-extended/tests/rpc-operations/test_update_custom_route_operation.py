#!/usr/bin/env python3
"""
Test RPC operation: update-custom-route
Update an existing custom routing rule
"""

import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from base_rpc_test import BaseRPCTest

class TestUpdateCustomRouteOperation(BaseRPCTest):
    """Test update-custom-route operation"""
    
    def test_update_custom_route(self):
        """Test updating a custom routing rule"""
        # Add a route first
        route_data = {
            "id": "update_test_route",
            "name": "Original Route",
            "vpn_instance": "test_instance",
            "destination": "192.168.1.0/24",
            "priority": 100,
            "enabled": True
        }
        
        self.send_rpc_request("add-custom-route", route_data)
        
        # Update the route
        update_data = {
            "id": "update_test_route",
            "name": "Updated Route",
            "priority": 300,
            "enabled": False,
            "description": "Updated description"
        }
        
        response = self.send_rpc_request("update-custom-route", update_data)
        
        self.assert_success(response)
        
        result = response['result']
        assert result['success'] == True
        assert "updated successfully" in result['message']
        
        print(f"✓ Custom route updated: {result['message']}")
        
    def test_update_nonexistent_custom_route(self):
        """Test updating a non-existent custom route"""
        response = self.send_rpc_request("update-custom-route", {
            "id": "nonexistent_route",
            "name": "Non-existent Route"
        })
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        
        print(f"✓ Non-existent route update correctly rejected: {result['error']}")
        
    def test_update_missing_id(self):
        """Test updating custom route without id"""
        response = self.send_rpc_request("update-custom-route", {
            "name": "Route without ID",
            "destination": "192.168.1.0/24"
        })
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "Missing 'id' field" in result['error']
        
        print(f"✓ Missing id correctly rejected: {result['error']}")
        
    def test_update_partial_fields(self):
        """Test updating only some fields of a custom route"""
        # Add a route first
        original_route = {
            "id": "partial_update_route",
            "name": "Original Name",
            "vpn_instance": "test_instance",
            "destination": "192.168.1.0/24",
            "priority": 100,
            "enabled": True,
            "description": "Original description"
        }
        
        self.send_rpc_request("add-custom-route", original_route)
        
        # Update only priority and enabled status
        update_data = {
            "id": "partial_update_route",
            "priority": 500,
            "enabled": False
        }
        
        response = self.send_rpc_request("update-custom-route", update_data)
        
        self.assert_success(response)
        
        result = response['result']
        assert result['success'] == True
        
        print(f"✓ Partial route update successful: {result['message']}")
        
    def test_update_all_fields(self):
        """Test updating all fields of a custom route"""
        # Add a route first
        self.send_rpc_request("add-custom-route", {
            "id": "full_update_route",
            "name": "Original",
            "vpn_instance": "test_instance",
            "destination": "192.168.1.0/24"
        })
        
        # Update with all new values
        update_data = {
            "id": "full_update_route",
            "name": "Completely Updated Route",
            "vpn_instance": "updated_instance",
            "vpn_profile": "new_profile",
            "source_type": "Network",
            "source_value": "10.0.0.0/8",
            "destination": "172.16.0.0/12",
            "gateway": "New Gateway",
            "protocol": "udp",
            "type": "full_tunnel",
            "priority": 999,
            "enabled": True,
            "log_traffic": False,
            "apply_to_existing": False,
            "description": "Completely updated",
            "last_modified": "2023-12-01",
            "user_modified": True
        }
        
        response = self.send_rpc_request("update-custom-route", update_data)
        
        self.assert_success(response)
        
        result = response['result']
        assert result['success'] == True
        
        print(f"✓ Full route update successful: {result['message']}")

def main():
    """Run update-custom-route operation tests"""
    test = TestUpdateCustomRouteOperation()
    
    tests = [
        test.test_update_custom_route,
        test.test_update_nonexistent_custom_route,
        test.test_update_missing_id,
        test.test_update_partial_fields,
        test.test_update_all_fields
    ]
    
    passed = 0
    total = len(tests)
    
    for test_func in tests:
        if test.run_test(test_func):
            passed += 1
            
    print(f"\n📊 Update Custom Route Operation Tests Results: {passed}/{total} passed")
    return passed == total

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
