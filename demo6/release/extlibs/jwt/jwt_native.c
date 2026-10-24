/* JWT HS256 wrapper backed by XRT codecs and crypto primitives. */
#include <string.h>

#define XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_CRYPTO_HMAC_SHA256
#define XRT_MODULE_JSON
#define XRT_MODULE_ONCE
#define XRT_MODULE_THREAD_KEY
#include "xrt.h"
#include "include/jwt_native.h"

typedef struct XJwtParts {
    const char* Header;
    size_t HeaderSize;
    const char* Payload;
    size_t PayloadSize;
    const char* Signature;
    size_t SignatureSize;
} XJwtParts;

static const char g_xjwt_header[] = "{\"alg\":\"HS256\",\"typ\":\"JWT\"}";

typedef struct XJwtState {
    char Error[256];
} XJwtState;

static xonce g_xjwt_once = XRT_ONCE_INIT;
static xthreadkey* g_xjwt_key;

static void xjwt_state_destroy(ptr data)
{
    xrtFree(data);
}

static bool xjwt_key_create(ptr unused)
{
    (void)unused;
    g_xjwt_key = xrtThreadKeyCreate(xjwt_state_destroy);
    return g_xjwt_key != NULL;
}

/* TCC JIT 不依赖静态 TLS；线程退出时由 XRT 释放诊断状态。 */
static XJwtState* xjwt_state_get(void)
{
    XJwtState* state;
    if (!xrtOnce(&g_xjwt_once, xjwt_key_create, NULL)) return NULL;
    state = (XJwtState*)xrtThreadKeyGet(g_xjwt_key);
    if (state != NULL) return state;
    state = (XJwtState*)xrtCalloc(1u, sizeof(*state));
    if (state == NULL) return NULL;
    if (!xrtThreadKeySet(g_xjwt_key, state)) {
        xrtFree(state);
        return NULL;
    }
    return state;
}

static void xjwt_set_error(const char* text)
{
    XJwtState* state = xjwt_state_get();
    size_t size = text != NULL ? strlen(text) : 0u;

    if (state == NULL) return;
    /* 包 API 通过返回值和 xjwt_error 报错，不把内部解析错误遗留给调用方。 */
    xrtClearError();
    if (size >= sizeof(state->Error)) size = sizeof(state->Error) - 1u;
    if (size != 0u) memcpy(state->Error, text, size);
    state->Error[size] = '\0';
}

static const xbase64config* xjwt_base64_config(void)
{
    static const xbase64config config = {
        NULL,
        (uint32)XBASE64_URL | (uint32)XBASE64_NO_PADDING
    };
    return &config;
}

static bool xjwt_json_object_valid(const char* text, size_t size)
{
    size_t i = 0u;
    while (i < size && (text[i] == ' ' || text[i] == '\t' ||
        text[i] == '\r' || text[i] == '\n')) ++i;
    return i < size && text[i] == '{' &&
        xrtJsonValid((xstrview){ text, size });
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
    XJwtState* state = xjwt_state_get();
    return state != NULL ? state->Error : "out of memory";
}

char* xjwt_encode(const char* payload_json, const char* secret)
{
    str header = NULL;
    str payload = NULL;
    str signature = NULL;
    xbuffer signing;
    xbuffer token;
    uint8 mac[XRT_SHA256_SIZE];
    char* result = NULL;

    if (payload_json == NULL || !xjwt_json_object_valid(payload_json, strlen(payload_json)) ||
        secret == NULL || secret[0] == '\0') {
        xjwt_set_error("JWT claims must be a JSON object and secret must not be empty");
        return NULL;
    }
    if (!xrtBufferInit(&signing) || !xrtBufferInit(&token)) {
        xjwt_set_error("out of memory");
        return NULL;
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
         !xrtBufferAppend(&token, (xbytesview){ (cbytes)signature, strlen(signature) }) ||
         !xrtBufferAppendByte(&token, 0u) ) {
        xjwt_set_error("out of memory");
        goto cleanup;
    }
    result = (char*)xrtBufferTake(&token, NULL, NULL);
    if (result != NULL) xjwt_set_error("");

cleanup:
    xrtSecureZero(mac, sizeof(mac));
    xrtFree(header);
    xrtFree(payload);
    xrtFree(signature);
    xrtBufferUnit(&signing);
    xrtBufferUnit(&token);
    return result;
}

char* xjwt_decode(const char* token)
{
    XJwtParts parts;
    bytes payload = NULL;
    size_t payload_size = 0u;
    if ( !xjwt_split(token, &parts) ||
         !xjwt_decode_segment(parts.Payload, parts.PayloadSize, &payload, &payload_size) ||
         !xjwt_json_object_valid((const char*)payload, payload_size) ) {
        xrtFree(payload);
        xjwt_set_error("token is not a valid compact JWT");
        return NULL;
    }
    xjwt_set_error("");
    return (char*)payload;
}

bool xjwt_verify(const char* token, const char* secret)
{
    XJwtParts parts;
    bytes signature = NULL;
    bytes payload = NULL;
    size_t signature_size = 0u;
    size_t payload_size = 0u;
    uint8 expected[XRT_SHA256_SIZE];
    size_t signing_size;
    bool valid = false;

    memset(expected, 0, sizeof(expected));
    if ( secret == NULL || secret[0] == '\0' || !xjwt_split(token, &parts) ||
         !xjwt_is_hs256_header(&parts) ||
         !xjwt_decode_segment(parts.Payload, parts.PayloadSize, &payload, &payload_size) ||
         !xjwt_json_object_valid((const char*)payload, payload_size) ||
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
    xrtFree(payload);
    xrtFree(signature);
    return valid;
}

char* xjwt_header(const char* token)
{
    XJwtParts parts;
    bytes header = NULL;
    size_t header_size = 0u;
    if ( !xjwt_split(token, &parts) ||
         !xjwt_decode_segment(parts.Header, parts.HeaderSize, &header, &header_size) ||
         !xjwt_json_object_valid((const char*)header, header_size) ) {
        xrtFree(header);
        xjwt_set_error("token header is not valid Base64URL");
        return NULL;
    }
    xjwt_set_error("");
    return (char*)header;
}

/* XLang point 的物理 ABI 是 void*；C 公开入口保持 char*。 */
void* xjwt_encode_xl(const char* payload_json, const char* secret)
{ return xjwt_encode(payload_json, secret); }
void* xjwt_decode_xl(const char* token)
{ return xjwt_decode(token); }
void* xjwt_header_xl(const char* token)
{ return xjwt_header(token); }
