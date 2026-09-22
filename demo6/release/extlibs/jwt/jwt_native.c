/* JWT HS256 wrapper backed by XRT codecs and crypto primitives. */
#include <string.h>

#define XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_CRYPTO_HMAC_SHA256
#define XRT_MODULE_JSON
#include "xrt.h"

#if defined(_WIN32) || defined(_WIN64)
#define XJWT_THREAD __declspec(thread)
#else
#define XJWT_THREAD __thread
#endif

#define XJWT_RESULT_SLOTS 8u

typedef struct XJwtParts {
    const char* Header;
    size_t HeaderSize;
    const char* Payload;
    size_t PayloadSize;
    const char* Signature;
    size_t SignatureSize;
} XJwtParts;

static const char g_xjwt_header[] = "{\"alg\":\"HS256\",\"typ\":\"JWT\"}";
static XJWT_THREAD char g_xjwt_error[256];
static XJWT_THREAD xbuffer g_xjwt_results[XJWT_RESULT_SLOTS];
static XJWT_THREAD size_t g_xjwt_result_index;

static void xjwt_set_error(const char* text)
{
    size_t size = text != NULL ? strlen(text) : 0u;

    if ( size >= sizeof(g_xjwt_error) ) size = sizeof(g_xjwt_error) - 1u;
    if ( size != 0u ) memcpy(g_xjwt_error, text, size);
    g_xjwt_error[size] = '\0';
}

static const char* xjwt_store(const void* data, size_t size)
{
    xbuffer* output = &g_xjwt_results[g_xjwt_result_index++ % XJWT_RESULT_SLOTS];

    xrtBufferClear(output);
    if ( !xrtBufferAppend(output, (xbytesview){ (cbytes)data, size }) ||
         !xrtBufferAppendByte(output, 0u) ) {
        xrtBufferClear(output);
        xjwt_set_error("out of memory");
        return "";
    }
    return (const char*)output->Data;
}

static const xbase64config* xjwt_base64_config(void)
{
    static const xbase64config config = {
        NULL,
        (uint32)XBASE64_URL | (uint32)XBASE64_NO_PADDING
    };
    return &config;
}

static bool xjwt_split(const char* token, XJwtParts* parts)
{
    const char* first;
    const char* second;

    if ( token == NULL || token[0] == '\0' || parts == NULL ) return false;
    first = strchr(token, '.');
    if ( first == NULL || first == token || first[1] == '\0' ) return false;
    second = strchr(first + 1, '.');
    if ( second == NULL || second == first + 1 ||
         strchr(second + 1, '.') != NULL ) return false;

    parts->Header = token;
    parts->HeaderSize = (size_t)(first - token);
    parts->Payload = first + 1;
    parts->PayloadSize = (size_t)(second - (first + 1));
    parts->Signature = second + 1;
    parts->SignatureSize = strlen(second + 1);
    return true;
}

static bool xjwt_decode_segment(const char* text, size_t text_size, bytes* output, size_t* output_size)
{
    bytes decoded;

    if ( output != NULL ) *output = NULL;
    if ( output_size != NULL ) *output_size = 0u;
    decoded = xrtBase64DecodeNew(text, text_size, output_size, xjwt_base64_config());
    if ( decoded == NULL ) return false;
    if ( output != NULL ) *output = decoded;
    else xrtFree(decoded);
    return true;
}

static bool xjwt_is_hs256_header(const XJwtParts* parts)
{
    bytes header = NULL;
    size_t size = 0u;
    xvalue* value = NULL;
    xvalue* algorithm;
    xstrview algorithm_text;
    bool result = false;

    if ( parts == NULL || !xjwt_decode_segment(parts->Header, parts->HeaderSize, &header, &size) ) {
        return false;
    }
    value = xrtJsonParse((xstrview){ (cstr)header, size });
    algorithm = value != NULL ? xrtValueObjectGet(value, XRT_STR_LITERAL("alg")) : NULL;
    result = algorithm != NULL && xrtValueGetString(algorithm, &algorithm_text) &&
        algorithm_text.Size == 5u && memcmp(algorithm_text.Data, "HS256", 5u) == 0;
    xrtValueRelease(value);
    xrtFree(header);
    return result;
}

const char* xjwt_error(void)
{
    return g_xjwt_error;
}

