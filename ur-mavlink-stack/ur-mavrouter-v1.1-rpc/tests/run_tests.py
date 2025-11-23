#!/usr/bin/env python3
"""
Simple test runner script for RPC thread state tests
"""

import subprocess
import sys
import os

def check_dependencies():
    """Check if required dependencies are installed"""
    try:
        import paho.mqtt.client as mqtt
        print("✓ paho-mqtt is installed")
        return True
    except ImportError:
        print("✗ paho-mqtt is not installed")
        print("Installing dependencies...")
        try:
            subprocess.check_call([sys.executable, "-m", "pip", "install", "-r", "requirements.txt"])
            print("✓ Dependencies installed successfully")
            return True
        except subprocess.CalledProcessError:
            print("✗ Failed to install dependencies")
            return False

def run_test(test_name, args=None):
    """Run a specific test"""
    cmd = [sys.executable, "test_rpc_thread_states.py"]
    if args:
        cmd.extend(args)
    
    print(f"\n🧪 Running: {' '.join(cmd)}")
    print("-" * 50)
    
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, cwd=os.path.dirname(__file__))
        print(result.stdout)
        if result.stderr:
            print("STDERR:", result.stderr)
        return result.returncode == 0
    except Exception as e:
        print(f"✗ Failed to run test: {e}")
        return False

def main():
    """Main test runner"""
    print("🚀 RPC Thread State Test Runner")
    print("=" * 50)
    
    # Check dependencies
    if not check_dependencies():
        print("❌ Cannot proceed without dependencies")
        return 1
    
    # Change to test directory
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    
    # Run tests
    tests = [
        ("Basic Operations", []),
        ("Extension Monitoring", ["extensions"]),
    ]
    
    results = []
    
    for test_name, args in tests:
        print(f"\n📋 Running {test_name} Test...")
        success = run_test(test_name, args)
        results.append((test_name, success))
    
    # Summary
    print("\n" + "=" * 50)
    print("📊 Test Results Summary")
    print("=" * 50)
    
    passed = 0
    for test_name, success in results:
        status = "✅ PASS" if success else "❌ FAIL"
        print(f"{test_name:<25} {status}")
        if success:
            passed += 1
    
    print(f"\nTotal: {passed}/{len(results)} tests passed")
    
    if passed == len(results):
        print("\n🎉 All tests passed!")
        return 0
    else:
        print("\n💥 Some tests failed!")
        return 1

if __name__ == "__main__":
    sys.exit(main())
