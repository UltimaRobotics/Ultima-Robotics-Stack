#pragma once

#include <string>
#include <vector>

class CryptoUtils {
public:
    // AES-256 encryption/decryption
    static std::string encrypt_aes256(const std::string& plaintext, const std::string& key_hex);
    static std::string decrypt_aes256(const std::string& ciphertext_hex, const std::string& key_hex);
    static bool encrypt_file_aes256(const std::string& input_file, const std::string& output_file, const std::string& key);

    // Hashing functions
    static std::string sha256(const std::string& data);

    // Utility functions
    static std::string bytes_to_hex(const std::vector<unsigned char>& bytes);
    static std::vector<unsigned char> hex_to_bytes(const std::string& hex);
    static std::string generate_random_key_hex(int key_size_bytes = 32);
    
    // Encryption detection
    static bool is_content_encrypted(const std::string& content);

private:
    static bool validate_hex_key(const std::string& key_hex, int expected_size);
};