const char* xjwt_encode(const char* payload_json, const char* secret)
{
    str header = NULL;
    str payload = NULL;
    str signature = NULL;
    xbuffer signing;
    xbuffer token;
    uint8 mac[XRT_SHA256_SIZE];
    const char* result = "";

    if ( payload_json == NULL || secret == NULL || secret[0] == '\0' ) {
        xjwt_set_error("payload and secret must not be empty");
        return "";
    }
    if ( !xrtBufferInit(&signing) || !xrtBufferInit(&token) ) {
        xjwt_set_error("out of memory");
        return "";
    }
    xjwt_set_error("");
    header = xrtBase64EncodeNew(g_xjwt_header, sizeof(g_xjwt_header) - 1u, xjwt_base64_config());
    payload = xrtBase64EncodeNew(payload_json, strlen(payload_json), xjwt_base64_config());
    if ( header == NULL || payload == NULL ||
         !xrtBufferAppend(&signing, (xbytesview){ (cbytes)header, strlen(header) }) ||
         !xrtBufferAppendByte(&signing, (uint8)'.') ||
         !xrtBufferAppend(&signing, (xbytesview){ (cbytes)payload, strlen(payload) }) ||
         !xrtHmacSha256(secret, strlen(secret), signing.Data, signing.Size, mac) ) {
        xjwt_set_error("cannot create HS256 signature");
        goto cleanup;
    }
    signature = xrtBase64EncodeNew(mac, sizeof(mac), xjwt_base64_config());
    if ( signature == NULL ||
         !xrtBufferAppend(&token, xrtBufferView(&signing)) ||
         !xrtBufferAppendByte(&token, (uint8)'.') ||
         !xrtBufferAppend(&token, (xbytesview){ (cbytes)signature, strlen(signature) }) ) {
        xjwt_set_error("out of memory");
        goto cleanup;
    }
    result = xjwt_store(token.Data, token.Size);
    if ( result[0] != '\0' ) xjwt_set_error("");

cleanup:
    xrtSecureZero(mac, sizeof(mac));
    xrtFree(header);
    xrtFree(payload);
    xrtFree(signature);
    xrtBufferUnit(&signing);
    xrtBufferUnit(&token);
    return result;
}

const char* xjwt_decode(const char* token)
{
    XJwtParts parts;
    bytes payload = NULL;
    size_t payload_size = 0u;
    const char* result;

    if ( !xjwt_split(token, &parts) ||
         !xjwt_decode_segment(parts.Payload, parts.PayloadSize, &payload, &payload_size) ) {
        xjwt_set_error("token is not a valid compact JWT");
        return "";
    }
    result = xjwt_store(payload, payload_size);
    xrtFree(payload);
    if ( result[0] != '\0' ) xjwt_set_error("");
    return result;
}

long long xjwt_verify(const char* token, const char* secret)
{
    XJwtParts parts;
    bytes signature = NULL;
    size_t signature_size = 0u;
    uint8 expected[XRT_SHA256_SIZE];
    size_t signing_size;
    bool valid = false;

    memset(expected, 0, sizeof(expected));
    if ( secret == NULL || secret[0] == '\0' || !xjwt_split(token, &parts) ||
         !xjwt_is_hs256_header(&parts) ||
         !xjwt_decode_segment(parts.Signature, parts.SignatureSize, &signature, &signature_size) ||
         signature_size != sizeof(expected) ) {
        xjwt_set_error("token is not a canonical HS256 JWT");
        goto cleanup;
    }
    signing_size = parts.HeaderSize + 1u + parts.PayloadSize;
    if ( !xrtHmacSha256(secret, strlen(secret), token, signing_size, expected) ) {
        xjwt_set_error("cannot verify HS256 signature");
        goto cleanup;
    }
    valid = xrtConstTimeEqual(signature, expected, sizeof(expected));
    xjwt_set_error(valid ? "" : "JWT signature does not match");

cleanup:
    xrtSecureZero(expected, sizeof(expected));
    xrtFree(signature);
    return valid ? 1 : 0;
}

const char* xjwt_header(const char* token)
{
    XJwtParts parts;
    bytes header = NULL;
    size_t header_size = 0u;
    const char* result;

    if ( !xjwt_split(token, &parts) ||
         !xjwt_decode_segment(parts.Header, parts.HeaderSize, &header, &header_size) ) {
        xjwt_set_error("token header is not valid Base64URL");
        return "";
    }
    result = xjwt_store(header, header_size);
    xrtFree(header);
    if ( result[0] != '\0' ) xjwt_set_error("");
    return result;
}
