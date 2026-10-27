#define XRT_MODULE_VALUE_CONTAINER
#include "include/memcached_native.h"
#include <stdio.h>
#include <string.h>

#define XMC_DOMAIN "xlang.extlib.memcached"

static void xmc_error(const char *message)
{
    xrtSetErrorInfo(XERR_VALUE, XMC_DOMAIN, 0, message);
}

static bool xmc_key_valid(xstrview key)
{
    size_t i;
    if (key.Data == NULL || key.Size == 0 || key.Size > 250) return false;
    for (i = 0; i < key.Size; ++i) {
        unsigned char c = (unsigned char)key.Data[i];
        if (c <= 32 || c == 127) return false;
    }
    return true;
}

static char *xmc_storage(xstrview key, int64 flags, int64 exptime,
    int64 bytes, const char *verb)
{
    char *line;
    int size;
    if (!xmc_key_valid(key) || flags < 0 || flags > UINT32_MAX ||
        exptime < 0 || exptime > UINT32_MAX || bytes < 0 ||
        bytes > 67108864) {
        xmc_error("invalid storage command parameters");
        return NULL;
    }
    line = xrtMalloc(key.Size + 96);
    if (line == NULL) return NULL;
    size = snprintf(line, key.Size + 96, "%s %.*s %llu %llu %llu\r\n",
        verb, (int)key.Size, key.Data, (unsigned long long)flags,
        (unsigned long long)exptime, (unsigned long long)bytes);
    if (size < 0 || (size_t)size >= key.Size + 96) {
        xrtFree(line);
        xmc_error("storage command too long");
        return NULL;
    }
    return line;
}

char *xmc_build_set(xstrview key, int64 flags, int64 exptime, int64 bytes)
{
    return xmc_storage(key, flags, exptime, bytes, "set");
}

char *xmc_build_add(xstrview key, int64 flags, int64 exptime, int64 bytes)
{
    return xmc_storage(key, flags, exptime, bytes, "add");
}

char *xmc_build_get(xvalue *keys)
{
    size_t length = 5;
    size_t position = 3;
    int64 count;
    int64 i;
    char *line;
    if (keys == NULL || xrtValueType(keys) != XVALUE_ARRAY ||
        (count = xrtValueCount(keys)) < 1) {
        xmc_error("get requires a nonempty key array");
        return NULL;
    }
    for (i = 0; i < count; ++i) {
        xstrview key;
        xvalue *item = xrtValueArrayAt(keys, i);
        if (item == NULL || !xrtValueGetString(item, &key) ||
            !xmc_key_valid(key) || length + key.Size + 1 > 1048576) {
            xmc_error("invalid get key or command too long");
            return NULL;
        }
        length += key.Size + 1;
    }
    line = xrtMalloc(length + 1);
    if (line == NULL) return NULL;
    memcpy(line, "get", 3);
    for (i = 0; i < count; ++i) {
        xstrview key;
        xrtValueGetString(xrtValueArrayAt(keys, i), &key);
        line[position++] = ' ';
        memcpy(line + position, key.Data, key.Size);
        position += key.Size;
    }
    memcpy(line + position, "\r\n", 3);
    return line;
}

static char *xmc_key_command(xstrview key, const char *verb,
    int64 exptime, bool withTime)
{
    char *line;
    int size;
    if (!xmc_key_valid(key) || (withTime &&
        (exptime < 0 || exptime > UINT32_MAX))) {
        xmc_error("invalid key command parameters");
        return NULL;
    }
    line = xrtMalloc(key.Size + 64);
    if (line == NULL) return NULL;
    if (withTime) {
        size = snprintf(line, key.Size + 64, "%s %.*s %llu\r\n",
            verb, (int)key.Size, key.Data, (unsigned long long)exptime);
    } else {
        size = snprintf(line, key.Size + 64, "%s %.*s\r\n",
            verb, (int)key.Size, key.Data);
    }
    if (size < 0 || (size_t)size >= key.Size + 64) {
        xrtFree(line);
        xmc_error("key command too long");
        return NULL;
    }
    return line;
}

char *xmc_build_delete(xstrview key)
{
    return xmc_key_command(key, "delete", 0, false);
}

char *xmc_build_touch(xstrview key, int64 exptime)
{
    return xmc_key_command(key, "touch", exptime, true);
}

const char *xmc_build_stats(void) { return "stats\r\n"; }
const char *xmc_build_flush_all(void) { return "flush_all\r\n"; }
const char *xmc_build_version(void) { return "version\r\n"; }

/* point ABI forwards ownership unchanged; no text wrapping or copy. */
bool xmc_key_valid_abi(xstrview key) { return xmc_key_valid(key); }
void *xmc_build_set_abi(xstrview key, int64 flags, int64 exptime, int64 bytes)
{ return xmc_build_set(key, flags, exptime, bytes); }
void *xmc_build_add_abi(xstrview key, int64 flags, int64 exptime, int64 bytes)
{ return xmc_build_add(key, flags, exptime, bytes); }
void *xmc_build_delete_abi(xstrview key) { return xmc_build_delete(key); }
void *xmc_build_touch_abi(xstrview key, int64 exptime)
{ return xmc_build_touch(key, exptime); }
