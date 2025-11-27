#!/usr/bin/env python3
"""
Test RPC operation: delete-custom-route
Delete a custom routing rule
"""

import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from base_rpc_test import BaseRPCTest

class TestDeleteCustomRouteOperation(BaseRPCTest):
    """Test delete-custom-route operation"""
    
    def test_delete_custom_route(self):
        """Test deleting a custom routing rule"""
        # Add a route first
        route_data = {
            "id": "delete_test_route",
            "vpn_instance": "test_instance",
            "destination": "192.168.100.0/24"
        }
        
        self.send_rpc_request("add-custom-route", route_data)
        
        # Delete the route
        response = self.send_rpc_request("delete-custom-route", {
            "id": "delete_test_route"
        })
        
        self.assert_success(response)
        
        result = response['result']
        assert result['success'] == True
        assert "deleted successfully" in result['message']
        
        print(f"✓ Custom route deleted: {result['message']}")
        
    def test_delete_nonexistent_custom_route(self):
        """Test deleting a non-existent custom route"""
        response = self.send_rpc_request("delete-custom-route", {
            "id": "nonexistent_route"
        })
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        
        print(f"✓ Non-existent route deletion correctly rejected: {result['error']}")
        
    def test_delete_missing_id(self):
        """Test deleting custom route without id"""
        response = self.send_rpc_request("delete-custom-route", {})
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "Missing 'id' field" in result['error']
        
        print(f"✓ Missing id correctly rejected: {result['error']}")
        
    def test_delete_empty_id(self):
        """Test deleting custom route with empty id"""
        response = self.send_rpc_request("delete-custom-route", {
            "id": ""
        })
        
        self.assert_success(response, expected_success=False)
        
        result = response['result']
        assert result['success'] == False
        assert "Missing 'id' field" in result['error']
        
        print(f"✓ Empty id correctly rejected: {result['error']}")
        
    def test_delete_multiple_routes(self):
        """Test deleting multiple custom routes"""
        # Add multiple routes
        routes = [
            {"id": "multi_delete_1", "vpn_instance": "test_instance", "destination": "192.168.1.0/24"},
            {"id": "multi_delete_2", "vpn_instance": "test_instance", "destination": "192.168.2.0/24"},
            {"id": "multi_delete_3", "vpn_instance": "test_instance", "destination": "192.168.3.0/24"}
        ]
        
        for route in routes:
            self.send_rpc_request("add-custom-route", route)
            
        # Delete all routes
        deleted_count = 0
        for route in routes:
            response = self.send_rpc_request("delete-custom-route", {
                "id": route["id"]
            })
            
            if response['result']['success']:
                deleted_count += 1
                
        assert deleted_count == len(routes), f"Expected {len(routes)} deletions, got {deleted_count}"
        
        print(f"✓ Successfully deleted {deleted_count} custom routes")
        
    def test_delete_already_deleted_route(self):
        """Test deleting a route that was already deleted"""
        route_data = {
            "id": "already_deleted_route",
            "vpn_instance": "test_instance",
            "destination": "192.168.200.0/24"
        }
        
        # Add and delete route
        self.send_rpc_request("add-custom-route", route_data)
        
        delete_response1 = self.send_rpc_request("delete-custom-route", {
            "id": "already_deleted_route"
        })
        assert delete_response1['result']['success']
        
        # Try to delete again
        response = self.send_rpc_request("delete-custom-route", {
            "id": "already_deleted_route"
        })
        
        # Should fail
        self.assert_success(response, expected_success=False)
        
        print(f"✓ Already deleted route correctly rejected: {response['result']['error']}")

def main():
    """Run delete-custom-route operation tests"""
    test = TestDeleteCustomRouteOperation()
    
    tests = [
        test.test_delete_custom_route,
        test.test_delete_nonexistent_custom_route,
        test.test_delete_missing_id,
        test.test_delete_empty_id,
        test.test_delete_multiple_routes,
        test.test_delete_already_deleted_route
    ]
    
    passed = 0
    total = len(tests)
    
    for test_func in tests:
        if test.run_test(test_func):
            passed += 1
            
    print(f"\n📊 Delete Custom Route Operation Tests Results: {passed}/{total} passed")
    return passed == total

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
