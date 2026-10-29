/*
 * xbot_crypto.c - 加密签名、URL 编码、通用 HTTP 请求、INI 读取
 * 为火山云手机 API 提供 HMAC-SHA256 签名支持。
 * TCC 兼容，使用 Windows CryptoAPI (advapi32)。
 */
#include "xbot_native.h"
/* ─── SHA256 / HMAC-SHA256 ─── */

#define XBOT_CRYPTO_RESULT_SLOTS 4u

#if defined(_WIN32) || defined(_WIN64)
#define XBOT_CRYPTO_THREAD __declspec(thread)
#else
#define XBOT_CRYPTO_THREAD
#endif

static XBOT_CRYPTO_THREAD xbuffer g_xbot_crypto_results[XBOT_CRYPTO_RESULT_SLOTS];
static XBOT_CRYPTO_THREAD size_t g_xbot_crypto_result_index;

static const char *xbot_crypto_hex(const uint8 *pData, size_t iSize)
{
    str sHex = xrtHexEncodeNew(pData, iSize, 0u);
    size_t iSlot = g_xbot_crypto_result_index++ % XBOT_CRYPTO_RESULT_SLOTS;
    xbuffer *pSlot = &g_xbot_crypto_results[iSlot];
    if (sHex == NULL) { return ""; }
    xrtBufferUnit(pSlot);
    if (!xrtBufferSetTake(pSlot, (bytes *)&sHex, strlen(sHex), strlen(sHex) + 1u)) {
        xrtFree(sHex);
        xbot_native_error(XERR_MEMORY, "xbot cryptographic result allocation failed");
        return "";
    }
    return (const char *)pSlot->Data;
}

const char *xxbot_sha256_hex(const char *data)
{
    uint8 aDigest[32];
    const char *sData = data != NULL ? data : "";
    if (!xrtSha256(sData, strlen(sData), aDigest)) { return ""; }
    return xbot_crypto_hex(aDigest, sizeof(aDigest));
}

const char *xxbot_hmac_sha256_hex(const char *key, const char *data)
{
    uint8 aDigest[32];
    const char *sKey = key != NULL ? key : "";
    const char *sData = data != NULL ? data : "";
    if (!xrtHmacSha256(sKey, strlen(sKey), sData, strlen(sData), aDigest)) { return ""; }
    return xbot_crypto_hex(aDigest, sizeof(aDigest));
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
