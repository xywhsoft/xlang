/* RFC 4226/6238 的 HMAC-SHA1 配置；SHA-1 与时钟由 XRT 提供。 */
#ifndef XRT_MODULE_CRYPTO_SHA1
#define XRT_MODULE_CRYPTO_SHA1
#endif
#ifndef XRT_MODULE_TIME
#define XRT_MODULE_TIME
#endif
#include "xrt.h"
#include "include/totp_native.h"
#include <string.h>

#define XTOTP_DOMAIN "xlang.extlib.totp"
#define XTOTP_MAX_SECRET_TEXT 256u
#define XTOTP_MAX_SECRET_BYTES 160u
#define XTOTP_MAX_WINDOW 10

static void xtotp_error(xerrkind kind, const char* message)
{
    xrtSetErrorInfo(kind, XTOTP_DOMAIN, 0, message);
}

static int xtotp_base32_digit(unsigned char value)
{
    if (value >= 'A' && value <= 'Z') return value - 'A';
    if (value >= 'a' && value <= 'z') return value - 'a';
    if (value >= '2' && value <= '7') return value - '2' + 26;
    return -1;
}

/* 接受 RFC 4648 的带/不带尾部填充形式，拒绝非零填充位。 */
static bool xtotp_decode_secret(const char* text, long long length,
    unsigned char output[XTOTP_MAX_SECRET_BYTES], size_t* output_size)
{
    size_t i, symbols = 0u, padding = 0u, bytes = 0u;
    unsigned int pending = 0u, bits = 0u;
    size_t expected_padding;
    if (text == NULL || length <= 0 || length > XTOTP_MAX_SECRET_TEXT) {
        xtotp_error(XERR_RANGE, "base32 secret length must be 1..256 bytes");
        return false;
    }
    for (i = 0u; i < (size_t)length; ++i) {
        int digit;
        if (text[i] == '=') { ++padding; continue; }
        digit = xtotp_base32_digit((unsigned char)text[i]);
        if (digit < 0 || padding != 0u) {
            xtotp_error(XERR_VALUE, "secret is not canonical base32");
            return false;
        }
        pending = (pending << 5) | (unsigned int)digit;
        bits += 5u;
        ++symbols;
        if (bits >= 8u) {
            bits -= 8u;
            output[bytes++] = (unsigned char)(pending >> bits);
            pending &= (1u << bits) - 1u;
        }
    }
    switch (symbols % 8u) {
    case 0u: expected_padding = 0u; break;
    case 2u: expected_padding = 6u; break;
    case 4u: expected_padding = 4u; break;
    case 5u: expected_padding = 3u; break;
    case 7u: expected_padding = 1u; break;
    default: expected_padding = 9u; break;
    }
    if (bytes == 0u || expected_padding == 9u ||
        (padding != 0u && padding != expected_padding) || pending != 0u) {
        xtotp_error(XERR_VALUE, "secret has invalid base32 padding");
        return false;
    }
    *output_size = bytes;
    return true;
}

/* XRT 尚无 HMAC-SHA1 接口；只组合已有的 SHA-1 流函数。 */
static bool xtotp_hmac_sha1(const unsigned char* key, size_t key_size,
    const unsigned char message[8], unsigned char digest[20])
{
    unsigned char hashed_key[20] = {0};
    unsigned char pad[64] = {0};
    unsigned char inner[20] = {0};
    xsha1 state;
    size_t i;
    bool valid = false;
    if (key_size > sizeof(pad)) {
        if (!xrtSha1(key, key_size, hashed_key)) goto done;
        key = hashed_key;
        key_size = sizeof(hashed_key);
    }
    memcpy(pad, key, key_size);
    for (i = 0u; i < sizeof(pad); ++i) pad[i] ^= 0x36u;
    xrtSha1Init(&state);
    if (!xrtSha1Update(&state, pad, sizeof(pad)) ||
        !xrtSha1Update(&state, message, 8u) ||
        !xrtSha1Final(&state, inner)) goto done;
    for (i = 0u; i < sizeof(pad); ++i) pad[i] ^= 0x36u ^ 0x5cu;
    xrtSha1Init(&state);
    valid = xrtSha1Update(&state, pad, sizeof(pad)) &&
        xrtSha1Update(&state, inner, sizeof(inner)) &&
        xrtSha1Final(&state, digest);
done:
    xrtSecureZero(&state, sizeof(state));
    xrtSecureZero(hashed_key, sizeof(hashed_key));
    xrtSecureZero(pad, sizeof(pad));
    xrtSecureZero(inner, sizeof(inner));
    if (!valid) xtotp_error(XERR_INTERNAL, "HMAC-SHA1 calculation failed");
    return valid;
}

