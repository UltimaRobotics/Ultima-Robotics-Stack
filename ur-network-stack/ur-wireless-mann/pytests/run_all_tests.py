
#!/usr/bin/env python3
"""
Master test runner for all ur-wireless-tools RPC functionality tests.
Runs all tests sequentially and reports overall results.
"""

import subprocess
import sys
from datetime import datetime

def run_test(test_script, description):
    """Run a single test script and return success status"""
    print(f"\n{'='*70}")
    print(f"Running: {description}")
    print(f"{'='*70}")
    
    try:
        result = subprocess.run(
            [sys.executable, test_script],
            capture_output=False,
            text=True
        )
        return result.returncode == 0
    except Exception as e:
        print(f"✗ Error running {test_script}: {e}")
        return False

def main():
    print("="*70)
    print("UR-WIRELESS-TOOLS RPC TEST SUITE")
    print("="*70)
    print(f"Start time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print()
    
    # Define all tests to run (excluding destructive tests)
    tests = [
        ("test_list_interfaces.py", "List Interfaces Test"),
        ("test_get_interface_info.py", "Get Interface Info Test"),
        ("test_scan_networks.py", "Scan Networks Test"),
        ("test_test_connection.py", "Test Connection Test"),
        ("test_get_status.py", "Get Status Test"),
        ("test_save_network.py", "Save Network Test"),
        ("test_list_saved_networks.py", "List Saved Networks Test"),
        ("test_remove_network.py", "Remove Network Test"),
        ("test_get_wireless_config.py", "Get Wireless Config Test"),
        ("test_get_system_state.py", "Get System State Test"),
        ("test_enable_wifi.py", "Enable WiFi Test"),
        ("test_set_mode.py", "Set Mode Test (STA)"),
    ]
    
    results = {}
    
    for test_file, description in tests:
        success = run_test(test_file, description)
        results[description] = success
        
    # Print summary
    print("\n" + "="*70)
    print("TEST SUMMARY")
    print("="*70)
    
    passed = sum(1 for v in results.values() if v)
    total = len(results)
    
    for test_name, success in results.items():
        status = "✓ PASSED" if success else "✗ FAILED"
        print(f"{status}: {test_name}")
        
    print("="*70)
    print(f"Results: {passed}/{total} tests passed")
    print(f"End time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print("="*70)
    
    # Note about excluded tests
    print("\nNote: The following tests must be run manually:")
    print("  - test_shutdown.py (requires confirmation and shuts down service)")
    print("  - test_disable_wifi.py (requires confirmation and disables WiFi)")
    
    return 0 if passed == total else 1

if __name__ == "__main__":
    sys.exit(main())
