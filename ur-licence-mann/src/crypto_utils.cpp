#include "crypto_utils.hpp"
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <fstream> // Include for file stream operations

std::string CryptoUtils::encrypt_aes256(const std::string& plaintext, const std::string& key_hex) {
    if (!validate_hex_key(key_hex, 64)) { // 32 bytes = 64 hex chars
        std::cerr << "Invalid AES-256 key format or size" << std::endl;
        return "";
    }

    try {
        // Convert hex key to bytes
        auto key_bytes = hex_to_bytes(key_hex);

        // Generate random IV
        std::vector<unsigned char> iv(16); // AES block size
        if (RAND_bytes(iv.data(), iv.size()) != 1) {
            std::cerr << "Failed to generate random IV" << std::endl;
            return "";
        }

        // Initialize encryption context
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            std::cerr << "Failed to create cipher context" << std::endl;
            return "";
        }

        // Initialize encryption
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key_bytes.data(), iv.data()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            std::cerr << "Failed to initialize encryption" << std::endl;
            return "";
        }

        // Encrypt data
        std::vector<unsigned char> ciphertext(plaintext.length() + AES_BLOCK_SIZE);
        int len;
        int ciphertext_len;

        if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, 
                             reinterpret_cast<const unsigned char*>(plaintext.c_str()), 
                             plaintext.length()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            std::cerr << "Failed to encrypt data" << std::endl;
            return "";
        }
        ciphertext_len = len;

        // Finalize encryption
        if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            std::cerr << "Failed to finalize encryption" << std::endl;
            return "";
        }
        ciphertext_len += len;

        EVP_CIPHER_CTX_free(ctx);

        // Combine IV + ciphertext and convert to hex
        std::vector<unsigned char> result;
        result.insert(result.end(), iv.begin(), iv.end());
        result.insert(result.end(), ciphertext.begin(), ciphertext.begin() + ciphertext_len);

        return bytes_to_hex(result);

    } catch (const std::exception& e) {
        std::cerr << "Exception during encryption: " << e.what() << std::endl;
        return "";
    }
}

std::string CryptoUtils::decrypt_aes256(const std::string& ciphertext_hex, const std::string& key_hex) {
    if (!validate_hex_key(key_hex, 64)) { // 32 bytes = 64 hex chars
        std::cerr << "Invalid AES-256 key format or size" << std::endl;
        return "";
    }

    try {
        // Convert hex to bytes
        auto key_bytes = hex_to_bytes(key_hex);
        auto encrypted_data = hex_to_bytes(ciphertext_hex);

        if (encrypted_data.size() < 16) { // Must have at least IV
            std::cerr << "Invalid ciphertext size" << std::endl;
            return "";
        }

        // Extract IV and ciphertext
        std::vector<unsigned char> iv(encrypted_data.begin(), encrypted_data.begin() + 16);
        std::vector<unsigned char> ciphertext(encrypted_data.begin() + 16, encrypted_data.end());

        // Initialize decryption context
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            std::cerr << "Failed to create cipher context" << std::endl;
            return "";
        }

        // Initialize decryption
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key_bytes.data(), iv.data()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            std::cerr << "Failed to initialize decryption" << std::endl;
            return "";
        }

        // Decrypt data
        std::vector<unsigned char> plaintext(ciphertext.size() + AES_BLOCK_SIZE);
        int len;
        int plaintext_len;

        if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(), ciphertext.size()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            std::cerr << "Failed to decrypt data" << std::endl;
            return "";
        }
        plaintext_len = len;

        // Finalize decryption
        if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            std::cerr << "Failed to finalize decryption" << std::endl;
            return "";
        }
        plaintext_len += len;

        EVP_CIPHER_CTX_free(ctx);

        return std::string(reinterpret_cast<char*>(plaintext.data()), plaintext_len);

    } catch (const std::exception& e) {
        std::cerr << "Exception during decryption: " << e.what() << std::endl;
        return "";
    }
}

