#ifndef XRT_MODULE_VALUE
#define XRT_MODULE_VALUE
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#include "xruntime.h"
#include "third_party/siphash.h"

#include <stdint.h>
#include <string.h>

typedef struct xsiphash128_hasher {
    siphash128_state state;
    uint8_t key[16];
} xsiphash128_hasher;

static void xsiphash128_error(xerrkind code, const char* message)
{
    xrtSetErrorInfo(code, "xlang.extlib.siphash128", 0, message);
}

static bool xsiphash128_key(const xvalue* value, uint8_t out[16])
{
    xbytesview view = {0};
    if (value == NULL || !xrtValueGetBytes(value, &view) ||
        view.Size != 16 || view.Data == NULL) {
        xsiphash128_error(XERR_VALUE, "SipHash key must contain exactly 16 bytes");
        return false;
    }
    memcpy(out, view.Data, 16);
    return true;
}

static bool xsiphash128_bytes(const xvalue* value, xbytesview* out)
{
    if (value != NULL && xrtValueGetBytes(value, out) &&
        (out->Size == 0 || out->Data != NULL)) return true;
    xsiphash128_error(XERR_VALUE, "SipHash data must be bytes");
    return false;
}

static bool xsiphash128_length(const void* data, size_t actual, int64_t limit,
    uint64_t already, size_t* result)
{
    if (limit < -1) {
        xsiphash128_error(XERR_RANGE, "SipHash size is invalid");
        return false;
    }
    if (actual != 0 && data == NULL) {
        xsiphash128_error(XERR_VALUE, "SipHash data pointer is null");
        return false;
    }
    *result = limit < 0 || (uint64_t)limit > (uint64_t)actual ?
        actual : (size_t)limit;
    if ((uint64_t)*result > UINT64_MAX - already) {
        xsiphash128_error(XERR_RANGE, "SipHash input length overflows uint64");
        return false;
    }
    return true;
}

static xbuffer* xsiphash128_output(const siphash128_state* state)
{
    uint8_t digest[16];
    xbuffer* output;
    siphash128_final(state, digest);
    output = xrtBufferCreate();
    if (output != NULL && !xrtBufferAssign(output, (xbytesview){digest, 16})) {
        xerror* error = xrtTakeError();
        xrtBufferDestroy(output);
        xrtSetErrorTake(error);
        output = NULL;
    }
    xrtSecureZero(digest, sizeof(digest));
    return output;
}

static xbuffer* xsiphash128_hash_raw(const void* data, size_t actual,
    const xvalue* keyValue, int64_t limit)
{
    uint8_t key[16];
    siphash128_state state;
    size_t size;
    xbuffer* result;
    if (!xsiphash128_key(keyValue, key)) return NULL;
    if (!xsiphash128_length(data, actual, limit, 0, &size)) {
        xrtSecureZero(key, sizeof(key));
        return NULL;
    }
    siphash128_init(&state, key);
    siphash128_update(&state, data, size);
    result = xsiphash128_output(&state);
    xrtSecureZero(&state, sizeof(state));
    xrtSecureZero(key, sizeof(key));
    return result;
}

void* xsiphash128_hash_string(const char* data, size_t actual,
    xvalue* key, int64_t limit)
{
    return xsiphash128_hash_raw(data, actual, key, limit);
}

void* xsiphash128_hash_bytes(xvalue* data, xvalue* key, int64_t limit)
{
    xbytesview view = {0};
    if (!xsiphash128_bytes(data, &view)) return NULL;
    return xsiphash128_hash_raw(view.Data, view.Size, key, limit);
}

void* xsiphash128_hash_buffer(void* data, size_t actual,
    xvalue* key, int64_t limit)
{
    return xsiphash128_hash_raw(data, actual, key, limit);
}

void* xsiphash128_hasher_create(xvalue* keyValue)
{
    uint8_t key[16];
    xsiphash128_hasher* hasher;
    if (!xsiphash128_key(keyValue, key)) return NULL;
    hasher = (xsiphash128_hasher*)xrtMalloc(sizeof(*hasher));
    if (hasher != NULL) {
        memcpy(hasher->key, key, sizeof(key));
        siphash128_init(&hasher->state, key);
    }
    xrtSecureZero(key, sizeof(key));
    return hasher;
}

void* xsiphash128_hasher_adopt(void* handle)
{
    if (handle == NULL) xsiphash128_error(XERR_STATE, "SipHash hasher is null");
    return handle;
}

void xsiphash128_hasher_destroy(void* handle)
{
    if (handle != NULL) {
        xrtSecureZero(handle, sizeof(xsiphash128_hasher));
        xrtFree(handle);
    }
}

static bool xsiphash128_update_view(void* handle, const void* data,
    size_t actual, int64_t limit)
{
    xsiphash128_hasher* hasher = (xsiphash128_hasher*)handle;
    size_t size;
    if (hasher == NULL) {
        xsiphash128_error(XERR_STATE, "SipHash hasher is null");
        return false;
    }
    if (!xsiphash128_length(data, actual, limit, hasher->state.total, &size))
        return false;
    siphash128_update(&hasher->state, data, size);
    return true;
}

bool xsiphash128_hasher_update_string(void* handle, const char* data,
    size_t actual, int64_t limit)
{
    return xsiphash128_update_view(handle, data, actual, limit);
}

bool xsiphash128_hasher_update_bytes(void* handle, xvalue* data, int64_t limit)
{
    xbytesview view = {0};
    if (!xsiphash128_bytes(data, &view)) return false;
    return xsiphash128_update_view(handle, view.Data, view.Size, limit);
}

bool xsiphash128_hasher_update_raw(void* handle, void* data,
    size_t actual, int64_t limit)
{
    return xsiphash128_update_view(handle, data, actual, limit);
}

void* xsiphash128_hasher_digest(void* handle)
{
    if (handle == NULL) {
        xsiphash128_error(XERR_STATE, "SipHash hasher is null");
        return NULL;
    }
    return xsiphash128_output(&((xsiphash128_hasher*)handle)->state);
}

void xsiphash128_hasher_reset(void* handle)
{
    xsiphash128_hasher* hasher = (xsiphash128_hasher*)handle;
    if (hasher == NULL) {
        xsiphash128_error(XERR_STATE, "SipHash hasher is null");
        return;
    }
    siphash128_init(&hasher->state, hasher->key);
}

void* xsiphash128_hasher_clone(void* handle)
{
    xsiphash128_hasher* copy;
    if (handle == NULL) {
        xsiphash128_error(XERR_STATE, "SipHash hasher is null");
        return NULL;
    }
    copy = (xsiphash128_hasher*)xrtMalloc(sizeof(*copy));
    if (copy != NULL) memcpy(copy, handle, sizeof(*copy));
    return copy;
}

uint64_t xsiphash128_hasher_input_bytes(void* handle)
{
    if (handle == NULL) {
        xsiphash128_error(XERR_STATE, "SipHash hasher is null");
        return 0;
    }
    return ((xsiphash128_hasher*)handle)->state.total;
}
