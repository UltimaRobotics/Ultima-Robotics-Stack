#!/usr/bin/env python3
"""
Run all individual RPC operation tests for ur-vpn-manager
Each operation now has its own dedicated test file
"""

import sys
import os
import time
import importlib.util
from pathlib import Path

def load_test_module(test_file):
    """Load a test module from file path"""
    spec = importlib.util.spec_from_file_location(test_file.stem, test_file)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

def run_test_suite():
    """Run all individual RPC operation tests"""
    print("🚀 Starting Individual RPC Operations Test Suite")
    print("=" * 60)
    
    # Get all individual test files ( *_operation.py )
    test_dir = Path(__file__).parent
    test_files = list(test_dir.glob("test_*_operation.py"))
    
    # Sort tests by operation name for consistent ordering
    test_files.sort(key=lambda x: x.name)
    
    if not test_files:
        print("❌ No individual test files found!")
        return False
    
    print(f"📋 Found {len(test_files)} individual test files:")
    for test_file in test_files:
        # Extract operation name from filename
        operation_name = test_file.stem.replace("test_", "").replace("_operation", "")
        print(f"   - {test_file.name} ({operation_name})")
    print()
    
    # Run each test
    total_tests = len(test_files)
    passed_tests = 0
    failed_tests = []
    
    start_time = time.time()
    
    for test_file in test_files:
        # Extract operation name for display
        operation_name = test_file.stem.replace("test_", "").replace("_operation", "")
        
        print(f"\n{'='*25} {operation_name.upper()} {'='*25}")
        
        try:
            # Load and run test module
            test_module = load_test_module(test_file)
            
            if hasattr(test_module, 'main'):
                success = test_module.main()
                if success:
                    passed_tests += 1
                    print(f"✅ {operation_name} PASSED")
                else:
                    failed_tests.append(operation_name)
                    print(f"❌ {operation_name} FAILED")
            else:
                print(f"❌ {test_file.name} has no main() function")
                failed_tests.append(operation_name)
                
        except Exception as e:
            print(f"❌ {operation_name} ERROR: {e}")
            failed_tests.append(operation_name)
            
    # Summary
    end_time = time.time()
    duration = end_time - start_time
    
    print("\n" + "=" * 60)
    print("📊 INDIVIDUAL TEST SUITE SUMMARY")
    print("=" * 60)
    print(f"Total Operations: {total_tests}")
    print(f"Passed: {passed_tests}")
    print(f"Failed: {len(failed_tests)}")
    print(f"Duration: {duration:.2f} seconds")
    print(f"Success Rate: {(passed_tests/total_tests)*100:.1f}%")
    
    # Operation breakdown
    print(f"\n🔧 Operations Tested:")
    for test_file in sorted(test_files):
        operation_name = test_file.stem.replace("test_", "").replace("_operation", "")
        status = "✅" if operation_name not in failed_tests else "❌"
        print(f"   {status} {operation_name}")
    
    if failed_tests:
        print(f"\n❌ Failed Operations:")
        for operation_name in failed_tests:
            print(f"   - {operation_name}")
        print(f"\n💡 To run individual operation test: python3 test_{operation_name}_operation.py")
        return False
    else:
        print(f"\n🎉 ALL OPERATIONS PASSED!")
        return True

def check_dependencies():
    """Check if required dependencies are available"""
    try:
        import paho.mqtt.client as mqtt
        print("✅ paho-mqtt client available")
    except ImportError:
        print("❌ paho-mqtt not installed. Install with: pip install paho-mqtt")
        return False
        
    try:
        import subprocess
        print("✅ subprocess module available")
    except ImportError:
        print("❌ subprocess module not available")
        return False
        
    return True

def check_prerequisites():
    """Check if test prerequisites are met"""
    print("🔍 Checking prerequisites...")
    
    # Check dependencies
    if not check_dependencies():
        return False
    
    # Check if ur-vpn-manager binary exists
    manager_path = Path(__file__).parent.parent.parent / "build" / "ur-vpn-manager"
    if not manager_path.exists():
        print(f"❌ ur-vpn-manager binary not found at {manager_path}")
        print("💡 Build the project first: cd build && make -j4")
        return False
    
    print(f"✅ ur-vpn-manager binary found at {manager_path}")
    
    # Check if config files exist
    config_dir = Path(__file__).parent.parent.parent / "config"
    master_config = config_dir / "master-config.json"
    rpc_config = config_dir / "ur-rpc-config.json"
    
    if not master_config.exists():
        print(f"❌ Master config not found at {master_config}")
        return False
        
    if not rpc_config.exists():
        print(f"❌ RPC config not found at {rpc_config}")
        return False
    
    print(f"✅ Configuration files found")
    
    # Check if MQTT broker is accessible
    try:
        import socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(2)
        result = sock.connect_ex(('127.0.0.1', 1899))
        sock.close()
        
        if result != 0:
            print("⚠️  MQTT broker not accessible at 127.0.0.1:1899")
            print("💡 Tests will start the broker automatically if needed")
        else:
            print("✅ MQTT broker accessible at 127.0.0.1:1899")
    except Exception as e:
        print(f"⚠️  Could not check MQTT broker: {e}")
    
    return True

def main():
    """Main test runner"""
    print("🧪 ur-vpn-manager Individual RPC Operations Test Suite")
    print("======================================================")
    
    # Check prerequisites
    if not check_prerequisites():
        print("\n❌ Prerequisites not met. Please fix the issues above.")
        sys.exit(1)
    
    # Run tests
    success = run_test_suite()
    
    if success:
        print("\n🎉 All individual RPC operation tests completed successfully!")
        print("📊 Each operation was tested in isolation for better coverage.")
        sys.exit(0)
    else:
        print("\n💥 Some operation tests failed. Check the output above for details.")
        sys.exit(1)

if __name__ == "__main__":
    main()
