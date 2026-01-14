// ======================================================================
//
// NunaCrypto.h
// TRE Archive Encryption Support
// Copyright (c) Titan Project
//
// Provides encryption/decryption for secure TRE archives.
// Uses key derivation and stream cipher for performance.
//
// ======================================================================

#ifndef NUNA_CRYPTO_H
#define NUNA_CRYPTO_H

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <random>
#include <algorithm>

namespace Nuna
{

// ======================================================================
// Simple but effective XOR-based stream cipher with key expansion
// This provides obfuscation; for true security, upgrade to AES
// ======================================================================

class Crypto
{
public:
    static constexpr size_t SALT_SIZE = 16;
    static constexpr size_t IV_SIZE = 16;
    static constexpr size_t KEY_SIZE = 32;
    static constexpr size_t BLOCK_SIZE = 256;

    // Generate random bytes for salt/IV
    static void generateRandom(uint8_t* buffer, size_t size)
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<unsigned int> dist(0, 255);
        
        for (size_t i = 0; i < size; ++i)
        {
            buffer[i] = static_cast<uint8_t>(dist(gen));
        }
    }

    // Derive a key from password and salt using a simple PBKDF-like function
    static void deriveKey(const std::string& password, 
                          const uint8_t* salt, 
                          uint8_t* key)
    {
        // Initialize key with salt
        std::memcpy(key, salt, SALT_SIZE);
        std::memset(key + SALT_SIZE, 0, KEY_SIZE - SALT_SIZE);
        
        // Mix password into key multiple times
        const size_t passLen = password.length();
        if (passLen == 0) return;
        
        for (int round = 0; round < 1000; ++round)
        {
            for (size_t i = 0; i < KEY_SIZE; ++i)
            {
                uint8_t passChar = static_cast<uint8_t>(password[i % passLen]);
                uint8_t saltChar = salt[i % SALT_SIZE];
                
                // Mix function
                key[i] ^= passChar;
                key[i] = rotateLeft(key[i], 3);
                key[i] ^= saltChar;
                key[i] += static_cast<uint8_t>(round & 0xFF);
                
                // Cascade to next byte
                if (i + 1 < KEY_SIZE)
                {
                    key[i + 1] ^= key[i];
                }
            }
        }
    }

    // Expand key into keystream for encryption
    static void expandKeystream(const uint8_t* key, 
                                const uint8_t* iv,
                                uint8_t* keystream, 
                                size_t length)
    {
        // Initialize state with key and IV
        uint8_t state[BLOCK_SIZE];
        for (size_t i = 0; i < BLOCK_SIZE; ++i)
        {
            state[i] = static_cast<uint8_t>(i);
        }
        
        // Key scheduling (RC4-like)
        size_t j = 0;
        for (size_t i = 0; i < BLOCK_SIZE; ++i)
        {
            j = (j + state[i] + key[i % KEY_SIZE] + iv[i % IV_SIZE]) % BLOCK_SIZE;
            std::swap(state[i], state[j]);
        }
        
        // Generate keystream
        size_t si = 0;
        j = 0;
        for (size_t k = 0; k < length; ++k)
        {
            si = (si + 1) % BLOCK_SIZE;
            j = (j + state[si]) % BLOCK_SIZE;
            std::swap(state[si], state[j]);
            keystream[k] = state[(state[si] + state[j]) % BLOCK_SIZE];
        }
    }

    // Encrypt data in-place
    static void encrypt(uint8_t* data, 
                        size_t length,
                        const uint8_t* key,
                        const uint8_t* iv)
    {
        // Process in chunks for efficiency
        std::vector<uint8_t> keystream(length);
        expandKeystream(key, iv, keystream.data(), length);
        
        for (size_t i = 0; i < length; ++i)
        {
            data[i] ^= keystream[i];
        }
    }

