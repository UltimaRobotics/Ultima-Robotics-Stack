#!/usr/bin/env python3
"""
Cleanup script to remove old combined test files and keep only individual operation tests
"""

import os
import sys
from pathlib import Path

def cleanup_old_tests():
    """Remove old combined test files"""
    print("🧹 Cleaning up old combined test files...")
    
    # Old combined test files to remove
    old_files = [
        "test_parse.py",
        "test_add.py", 
        "test_delete.py",
        "test_update.py",
        "test_start_stop_restart.py",
        "test_enable_disable.py",
        "test_status_list_stats.py",
        "test_custom_routes.py",
        "test_instance_routes.py"
    ]
    
    # Keep these files
    keep_files = [
        "base_rpc_test.py",
        "run_all_tests.py",
        "requirements.txt",
        "README.md",
        "TEST_SUMMARY.md",
        "INDIVIDUAL_TESTS_SUMMARY.md",
        "cleanup_old_tests.py"
    ]
    
    test_dir = Path(__file__).parent
    removed_count = 0
    kept_count = 0
    
    # Remove old combined test files
    for old_file in old_files:
        file_path = test_dir / old_file
        if file_path.exists():
            try:
                file_path.unlink()
                print(f"   🗑️  Removed: {old_file}")
                removed_count += 1
            except Exception as e:
                print(f"   ❌ Failed to remove {old_file}: {e}")
        else:
            print(f"   ⚠️  Not found: {old_file}")
    
    # List remaining files
    print(f"\n📋 Remaining files in test directory:")
    all_files = list(test_dir.glob("*"))
    
    for file_path in sorted(all_files):
        if file_path.is_file():
            file_name = file_path.name
            if file_name.endswith("_operation.py"):
                print(f"   ✅ Individual test: {file_name}")
                kept_count += 1
            elif file_name in keep_files:
                print(f"   ✅ Essential file: {file_name}")
                kept_count += 1
            else:
                print(f"   ❓ Other file: {file_name}")
    
    print(f"\n📊 Cleanup Summary:")
    print(f"   Removed: {removed_count} old combined test files")
    print(f"   Kept: {kept_count} individual test files + essentials")
    print(f"   Total individual operations: {kept_count - len(keep_files)}")
    
    return removed_count > 0

def main():
    """Main cleanup function"""
    print("🧪 ur-vpn-manager Test Suite Cleanup")
    print("====================================")
    print("This script removes old combined test files and keeps only")
    print("individual operation test files for better isolation.")
    print()
    
    # Ask for confirmation
    response = input("Do you want to proceed with cleanup? (y/N): ").strip().lower()
    if response not in ['y', 'yes']:
        print("❌ Cleanup cancelled.")
        return
    
    # Perform cleanup
    success = cleanup_old_tests()
    
    if success:
        print(f"\n✅ Cleanup completed successfully!")
        print(f"🎯 Test suite now uses individual operation files only.")
        print(f"🚀 Run tests with: python3 run_all_tests.py")
    else:
        print(f"\n⚠️  Cleanup completed with some issues.")

if __name__ == "__main__":
    main()
