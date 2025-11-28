#!/usr/bin/env python3
"""
Test RPC operation: purge-cleanup
Comprehensive cleanup of all VPN instances, data, routes, and configurations
Usage: python test_purge_cleanup_operation.py [--confirm]
"""

import sys
import os
import json
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from base_rpc_test import BaseRPCTest

def print_response(response):
    """Pretty print the RPC response"""
    print(f"Response:\n{json.dumps(response, indent=2)}")

def test_purge_cleanup(confirm=False):
    """Test comprehensive purge cleanup via RPC"""
    test = BaseRPCTest()
    
    try:
        if not test.setup():
            print("✗ Test setup failed")
            return False
            
        if confirm:
            print("Testing: Purge Cleanup (WITH CONFIRMATION - DESTRUCTIVE)")
            print("⚠️  WARNING: This will remove ALL VPN instances, configurations, and data!")
        else:
            print("Testing: Purge Cleanup (WITHOUT CONFIRMATION - SAFETY CHECK)")
        
        params = {
            "confirm": confirm
        }
        
        response = test.send_rpc_request("purge-cleanup", params)
        print_response(response)
        
        # Basic validation
        if 'result' in response:
            result = response['result']
            
            if not confirm:
                # Should fail without confirmation
                if not result.get('success') and 'confirmation required' in result.get('error', '').lower():
                    print("✓ Safety check passed - confirmation required as expected")
                    return True
                else:
                    print("✗ Safety check failed - should require confirmation")
                    return False
            else:
                # Should succeed with confirmation
                if result.get('success'):
                    print("✓ Purge cleanup operation completed successfully")
                    
                    # Validate expected response fields
                    expected_fields = [
                        'type', 'instances_stopped', 'instances_cleared',
                        'routing_rules_cleared', 'config_saved', 'cache_saved',
                        'routing_rules_file_cleared', 'interface_cleanup'
                    ]
                    
                    missing_fields = []
                    for field in expected_fields:
                        if field not in result:
                            missing_fields.append(field)
                    
                    if missing_fields:
                        print(f"⚠️  Missing expected response fields: {missing_fields}")
                    else:
                        print("✓ All expected response fields present")
                    
                    # Validate cleanup counts
                    if result.get('instances_cleared', 0) >= 0:
                        print(f"✓ Instances cleared: {result.get('instances_cleared')}")
                    
                    if result.get('routing_rules_cleared', 0) >= 0:
                        print(f"✓ Routing rules cleared: {result.get('routing_rules_cleared')}")
                    
                    return True
                else:
                    print("✗ Purge cleanup operation failed")
                    if 'error' in result:
                        print(f"Error: {result['error']}")
                    return False
        else:
            print("✗ Invalid response format")
            return False
            
    except Exception as e:
        print(f"✗ Test failed: {e}")
        return False
    finally:
        test.teardown()

def print_usage():
    """Print usage information"""
    print("Usage: python test_purge_cleanup_operation.py [--confirm]")
    print("")
    print("Options:")
    print("  --confirm    Execute destructive purge cleanup (removes all VPN data)")
    print("")
    print("Examples:")
    print("  # Test safety check (without cleanup)")
    print("  python test_purge_cleanup_operation.py")
    print("")
    print("  # Execute actual purge cleanup (DESTRUCTIVE)")
    print("  python test_purge_cleanup_operation.py --confirm")
    print("")
    print("⚠️  WARNING: Using --confirm will permanently delete:")
    print("   - All VPN instances")
    print("   - All configuration data")
    print("   - All routing rules")
    print("   - All cached data")
    print("   - All VPN interfaces")

if __name__ == "__main__":
    confirm = False
    
    if len(sys.argv) > 1:
        if sys.argv[1] == "--confirm":
            confirm = True
        elif sys.argv[1] in ["-h", "--help", "help"]:
            print_usage()
            sys.exit(0)
        else:
            print(f"Unknown option: {sys.argv[1]}")
            print_usage()
            sys.exit(1)
    
    # Print warning for destructive operation
    if confirm:
        print("=" * 60)
        print("⚠️  DESTRUCTIVE OPERATION CONFIRMED ⚠️")
        print("This will remove ALL VPN data from the system!")
        print("=" * 60)
        
        # Ask for additional confirmation
        try:
            response = input("Type 'PURGE' to confirm: ").strip()
            if response != "PURGE":
                print("Operation cancelled")
                sys.exit(0)
        except KeyboardInterrupt:
            print("\nOperation cancelled")
            sys.exit(0)
    
    success = test_purge_cleanup(confirm)
    
    if success and confirm:
        print("\n" + "=" * 60)
        print("✅ PURGE CLEANUP COMPLETED")
        print("All VPN data has been removed from the system")
        print("=" * 60)
    
    sys.exit(0 if success else 1)