bool CryptoUtils::encrypt_file_aes256(const std::string& input_file, const std::string& output_file, const std::string& key) {
    try {
        // Read input file
        std::ifstream input(input_file, std::ios::binary);
        if (!input.is_open()) {
            std::cerr << "Failed to open input file: " << input_file << std::endl;
            return false;
        }

        std::stringstream buffer;
        buffer << input.rdbuf();
        std::string plaintext = buffer.str();
        input.close();

        // Encrypt content
        std::string encrypted = encrypt_aes256(plaintext, key);
        if (encrypted.empty()) {
            std::cerr << "Encryption failed for file: " << input_file << std::endl;
            return false;
        }

        // Write encrypted content
        std::ofstream output(output_file, std::ios::binary);
        if (!output.is_open()) {
            std::cerr << "Failed to open output file: " << output_file << std::endl;
            return false;
        }

        output << encrypted;
        output.close();

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception during file encryption: " << e.what() << std::endl;
        return false;
    }
}

std::string CryptoUtils::bytes_to_hex(const std::vector<unsigned char>& bytes) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned char byte : bytes) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}

std::vector<unsigned char> CryptoUtils::hex_to_bytes(const std::string& hex) {
    std::vector<unsigned char> bytes;

    // Remove any whitespace and ensure even length
    std::string clean_hex = hex;
    clean_hex.erase(std::remove_if(clean_hex.begin(), clean_hex.end(), ::isspace), clean_hex.end());

    if (clean_hex.length() % 2 != 0) {
        return bytes; // Invalid hex string
    }

    for (size_t i = 0; i < clean_hex.length(); i += 2) {
        std::string byte_string = clean_hex.substr(i, 2);
        try {
            unsigned char byte = static_cast<unsigned char>(std::stoi(byte_string, nullptr, 16));
            bytes.push_back(byte);
        } catch (const std::exception&) {
            return std::vector<unsigned char>(); // Return empty vector on error
        }
    }

    return bytes;
}

std::string CryptoUtils::generate_random_key_hex(int key_size_bytes) {
    std::vector<unsigned char> key(key_size_bytes);
    if (RAND_bytes(key.data(), key_size_bytes) != 1) {
        return "";
    }
    return bytes_to_hex(key);
}

std::string CryptoUtils::sha256(const std::string& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, data.c_str(), data.length());
    SHA256_Final(hash, &sha256);

    std::vector<unsigned char> hash_vec(hash, hash + SHA256_DIGEST_LENGTH);
    return bytes_to_hex(hash_vec);
}

bool CryptoUtils::validate_hex_key(const std::string& key_hex, int expected_size) {
    if (key_hex.length() != static_cast<size_t>(expected_size)) {
        return false;
    }

    // Check if all characters are valid hex
    return std::all_of(key_hex.begin(), key_hex.end(), [](char c) {
        return std::isxdigit(c);
    });
}

bool CryptoUtils::is_content_encrypted(const std::string& content) {
    if (content.empty()) {
        return false;
    }
    
    // Check if content is valid hex string (encrypted content is hex-encoded)
    // Encrypted content should be all hex characters and fairly long
    if (content.length() < 32) {
        return false; // Too short to be encrypted
    }
    
    // Check if all characters are hex digits
    bool all_hex = std::all_of(content.begin(), content.end(), [](char c) {
        return std::isxdigit(c) || std::isspace(c);
    });
    
    if (!all_hex) {
        return false;
    }
    
    // Try to parse as JSON - if it fails, it's likely encrypted
    try {
        // Remove whitespace for checking
        std::string trimmed = content;
        trimmed.erase(std::remove_if(trimmed.begin(), trimmed.end(), ::isspace), trimmed.end());
        
        // Check if it starts with '{' (JSON) - if not and it's all hex, it's encrypted
        if (!trimmed.empty() && trimmed[0] != '{') {
            return all_hex;
        }
        
        return false;
    } catch (...) {
        return all_hex;
    }
}