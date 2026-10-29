#define XRT_MODULE_THREAD
#ifndef XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_BUFFER
#endif
#ifndef XRT_MODULE_TEMP_MEMORY
#define XRT_MODULE_TEMP_MEMORY
#endif
#ifndef XRT_MODULE_CODEC_HEX
#define XRT_MODULE_CODEC_HEX
#endif
#include "../include/blake2_native.h"
#include "../include/xrt_blake2.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct worker {
    bool large;
    char text[24];
    char expected[33];
} worker;

static const char* hash(worker* w, const char* text, size_t size)
{
    return w->large ? xblake2b_hash_str_hex(text, strlen(text), size, "key", 3) :
        xblake2s_hash_str_hex(text, strlen(text), size, "key", 3);
}

static int32 run_worker(ptr data)
{
    worker* w = data;
    unsigned int i, j;
    for (i = 0; i < 200; ++i) {
        const char* saved = hash(w, w->text, 16);
        assert(strcmp(saved, w->expected) == 0);
        /* 同线程连续调用超过旧环形槽容量，也不能覆盖之前借用的临时字符串。 */
        for (j = 0; j < 16; ++j) {
            assert(hash(w, "other input", 16)[0] != 0);
            xrtThreadYield();
            assert(strcmp(saved, w->expected) == 0);
        }
        assert(hash(w, "bad", 0)[0] == 0);
        xrtThreadYield();
        assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
        assert(strcmp(hash(w, w->text, 16), w->expected) == 0);
        assert(xrtGetError() == NULL);
        assert(xrtTempClear());
    }
    xrtTempUnit(xrtTempCurrent());
    return 0;
}

int main(void)
{
    worker workers[4];
    xthread* threads[4];
    unsigned int i;
    uint8 bytes[16];
    uint8 long_key[65] = {0};
    xvalue* key_value;
    void* keyed_b;
    void* keyed_s;
    size_t written;
    assert(xblake2_available() && xblake2_version()[0] != 0);
    assert(!xblake2b_hasher_update(NULL, NULL, 0));
    assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT && xblake2_last_error()[0]);
    assert(!xblake2s_hasher_reset(NULL));
    assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(!xblake2b_hasher_clone(NULL) && xrtErrorKind(xrtGetError()) == XERR_STATE);
    assert(!xblake2s_hasher_digest_bytes(NULL));
    assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(!xblake2b_file_hex("", 0, 16, NULL)[0]);
    assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    xblake2b_hasher_destroy(NULL);
    xblake2s_hasher_destroy(NULL);
    xrtClearError();

    assert(!xblake2b_hasher_create_keyed_bytes(16, NULL, 0));
    assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    key_value = xrtValueBytes((xbytesview){long_key, sizeof(long_key)});
    assert(key_value != NULL);
    assert(!xblake2b_hasher_create_keyed_bytes(16, key_value, 0));
    assert(!xblake2s_hasher_create_keyed_bytes(16, key_value, 0));
    xrtValueRelease(key_value);
    key_value = xrtValueBytes((xbytesview){(const uint8*)"k", 1u});
    assert(key_value != NULL);
    keyed_b = xblake2b_hasher_create_keyed_bytes(16, key_value, 0);
    keyed_s = xblake2s_hasher_create_keyed_bytes(16, key_value, 0);
    xrtValueRelease(key_value);
    assert(keyed_b != NULL && keyed_s != NULL);
    assert(xblake2b_hasher_update_str(keyed_b, "abc", 3));
    assert(xblake2s_hasher_update_str(keyed_s, "abc", 3));
    assert(strcmp(xblake2b_hasher_digest_hex(keyed_b),
        xblake2b_hash_str_hex("abc", 3, 16, "k", 1)) == 0);
    assert(strcmp(xblake2s_hasher_digest_hex(keyed_s),
        xblake2s_hash_str_hex("abc", 3, 16, "k", 1)) == 0);
    xblake2b_hasher_destroy(keyed_b);
    xblake2s_hasher_destroy(keyed_s);

    /* 每个线程独立创建临时区，结果和错误通过线程自己的上下文保存。 */
    for (i = 0; i < 4; ++i) {
        workers[i].large = i % 2 == 0;
        snprintf(workers[i].text, sizeof(workers[i].text), "worker-%u", i);
        if (workers[i].large) assert(xrtBlake2bHash(workers[i].text, strlen(workers[i].text), 16, "key", 3, bytes, NULL));
        else assert(xrtBlake2sHash(workers[i].text, strlen(workers[i].text), 16, "key", 3, bytes, NULL));
        assert(xrtHexEncode(bytes, sizeof(bytes), workers[i].expected, sizeof(workers[i].expected), &written, 0));
        threads[i] = xrtThreadCreate(run_worker, &workers[i], 0);
        assert(threads[i]);
    }
    for (i = 0; i < 4; ++i) {
        assert(xrtThreadWait(threads[i]) == XWAIT_OK && xrtThreadExitCode(threads[i]) == 0);
        xrtThreadDestroy(threads[i]);
    }
    assert(xrtGetError() == NULL);
    puts("blake2 native: invalid handles, 4 independent threads, temporary results and errors passed");
    return 0;
}
