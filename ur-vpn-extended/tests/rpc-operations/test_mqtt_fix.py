#!/usr/bin/env python3
"""
Quick test to verify MQTT client fix works
"""

import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from base_rpc_test import BaseRPCTest

def test_mqtt_client():
    """Test MQTT client initialization"""
    print("🧪 Testing MQTT client initialization...")
    
    try:
        test = BaseRPCTest()
        print("✅ BaseRPCTest initialized successfully")
        print(f"✅ MQTT client created: {type(test.client)}")
        return True
    except Exception as e:
        print(f"❌ MQTT client initialization failed: {e}")
        return False

if __name__ == "__main__":
    success = test_mqtt_client()
    sys.exit(0 if success else 1)
