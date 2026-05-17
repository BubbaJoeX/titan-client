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
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <random>
#include <algorithm>

namespace Nuna
{

// ======================================================================
// Hardcoded TitanPak encryption password
// This is embedded in the binary - do not share source code publicly
// ======================================================================

static const char* const TITANPAK_PASSWORD = "T1t4n_S3cur3_P4k_K3y_2024!@#$";

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

    // Get the hardcoded TitanPak password (literal; does not read environment).
    static const char* getTitanPakPassword()
    {
        return TITANPAK_PASSWORD;
    }

    static std::string trimAsciiWhitespaceCopy(std::string s)
    {
        while (!s.empty() && (static_cast<unsigned char>(s.back()) <= 32u || s.back() == '\t'))
            s.pop_back();
        size_t i = 0;
        while (i < s.size() && (static_cast<unsigned char>(s[i]) <= 32u || s[i] == '\t'))
            ++i;
        return s.substr(i);
    }

    static void appendUniquePassword(std::vector<std::string>& out, const std::string& cand)
    {
        if (cand.empty())
            return;
        for (const auto& x : out)
            if (x == cand)
                return;
        out.push_back(cand);
    }

    /// Ordered XOR keystream candidates for NUNA/LEGE: empty password (salt-only deriveKey),
    /// explicit GUI/CLI, SWG_TRE_PASSWORD, built-in Titan key. All are tried until TOC decodes.
    static std::vector<std::string> trePasswordCandidates(const std::string& explicitPasswordOrEmptyFromUi)
    {
        std::vector<std::string> c;
        c.emplace_back(); // deriveKey early-outs: key is salt + zero padding only
        const std::string ex = trimAsciiWhitespaceCopy(explicitPasswordOrEmptyFromUi);
        appendUniquePassword(c, ex);
        if (const char* e = std::getenv("SWG_TRE_PASSWORD"))
            appendUniquePassword(c, trimAsciiWhitespaceCopy(std::string(e)));
        appendUniquePassword(c, std::string(TITANPAK_PASSWORD));
        return c;
    }

    /// Empty explicitPassword → `SWG_TRE_PASSWORD` env (if set), else built-in TitanPak key.
    static std::string resolveTrePassword(const std::string& explicitPassword)
    {
        const std::string t = trimAsciiWhitespaceCopy(explicitPassword);
        if (!t.empty())
            return t;
        if (const char* e = std::getenv("SWG_TRE_PASSWORD"))
        {
            const std::string v = trimAsciiWhitespaceCopy(std::string(e));
            if (!v.empty())
                return v;
        }
        return std::string(TITANPAK_PASSWORD);
    }

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

    /// Standard RC4 (ARCFOUR): `output[n] = input[n] ^ KS[n]`. `input` and `output` may alias for in-place.
    /// No-op if `keyLen == 0` or `len == 0`.
    static void rc4Crypt(const uint8_t* key, size_t keyLen, const uint8_t* input, size_t len, uint8_t* output)
    {
        if (!key || keyLen == 0 || !input || !output || len == 0)
            return;

        uint8_t S[256];
        for (int i = 0; i < 256; ++i)
            S[i] = static_cast<uint8_t>(i);
        int j = 0;
        for (int i = 0; i < 256; ++i)
        {
            j = (j + S[i] + key[static_cast<size_t>(i) % keyLen]) & 255;
            std::swap(S[static_cast<unsigned>(i)], S[static_cast<unsigned>(j)]);
        }
        int i = 0;
        j = 0;
        for (size_t n = 0; n < len; ++n)
        {
            i = (i + 1) & 255;
            j = (j + S[static_cast<unsigned>(i)]) & 255;
            std::swap(S[static_cast<unsigned>(i)], S[static_cast<unsigned>(j)]);
            const uint8_t K = S[(S[static_cast<unsigned>(i)] + S[static_cast<unsigned>(j)]) & 255];
            output[n] = static_cast<uint8_t>(input[n] ^ K);
        }
    }

    /// RC4 (ARCFOUR) after discarding the first `dropKeystreamBytes` PRGA output bytes (some stacks sync this way).
    static void rc4CryptDropKeystream(const uint8_t* key, size_t keyLen, size_t dropKeystreamBytes,
                                      const uint8_t* input, size_t len, uint8_t* output)
    {
        if (!key || keyLen == 0 || !input || !output || len == 0)
            return;

        uint8_t S[256];
        for (int i = 0; i < 256; ++i)
            S[i] = static_cast<uint8_t>(i);
        int j = 0;
        for (int i = 0; i < 256; ++i)
        {
            j = (j + S[i] + key[static_cast<size_t>(i) % keyLen]) & 255;
            std::swap(S[static_cast<unsigned>(i)], S[static_cast<unsigned>(j)]);
        }
        int i = 0;
        j = 0;
        for (size_t n = 0; n < dropKeystreamBytes; ++n)
        {
            i = (i + 1) & 255;
            j = (j + S[static_cast<unsigned>(i)]) & 255;
            std::swap(S[static_cast<unsigned>(i)], S[static_cast<unsigned>(j)]);
            (void)S[(S[static_cast<unsigned>(i)] + S[static_cast<unsigned>(j)]) & 255];
        }
        for (size_t n = 0; n < len; ++n)
        {
            i = (i + 1) & 255;
            j = (j + S[static_cast<unsigned>(i)]) & 255;
            std::swap(S[static_cast<unsigned>(i)], S[static_cast<unsigned>(j)]);
            const uint8_t K = S[(S[static_cast<unsigned>(i)] + S[static_cast<unsigned>(j)]) & 255];
            output[n] = static_cast<uint8_t>(input[n] ^ K);
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
