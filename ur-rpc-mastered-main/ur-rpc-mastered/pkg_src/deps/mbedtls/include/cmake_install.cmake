# Install script for directory: /home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/mbedtls" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/aes.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/aesni.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/arc4.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/aria.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/asn1.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/asn1write.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/base64.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/bignum.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/blowfish.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/bn_mul.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/camellia.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/ccm.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/certs.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/chacha20.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/chachapoly.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/check_config.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/cipher.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/cipher_internal.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/cmac.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/compat-1.3.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/config.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/config_psa.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/constant_time.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/ctr_drbg.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/debug.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/des.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/dhm.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/ecdh.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/ecdsa.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/ecjpake.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/ecp.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/ecp_internal.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/entropy.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/entropy_poll.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/error.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/gcm.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/havege.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/hkdf.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/hmac_drbg.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/md.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/md2.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/md4.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/md5.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/md_internal.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/memory_buffer_alloc.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/net.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/net_sockets.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/nist_kw.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/oid.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/padlock.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/pem.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/pk.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/pk_internal.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/pkcs11.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/pkcs12.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/pkcs5.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/platform.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/platform_time.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/platform_util.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/poly1305.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/psa_util.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/ripemd160.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/rsa.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/rsa_internal.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/sha1.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/sha256.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/sha512.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/ssl.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/ssl_cache.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/ssl_ciphersuites.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/ssl_cookie.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/ssl_internal.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/ssl_ticket.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/threading.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/timing.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/version.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/x509.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/x509_crl.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/x509_crt.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/x509_csr.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/mbedtls/xtea.h"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/psa" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/psa/crypto.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/psa/crypto_builtin_composites.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/psa/crypto_builtin_primitives.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/psa/crypto_compat.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/psa/crypto_config.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/psa/crypto_driver_common.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/psa/crypto_driver_contexts_composites.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/psa/crypto_driver_contexts_primitives.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/psa/crypto_extra.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/psa/crypto_platform.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/psa/crypto_se_driver.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/psa/crypto_sizes.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/psa/crypto_struct.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/psa/crypto_types.h"
    "/home/fyou/Downloads/ur-rpc-mastered-main/ur-rpc-mastered/pkg_src/deps/mbedtls/include/psa/crypto_values.h"
    )
endif()

