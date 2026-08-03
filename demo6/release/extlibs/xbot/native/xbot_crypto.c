/*
 * xbot_crypto.c - 加密签名、URL 编码、通用 HTTP 请求、INI 读取
 * 为火山云手机 API 提供 HMAC-SHA256 签名支持。
 * TCC 兼容，使用 Windows CryptoAPI (advapi32)。
 */
#include "xbot_native.h"
#include <wincrypt.h>

/* 链接 advapi32 (CryptoAPI) */
#pragma comment(lib, "advapi32")

/* ─── SHA256 ─── */

static char sha256_buf[128];

const char *xxbot_sha256_hex(const char *data) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE hash[32];
    DWORD hash_len = 32;
    int i;

    if (!data) data = "";

    if (!CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        xrtSetError("SHA256: CryptAcquireContext failed", 0);
        return "";
    }
    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        xrtSetError("SHA256: CryptCreateHash failed", 0);
        return "";
    }
    if (!CryptHashData(hHash, (const BYTE *)data, (DWORD)strlen(data), 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        xrtSetError("SHA256: CryptHashData failed", 0);
        return "";
    }
    if (!CryptGetHashParam(hHash, HP_HASHVAL, hash, &hash_len, 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        xrtSetError("SHA256: CryptGetHashParam failed", 0);
        return "";
    }
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    for (i = 0; i < 32; i++) {
        sprintf(sha256_buf + i * 2, "%02x", hash[i]);
    }
    sha256_buf[64] = '\0';
    return sha256_buf;
}

/* ─── HMAC-SHA256 ─── */

static char hmac_buf[128];

const char *xxbot_hmac_sha256_hex(const char *key, const char *data) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    HCRYPTKEY hKey = 0;
    BYTE hash[32];
    DWORD hash_len = 32;
    int i;

    /* HMAC 需要导入密钥 */
    typedef struct {
        BLOBHEADER hdr;
        DWORD len;
        BYTE keyData[256];
    } HMAC_KEY_BLOB;

    HMAC_KEY_BLOB blob;
    DWORD key_len;

    if (!key) key = "";
    if (!data) data = "";
    key_len = (DWORD)strlen(key);
    if (key_len > 256) key_len = 256;

    if (!CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        xrtSetError("HMAC: CryptAcquireContext failed", 0);
        return "";
    }

    /* 构造 PLAINTEXTKEYBLOB */
    memset(&blob, 0, sizeof(blob));
    blob.hdr.bType = PLAINTEXTKEYBLOB;
    blob.hdr.bVersion = CUR_BLOB_VERSION;
    blob.hdr.reserved = 0;
    blob.hdr.aiKeyAlg = CALG_RC2;  /* 用于导入原始密钥 */
    blob.len = key_len;
    memcpy(blob.keyData, key, key_len);

    if (!CryptImportKey(hProv, (BYTE *)&blob, sizeof(BLOBHEADER) + 4 + key_len, 0, CRYPT_IPSEC_HMAC_KEY, &hKey)) {
        /* 回退：使用简单 HMAC 方式 */
        CryptReleaseContext(hProv, 0);
        /* 使用手动 HMAC 实现 */
        {
            BYTE k_ipad[64], k_opad[64];
            BYTE tk[32];
            DWORD tk_len = 32;
            HCRYPTHASH hInner = 0, hOuter = 0;
            size_t data_len = strlen(data);
            BYTE *inner_data;
            DWORD inner_len;

            /* 如果 key > 64 字节，先 hash */
            if (key_len > 64) {
                HCRYPTHASH hKeyHash = 0;
                CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hKeyHash);
                CryptHashData(hKeyHash, (const BYTE *)key, key_len, 0);
                CryptGetHashParam(hKeyHash, HP_HASHVAL, tk, &tk_len, 0);
                CryptDestroyHash(hKeyHash);
                key = (const char *)tk;
                key_len = tk_len;
            }

            memset(k_ipad, 0x36, 64);
            memset(k_opad, 0x5c, 64);
            for (i = 0; i < (int)key_len; i++) {
                k_ipad[i] ^= (BYTE)key[i];
                k_opad[i] ^= (BYTE)key[i];
            }

            /* inner hash: SHA256(k_ipad || data) */
            CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hInner);
            CryptHashData(hInner, k_ipad, 64, 0);
            CryptHashData(hInner, (const BYTE *)data, (DWORD)data_len, 0);
            CryptGetHashParam(hInner, HP_HASHVAL, tk, &tk_len, 0);
            CryptDestroyHash(hInner);

            /* outer hash: SHA256(k_opad || inner_hash) */
            CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hOuter);
            CryptHashData(hOuter, k_opad, 64, 0);
            CryptHashData(hOuter, tk, 32, 0);
            CryptGetHashParam(hOuter, HP_HASHVAL, hash, &hash_len, 0);
            CryptDestroyHash(hOuter);

            CryptReleaseContext(hProv, 0);

            for (i = 0; i < 32; i++) {
                sprintf(hmac_buf + i * 2, "%02x", hash[i]);
            }
            hmac_buf[64] = '\0';
            return hmac_buf;
        }
    }

    /* 使用 CryptoAPI HMAC */
    {
        HMAC_INFO info;
        memset(&info, 0, sizeof(info));
        info.HashAlgid = CALG_SHA_256;

        if (!CryptCreateHash(hProv, CALG_HMAC, hKey, 0, &hHash)) {
            CryptDestroyKey(hKey);
            CryptReleaseContext(hProv, 0);
            xrtSetError("HMAC: CryptCreateHash failed", 0);
            return "";
        }
        CryptSetHashParam(hHash, HP_HMAC_INFO, (BYTE *)&info, 0);
        CryptHashData(hHash, (const BYTE *)data, (DWORD)strlen(data), 0);
        CryptGetHashParam(hHash, HP_HASHVAL, hash, &hash_len, 0);
        CryptDestroyHash(hHash);
        CryptDestroyKey(hKey);
        CryptReleaseContext(hProv, 0);
    }

    for (i = 0; i < 32; i++) {
        sprintf(hmac_buf + i * 2, "%02x", hash[i]);
    }
    hmac_buf[64] = '\0';
    return hmac_buf;
}

/* ─── URL 编码 ─── */

static char url_enc_buf[4096];

const char *xxbot_url_encode(const char *value) {
    int i = 0, o = 0;
    if (!value) return "";
    while (value[i] && o < 4090) {
        unsigned char c = (unsigned char)value[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            url_enc_buf[o++] = (char)c;
        } else {
            sprintf(url_enc_buf + o, "%%%02X", c);
            o += 3;
        }
        i++;
    }
    url_enc_buf[o] = '\0';
    return url_enc_buf;
}

/* ─── INI 文件读取 ─── */

static char ini_buf[1024];

const char *xxbot_read_ini_value(const char *file_path, const char *section,
                                  const char *key) {
    DWORD len;
    if (!file_path || !section || !key) return "";

    /* 转为宽字符路径 */
    {
        wchar_t w_path[512], w_section[128], w_key[128], w_result[1024];
        MultiByteToWideChar(CP_UTF8, 0, file_path, -1, w_path, 512);
        MultiByteToWideChar(CP_UTF8, 0, section, -1, w_section, 128);
        MultiByteToWideChar(CP_UTF8, 0, key, -1, w_key, 128);

        len = GetPrivateProfileStringW(w_section, w_key, L"",
                                       w_result, 1024, w_path);
        if (len == 0) return "";

        WideCharToMultiByte(CP_UTF8, 0, w_result, -1, ini_buf, 1024, NULL, NULL);
        return ini_buf;
    }
}
