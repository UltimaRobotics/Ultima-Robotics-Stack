
#!/usr/bin/env python3
"""
Comprehensive test suite for all RPC operations
"""

import sys
import time
import subprocess
from pathlib import Path


def run_test(test_script):
    """Run a test script and return success status"""
    print(f"\n{'#'*70}")
    print(f"# Running: {test_script}")
    print(f"{'#'*70}\n")
    
    try:
        result = subprocess.run(
            [sys.executable, test_script],
            cwd=Path(__file__).parent,
            timeout=60
        )
        return result.returncode == 0
    except subprocess.TimeoutExpired:
        print(f"✗ Test {test_script} timed out")
        return False
    except Exception as e:
        print(f"✗ Error running {test_script}: {e}")
        return False


def main():
    """Run all RPC tests in sequence"""
    print("\n" + "="*70)
    print("  UR-LICENCE-MANN RPC TEST SUITE")
    print("="*70 + "\n")
    
    # Ensure output directory exists
    output_dir = Path(__file__).parent / "output"
    output_dir.mkdir(exist_ok=True)
    
    tests = [
        "test_rpc_generate.py",
        "test_rpc_verify.py",
        "test_rpc_get_info.py",
        "test_rpc_get_plan.py",
        "test_rpc_update.py",
        "test_rpc_get_definitions.py",
        "test_rpc_update_definitions.py"
    ]
    
    results = {}
    
    for test in tests:
        test_path = Path(__file__).parent / test
        if test_path.exists():
            success = run_test(test)
            results[test] = success
            time.sleep(1)  # Brief pause between tests
        else:
            print(f"⚠ Test file not found: {test}")
            results[test] = False
    
    # Print summary
    print("\n" + "="*70)
    print("  TEST SUMMARY")
    print("="*70 + "\n")
    
    passed = sum(1 for v in results.values() if v)
    total = len(results)
    
    for test, success in results.items():
        status = "✓ PASS" if success else "✗ FAIL"
        print(f"{status}: {test}")
    
    print(f"\n{'='*70}")
    print(f"Results: {passed}/{total} tests passed")
    print(f"{'='*70}\n")
    
    return all(results.values())


if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
