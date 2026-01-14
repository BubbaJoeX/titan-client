// ======================================================================
//
// TitanPakCrypto.h
// Encryption support for TitanPak (.titanpak) archives
// Copyright (c) Titan Project
//
// This provides decryption for encrypted TitanPak archives.
// Encrypted archives use "NUNA" magic instead of "TREE".
//
// ======================================================================

#ifndef INCLUDED_TitanPakCrypto_H
#define INCLUDED_TitanPakCrypto_H

#include <cstdint>
#include <cstring>
#include <string>

// ======================================================================

class TitanPakCrypto
{
public:
    static const size_t SALT_SIZE = 16;
    static const size_t IV_SIZE = 16;
    static const size_t KEY_SIZE = 32;
    static const size_t BLOCK_SIZE = 256;

    // ======================================================================
    // Encryption Header (follows standard TRE header for encrypted files)
    // ======================================================================
    
    struct EncryptionHeader
    {
        uint32_t encryptionVersion;    // Encryption version (1 = stream cipher)
        uint8_t  salt[16];             // Salt for key derivation
        uint8_t  iv[16];               // Initialization vector
        uint32_t flags;                // Encryption flags
    };

    // ======================================================================
    // Derive a key from password and salt
    // ======================================================================
    
    static void deriveKey(const char* password, 
                          size_t passwordLen,
                          const uint8_t* salt, 
                          uint8_t* key)
    {
        // Initialize key with salt
        memcpy(key, salt, SALT_SIZE);
        memset(key + SALT_SIZE, 0, KEY_SIZE - SALT_SIZE);
        
        if (passwordLen == 0) return;
        
        // Mix password into key multiple times
        for (int round = 0; round < 1000; ++round)
        {
            for (size_t i = 0; i < KEY_SIZE; ++i)
            {
                uint8_t passChar = static_cast<uint8_t>(password[i % passwordLen]);
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

    // ======================================================================
    // Expand key into keystream for decryption
    // ======================================================================
    
    static void expandKeystream(const uint8_t* key, 
                                const uint8_t* iv,
                                uint8_t* keystream, 
                                size_t length)
    {
        // Initialize state
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
            swapBytes(state[i], state[j]);
        }
        
        // Generate keystream
        size_t si = 0;
        j = 0;
        for (size_t k = 0; k < length; ++k)
        {
            si = (si + 1) % BLOCK_SIZE;
            j = (j + state[si]) % BLOCK_SIZE;
            swapBytes(state[si], state[j]);
            keystream[k] = state[(state[si] + state[j]) % BLOCK_SIZE];
        }
    }

    // ======================================================================
    // Decrypt data in-place at a specific file offset
    // The offset is mixed into the IV for random access support
    // ======================================================================
    
    static void decryptAt(uint8_t* data, 
                          size_t length,
                          const uint8_t* key,
                          const uint8_t* baseIv,
                          uint64_t offset)
    {
        // Create offset-dependent IV for random access
        uint8_t offsetIv[IV_SIZE];
        memcpy(offsetIv, baseIv, IV_SIZE);
        for (size_t i = 0; i < 8 && i < IV_SIZE; ++i)
        {
            offsetIv[i] ^= static_cast<uint8_t>((offset >> (i * 8)) & 0xFF);
        }
        
        // Generate keystream and XOR with data
        uint8_t* keystream = new uint8_t[length];
        expandKeystream(key, offsetIv, keystream, length);
        
        for (size_t i = 0; i < length; ++i)
        {
            data[i] ^= keystream[i];
        }
        
        delete[] keystream;
    }

    // ======================================================================
    // Decrypt data in-place (convenience overload for full buffer)
    // ======================================================================
    
    static void decrypt(uint8_t* data, 
                        size_t length,
                        const uint8_t* key,
                        const uint8_t* iv)
    {
        decryptAt(data, length, key, iv, 0);
    }

private:
    static uint8_t rotateLeft(uint8_t value, int shift)
    {
        return static_cast<uint8_t>((value << shift) | (value >> (8 - shift)));
    }
    
    static void swapBytes(uint8_t& a, uint8_t& b)
    {
        uint8_t temp = a;
        a = b;
        b = temp;
    }
};

// ======================================================================
// Encryption context for a single encrypted archive
// ======================================================================

class TitanPakEncryptionContext
{
public:
    TitanPakEncryptionContext()
        : m_initialized(false)
    {
        memset(m_key, 0, sizeof(m_key));
        memset(m_iv, 0, sizeof(m_iv));
    }
    
    // Initialize with password and encryption header from file
    void initialize(const char* password, const TitanPakCrypto::EncryptionHeader& header)
    {
        size_t passwordLen = password ? strlen(password) : 0;
        TitanPakCrypto::deriveKey(password, passwordLen, header.salt, m_key);
        memcpy(m_iv, header.iv, TitanPakCrypto::IV_SIZE);
        m_initialized = true;
    }
    
    // Decrypt data at a specific offset
    void decryptAt(uint8_t* data, size_t length, uint64_t offset) const
    {
        if (!m_initialized) return;
        TitanPakCrypto::decryptAt(data, length, m_key, m_iv, offset);
    }
    
    // Decrypt data (at offset 0)
    void decrypt(uint8_t* data, size_t length) const
    {
        decryptAt(data, length, 0);
    }
    
    bool isInitialized() const { return m_initialized; }
    
private:
    uint8_t m_key[TitanPakCrypto::KEY_SIZE];
    uint8_t m_iv[TitanPakCrypto::IV_SIZE];
    bool    m_initialized;
};

// ======================================================================

#endif // INCLUDED_TitanPakCrypto_H
