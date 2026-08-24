#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <bcrypt.h>
#include <vector>
#include <string>
#include <cstdint>

#pragma comment(lib, "bcrypt.lib")

namespace MiioCrypto
{
    // UTF-8 与 WideChar (UTF-16) 字符串安全转换辅助
    inline std::wstring Utf8ToWide(const std::string& utf8Str)
    {
        if (utf8Str.empty()) return L"";
        int count = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, nullptr, 0);
        if (count <= 0) return L"";
        std::wstring result(count - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, &result[0], count);
        return result;
    }

    inline std::string WideToUtf8(const std::wstring& wideStr)
    {
        if (wideStr.empty()) return "";
        int count = WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (count <= 0) return "";
        std::string result(count - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, &result[0], count, nullptr, nullptr);
        return result;
    }

    // 将十六进制字符串转为字节数组
    inline std::vector<uint8_t> HexToBytes(const std::string& hex)
    {
        std::vector<uint8_t> bytes;
        bytes.reserve(hex.length() / 2);
        for (size_t i = 0; i + 1 < hex.length(); i += 2)
        {
            char byteStr[3] = { hex[i], hex[i + 1], '\0' };
            char* endPtr = nullptr;
            unsigned long val = strtoul(byteStr, &endPtr, 16);
            if (endPtr != byteStr)
            {
                bytes.push_back(static_cast<uint8_t>(val));
            }
        }
        return bytes;
    }

    // 将字节数组转为十六进制字符串
    inline std::string BytesToHex(const uint8_t* data, size_t length)
    {
        if (!data || length == 0) return "";
        static const char hexChars[] = "0123456789abcdef";
        std::string hex;
        hex.reserve(length * 2);
        for (size_t i = 0; i < length; ++i)
        {
            hex.push_back(hexChars[(data[i] >> 4) & 0x0F]);
            hex.push_back(hexChars[data[i] & 0x0F]);
        }
        return hex;
    }

    // BCrypt 算法提供者单例管理类（复用内核句柄，提升高频加密性能）
    class BCryptProvider
    {
    public:
        static BCryptProvider& Instance()
        {
            static BCryptProvider s_instance;
            return s_instance;
        }

        BCRYPT_ALG_HANDLE GetMd5Alg() const { return m_hMd5Alg; }
        BCRYPT_ALG_HANDLE GetAesCbcAlg() const { return m_hAesCbcAlg; }

    private:
        BCryptProvider()
        {
            // 初始化 MD5 算法提供者
            BCryptOpenAlgorithmProvider(&m_hMd5Alg, BCRYPT_MD5_ALGORITHM, nullptr, 0);

            // 初始化 AES-CBC 算法提供者
            if (BCryptOpenAlgorithmProvider(&m_hAesCbcAlg, BCRYPT_AES_ALGORITHM, nullptr, 0) >= 0)
            {
                BCryptSetProperty(m_hAesCbcAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0);
            }
        }

        ~BCryptProvider()
        {
            if (m_hMd5Alg)
            {
                BCryptCloseAlgorithmProvider(m_hMd5Alg, 0);
                m_hMd5Alg = nullptr;
            }
            if (m_hAesCbcAlg)
            {
                BCryptCloseAlgorithmProvider(m_hAesCbcAlg, 0);
                m_hAesCbcAlg = nullptr;
            }
        }

        BCRYPT_ALG_HANDLE m_hMd5Alg = nullptr;
        BCRYPT_ALG_HANDLE m_hAesCbcAlg = nullptr;
    };

    // RAII 密钥管理
    struct ScopedBCryptKey
    {
        BCRYPT_KEY_HANDLE hKey = nullptr;
        ~ScopedBCryptKey()
        {
            if (hKey)
            {
                BCryptDestroyKey(hKey);
                hKey = nullptr;
            }
        }
    };

    // 计算数据的 MD5 哈希（16 字节）
    inline bool ComputeMD5(const uint8_t* data, size_t length, uint8_t output[16])
    {
        if (!data || !output) return false;

        BCRYPT_ALG_HANDLE hAlg = BCryptProvider::Instance().GetMd5Alg();
        if (!hAlg) return false;

        BCRYPT_HASH_HANDLE hHash = nullptr;
        NTSTATUS status = BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
        if (status < 0) return false;

        status = BCryptHashData(hHash, (PUCHAR)data, static_cast<ULONG>(length), 0);
        if (status >= 0)
        {
            status = BCryptFinishHash(hHash, (PUCHAR)output, 16, 0);
        }

        BCryptDestroyHash(hHash);
        return (status >= 0);
    }

    // AES-128-CBC 加密（带 PKCS7 填充）
    inline bool Aes128CbcEncrypt(const std::vector<uint8_t>& plaintext,
                                 const uint8_t key[16],
                                 const uint8_t iv[16],
                                 std::vector<uint8_t>& ciphertext)
    {
        if (!key || !iv) return false;

        BCRYPT_ALG_HANDLE hAlg = BCryptProvider::Instance().GetAesCbcAlg();
        if (!hAlg) return false;

        ScopedBCryptKey keyHolder;
        NTSTATUS status = BCryptGenerateSymmetricKey(hAlg, &keyHolder.hKey, nullptr, 0, (PUCHAR)key, 16, 0);
        if (status < 0) return false;

        // 进行 PKCS7 填充
        size_t padLen = 16 - (plaintext.size() % 16);
        std::vector<uint8_t> paddedData = plaintext;
        paddedData.insert(paddedData.end(), padLen, static_cast<uint8_t>(padLen));

        // 复制 IV（加密过程中会被底层驱动修改）
        uint8_t currentIv[16];
        memcpy(currentIv, iv, 16);

        ciphertext.resize(paddedData.size());
        ULONG cbData = 0;
        status = BCryptEncrypt(keyHolder.hKey,
                               paddedData.data(),
                               static_cast<ULONG>(paddedData.size()),
                               nullptr,
                               currentIv,
                               16,
                               ciphertext.data(),
                               static_cast<ULONG>(ciphertext.size()),
                               &cbData,
                               0);

        if (status >= 0)
        {
            ciphertext.resize(cbData);
            return true;
        }
        return false;
    }

    // AES-128-CBC 解密（去除 PKCS7 填充）
    inline bool Aes128CbcDecrypt(const std::vector<uint8_t>& ciphertext,
                                 const uint8_t key[16],
                                 const uint8_t iv[16],
                                 std::vector<uint8_t>& plaintext)
    {
        if (ciphertext.empty() || (ciphertext.size() % 16 != 0) || !key || !iv) return false;

        BCRYPT_ALG_HANDLE hAlg = BCryptProvider::Instance().GetAesCbcAlg();
        if (!hAlg) return false;

        ScopedBCryptKey keyHolder;
        NTSTATUS status = BCryptGenerateSymmetricKey(hAlg, &keyHolder.hKey, nullptr, 0, (PUCHAR)key, 16, 0);
        if (status < 0) return false;

        uint8_t currentIv[16];
        memcpy(currentIv, iv, 16);

        std::vector<uint8_t> decrypted(ciphertext.size());
        ULONG cbData = 0;
        status = BCryptDecrypt(keyHolder.hKey,
                               (PUCHAR)ciphertext.data(),
                               static_cast<ULONG>(ciphertext.size()),
                               nullptr,
                               currentIv,
                               16,
                               decrypted.data(),
                               static_cast<ULONG>(decrypted.size()),
                               &cbData,
                               0);

        if (status < 0 || cbData == 0) return false;

        // 去除 PKCS7 填充并严谨校验边界
        uint8_t padLen = decrypted[cbData - 1];
        if (padLen == 0 || padLen > 16 || padLen > cbData) return false;

        for (size_t i = cbData - padLen; i < cbData; ++i)
        {
            if (decrypted[i] != padLen) return false;
        }

        plaintext.assign(decrypted.begin(), decrypted.begin() + (cbData - padLen));
        return true;
    }
}