    // Decrypt data in-place (symmetric - same as encrypt)
    static void decrypt(uint8_t* data, 
                        size_t length,
                        const uint8_t* key,
                        const uint8_t* iv)
    {
        encrypt(data, length, key, iv);  // XOR is symmetric
    }

    // Encrypt a buffer, returning new encrypted buffer
    static std::vector<uint8_t> encryptBuffer(const uint8_t* data,
                                               size_t length,
                                               const std::string& password,
                                               uint8_t* outSalt,
                                               uint8_t* outIv)
    {
        // Generate salt and IV
        generateRandom(outSalt, SALT_SIZE);
        generateRandom(outIv, IV_SIZE);
        
        // Derive key
        uint8_t key[KEY_SIZE];
        deriveKey(password, outSalt, key);
        
        // Copy and encrypt
        std::vector<uint8_t> result(data, data + length);
        encrypt(result.data(), length, key, outIv);
        
        return result;
    }

    // Decrypt a buffer
    static std::vector<uint8_t> decryptBuffer(const uint8_t* data,
                                               size_t length,
                                               const std::string& password,
                                               const uint8_t* salt,
                                               const uint8_t* iv)
    {
        // Derive key
        uint8_t key[KEY_SIZE];
        deriveKey(password, salt, key);
        
        // Copy and decrypt
        std::vector<uint8_t> result(data, data + length);
        decrypt(result.data(), length, key, iv);
        
        return result;
    }

private:
    static uint8_t rotateLeft(uint8_t value, int shift)
    {
        return static_cast<uint8_t>((value << shift) | (value >> (8 - shift)));
    }
};

// ======================================================================
// Encryption Context - maintains state for streaming encryption
// ======================================================================

class EncryptionContext
{
public:
    EncryptionContext() = default;
    
    // Initialize for encryption (generates salt/IV)
    void initEncrypt(const std::string& password)
    {
        m_password = password;
        Crypto::generateRandom(m_salt, Crypto::SALT_SIZE);
        Crypto::generateRandom(m_iv, Crypto::IV_SIZE);
        Crypto::deriveKey(password, m_salt, m_key);
        m_initialized = true;
    }
    
    // Initialize for decryption (uses existing salt/IV)
    void initDecrypt(const std::string& password,
                     const uint8_t* salt,
                     const uint8_t* iv)
    {
        m_password = password;
        std::memcpy(m_salt, salt, Crypto::SALT_SIZE);
        std::memcpy(m_iv, iv, Crypto::IV_SIZE);
        Crypto::deriveKey(password, salt, m_key);
        m_initialized = true;
    }
    
    // Encrypt data at a specific offset (for random access)
    void encryptAt(uint8_t* data, size_t length, uint64_t offset) const
    {
        if (!m_initialized) return;
        
        // Create offset-dependent IV
        uint8_t offsetIv[Crypto::IV_SIZE];
        std::memcpy(offsetIv, m_iv, Crypto::IV_SIZE);
        for (size_t i = 0; i < 8 && i < Crypto::IV_SIZE; ++i)
        {
            offsetIv[i] ^= static_cast<uint8_t>((offset >> (i * 8)) & 0xFF);
        }
        
        Crypto::encrypt(data, length, m_key, offsetIv);
    }
    
    // Decrypt data at a specific offset
    void decryptAt(uint8_t* data, size_t length, uint64_t offset) const
    {
        encryptAt(data, length, offset);  // Symmetric
    }
    
    // Accessors
    const uint8_t* getSalt() const { return m_salt; }
    const uint8_t* getIv() const { return m_iv; }
    bool isInitialized() const { return m_initialized; }
    
private:
    std::string m_password;
    uint8_t     m_salt[Crypto::SALT_SIZE] = {};
    uint8_t     m_iv[Crypto::IV_SIZE] = {};
    uint8_t     m_key[Crypto::KEY_SIZE] = {};
    bool        m_initialized = false;
};

} // namespace Nuna

#endif // NUNA_CRYPTO_H
