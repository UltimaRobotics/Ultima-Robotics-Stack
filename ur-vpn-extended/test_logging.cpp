#include <iostream>
#include <thread>
#include <chrono>
extern "C" {
    #include "ur-rpc-template/deps/ur-logger-api/logger.h"
}

int main() {
    std::cout << "Testing logging control functionality..." << std::endl;
    
    // Initialize logger
    logger_init(LOG_INFO, (log_flags_t)(LOG_FLAG_CONSOLE | LOG_FLAG_TIMESTAMP), NULL);
    
    // Test 1: All sources enabled
    std::cout << "\n=== Test 1: All sources enabled ===" << std::endl;
    bool test1_sources[10] = {true, true, true, true, true, true, true, true, true, true};
    logger_configure_sources(true, test1_sources);
    
    logger_log_with_source(LOG_INFO, LOG_SOURCE_VPN_MANAGER, __FILE__, __LINE__, __func__, "VPN Manager test message");
    logger_log_with_source(LOG_INFO, LOG_SOURCE_OPENVPN_LIBRARY, __FILE__, __LINE__, __func__, "OpenVPN Library test message");
    logger_log_with_source(LOG_INFO, LOG_SOURCE_WIREGUARD_LIBRARY, __FILE__, __LINE__, __func__, "WireGuard Library test message");
    
    // Test 2: VPN Manager disabled
    std::cout << "\n=== Test 2: VPN Manager disabled ===" << std::endl;
    bool test2_sources[10] = {true, true, true, false, true, true, true, true, true, true};
    logger_configure_sources(true, test2_sources);
    
    logger_log_with_source(LOG_INFO, LOG_SOURCE_VPN_MANAGER, __FILE__, __LINE__, __func__, "VPN Manager test message (should not appear)");
    logger_log_with_source(LOG_INFO, LOG_SOURCE_OPENVPN_LIBRARY, __FILE__, __LINE__, __func__, "OpenVPN Library test message");
    logger_log_with_source(LOG_INFO, LOG_SOURCE_WIREGUARD_LIBRARY, __FILE__, __LINE__, __func__, "WireGuard Library test message");
    
    // Test 3: OpenVPN Library disabled
    std::cout << "\n=== Test 3: OpenVPN Library disabled ===" << std::endl;
    bool test3_sources[10] = {true, true, true, true, false, true, true, true, true, true};
    logger_configure_sources(true, test3_sources);
    
    logger_log_with_source(LOG_INFO, LOG_SOURCE_VPN_MANAGER, __FILE__, __LINE__, __func__, "VPN Manager test message");
    logger_log_with_source(LOG_INFO, LOG_SOURCE_OPENVPN_LIBRARY, __FILE__, __LINE__, __func__, "OpenVPN Library test message (should not appear)");
    logger_log_with_source(LOG_INFO, LOG_SOURCE_WIREGUARD_LIBRARY, __FILE__, __LINE__, __func__, "WireGuard Library test message");
    
    // Test 4: WireGuard Library disabled
    std::cout << "\n=== Test 4: WireGuard Library disabled ===" << std::endl;
    bool test4_sources[10] = {true, true, true, true, true, false, true, true, true, true};
    logger_configure_sources(true, test4_sources);
    
    logger_log_with_source(LOG_INFO, LOG_SOURCE_VPN_MANAGER, __FILE__, __LINE__, __func__, "VPN Manager test message");
    logger_log_with_source(LOG_INFO, LOG_SOURCE_OPENVPN_LIBRARY, __FILE__, __LINE__, __func__, "OpenVPN Library test message");
    logger_log_with_source(LOG_INFO, LOG_SOURCE_WIREGUARD_LIBRARY, __FILE__, __LINE__, __func__, "WireGuard Library test message (should not appear)");
    
    // Test 5: Global logging disabled
    std::cout << "\n=== Test 5: Global logging disabled ===" << std::endl;
    bool test5_sources[10] = {true, true, true, true, true, true, true, true, true, true};
    logger_configure_sources(false, test5_sources);
    
    logger_log_with_source(LOG_INFO, LOG_SOURCE_VPN_MANAGER, __FILE__, __LINE__, __func__, "VPN Manager test message (should not appear)");
    logger_log_with_source(LOG_INFO, LOG_SOURCE_OPENVPN_LIBRARY, __FILE__, __LINE__, __func__, "OpenVPN Library test message (should not appear)");
    logger_log_with_source(LOG_INFO, LOG_SOURCE_WIREGUARD_LIBRARY, __FILE__, __LINE__, __func__, "WireGuard Library test message (should not appear)");
    
    std::cout << "\n=== Logging control test completed ===" << std::endl;
    
    // Cleanup
    logger_destroy();
    
    return 0;
}