static bool xtotp_code(const unsigned char* secret, size_t secret_size,
    uint64 counter, int digits, uint32* output)
{
    static const uint32 moduli[] = {1000000u, 10000000u, 100000000u, 1000000000u};
    unsigned char message[8], digest[20] = {0};
    uint32 value;
    int offset, i;
    for (i = 7; i >= 0; --i) {
        message[i] = (unsigned char)counter;
        counter >>= 8;
    }
    if (!xtotp_hmac_sha1(secret, secret_size, message, digest)) {
        xrtSecureZero(digest, sizeof(digest));
        return false;
    }
    offset = digest[19] & 15;
    value = ((uint32)(digest[offset] & 0x7fu) << 24) |
        ((uint32)digest[offset + 1] << 16) |
        ((uint32)digest[offset + 2] << 8) | (uint32)digest[offset + 3];
    *output = value % moduli[digits - 6];
    xrtSecureZero(digest, sizeof(digest));
    return true;
}

static bool xtotp_options(int time_step, int digits)
{
    if (time_step <= 0 || digits < 6 || digits > 9) {
        xtotp_error(XERR_RANGE, "time step must be positive and digits 6..9");
        return false;
    }
    return true;
}

static void* xtotp_format(uint32 code, int digits)
{
    char* result = (char*)xrtMalloc((size_t)digits + 1u);
    int i;
    if (result == NULL) return NULL;
    for (i = digits - 1; i >= 0; --i) {
        result[i] = (char)('0' + code % 10u);
        code /= 10u;
    }
    result[digits] = '\0';
    return result;
}

void* xtotp_hotp(const char* secret_text, long long secret_length,
    long long counter, int digits)
{
    unsigned char secret[XTOTP_MAX_SECRET_BYTES] = {0};
    size_t secret_size = 0u;
    uint32 code;
    void* result = NULL;
    xrtClearError();
    if (counter < 0 || !xtotp_options(30, digits)) {
        if (counter < 0) xtotp_error(XERR_RANGE, "counter must be nonnegative");
        return NULL;
    }
    if (xtotp_decode_secret(secret_text, secret_length, secret, &secret_size) &&
        xtotp_code(secret, secret_size, (uint64)counter, digits, &code))
        result = xtotp_format(code, digits);
    xrtSecureZero(secret, sizeof(secret));
    return result;
}

void* xtotp_generate_at(const char* secret, long long secret_length,
    long long timestamp, int time_step, int digits)
{
    xrtClearError();
    if (!xtotp_options(time_step, digits)) return NULL;
    if (timestamp < 0) {
        xtotp_error(XERR_RANGE, "timestamp must be nonnegative");
        return NULL;
    }
    return xtotp_hotp(secret, secret_length, timestamp / time_step, digits);
}

void* xtotp_generate(const char* secret, long long secret_length,
    int time_step, int digits)
{
    long long timestamp;
    xrtClearError();
    timestamp = xrtTimeUnix(xrtNow());
    if (xrtGetError() != NULL) return NULL;
    return xtotp_generate_at(secret, secret_length, timestamp, time_step, digits);
}

int xtotp_verify_at(const char* secret_text, long long secret_length,
    const char* candidate, long long candidate_length, long long timestamp,
    int time_step, int digits, int window)
{
    unsigned char secret[XTOTP_MAX_SECRET_BYTES] = {0};
    size_t secret_size = 0u;
    uint64 counter;
    int offset, i, matched = 0;
    xrtClearError();
    if (!xtotp_options(time_step, digits) || timestamp < 0 ||
        window < 0 || window > XTOTP_MAX_WINDOW) {
        if (timestamp < 0 || window < 0 || window > XTOTP_MAX_WINDOW)
            xtotp_error(XERR_RANGE, "timestamp or verification window is invalid");
        return -1;
    }
    if (!xtotp_decode_secret(secret_text, secret_length, secret, &secret_size)) {
        xrtSecureZero(secret, sizeof(secret));
        return -1;
    }
    if (candidate == NULL || candidate_length != digits) goto done;
    for (i = 0; i < digits; ++i)
        if (candidate[i] < '0' || candidate[i] > '9') goto done;
    counter = (uint64)(timestamp / time_step);
    /* 不因提前命中而缩短循环；单次比较始终处理所有数字位。 */
    for (offset = -window; offset <= window; ++offset) {
        uint64 current = counter;
        uint32 code;
        unsigned int mismatch = 0u;
        int valid = 1;
        if (offset < 0) {
            if (current < (uint64)(-offset)) valid = 0;
            else current -= (uint64)(-offset);
        } else {
            current += (uint64)offset;
        }
        if (!xtotp_code(secret, secret_size, current, digits, &code)) {
            matched = -1;
            goto done;
        }
        for (i = digits - 1; i >= 0; --i) {
            mismatch |= (unsigned int)((unsigned char)candidate[i] ^
                (unsigned char)('0' + code % 10u));
            code /= 10u;
        }
        matched |= (mismatch == 0u) & valid;
    }
done:
    xrtSecureZero(secret, sizeof(secret));
    return matched;
}

int xtotp_verify(const char* secret, long long secret_length,
    const char* candidate, long long candidate_length, int time_step,
    int digits, int window)
{
    long long timestamp;
    xrtClearError();
    timestamp = xrtTimeUnix(xrtNow());
    if (xrtGetError() != NULL) return -1;
    return xtotp_verify_at(secret, secret_length, candidate, candidate_length,
        timestamp, time_step, digits, window);
}
