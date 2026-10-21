#ifndef XRT_MODULE_THREAD
#define XRT_MODULE_THREAD
#endif
#ifndef XRT_MODULE_TEMP_MEMORY
#define XRT_MODULE_TEMP_MEMORY
#endif
#ifndef XRT_MODULE_CODEC_HEX
#define XRT_MODULE_CODEC_HEX
#endif
#ifndef XRT_MODULE_VALUE
#define XRT_MODULE_VALUE
#endif
#include "../include/xrt_blake3.h"
#include "../include/blake3_native.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char key[] = "whats the Elvish word for friend";
typedef struct worker { char text[24], expected[33]; } worker;

static int32 run_worker(ptr data)
{
    worker* w = data;
    unsigned int i, j;
    void* h = xblake3_hasher_create(128);
    assert(h);
    for (i = 0; i < 200; ++i) {
        const char* saved = xblake3_hash_str_hex(w->text, strlen(w->text), 16);
        assert(strcmp(saved, w->expected) == 0);
        /* 连续结果超过旧环形槽数量，先前借用的结果仍保持有效。 */
        for (j = 0; j < 16; ++j) {
            assert(xblake3_hash_str_hex("other", 5, 65)[0]);
            xrtThreadYield();
            assert(strcmp(saved, w->expected) == 0);
        }
        assert(!xblake3_hash_str_hex("bad", 3, 0)[0]);
        xrtThreadYield();
        assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
        assert(xblake3_hasher_reset(h));
        assert(xblake3_hasher_update_str(h, w->text, strlen(w->text)));
        assert(strcmp(xblake3_hasher_digest_hex(h, 16), w->expected) == 0);
        assert(xrtGetError() == NULL && xrtTempClear());
    }
    xblake3_hasher_destroy(h);
    xrtTempUnit(xrtTempCurrent());
    return 0;
}

static void boundaries(void)
{
    uint8 input[3] = {0, 1, 255}, digest[131];
    char expected[263];
    size_t written;
    xrtresourcelimits limits;
    void* h;
    void* copy;
    xbuffer* bytes;
    xvalue* key_value;
    assert(xblake3_available() && strcmp(xblake3_version(), "1.5.0") == 0);
    assert(!xblake3_hasher_update(NULL, NULL, 0));
    assert(xrtErrorKind(xrtGetError()) == XERR_STATE && xblake3_last_error()[0]);
    assert(!xblake3_hasher_reset(NULL) && !xblake3_hasher_clone(NULL));
    assert(!xblake3_hasher_digest_bytes(NULL, 32));
    assert(!xblake3_hasher_digest_hex(NULL, 32)[0]);
    assert(xblake3_hasher_input_bytes(NULL) == 0 && xblake3_hasher_adopt(NULL) == NULL);
    xblake3_hasher_destroy(NULL);
    assert(!xblake3_hash_hex(NULL, 1, 32)[0]);
    assert(!xblake3_hash_str_bytes(NULL, 1, 32));
    assert(!xblake3_hash_hex(input, sizeof(input), SIZE_MAX)[0]);
    assert(!xblake3_hash_bytes(input, sizeof(input), 0));
    assert(!xblake3_hash_mac_hex(input, 3, NULL, 32, 32)[0]);
    assert(!xblake3_hash_mac_str_hex("x", 1, key, 31, 32)[0]);
    assert(!xblake3_hash_mac_str_hex("x", 1, key, 33, 32)[0]);
    assert(!xblake3_hasher_create_keyed((void*)key, 31, 0));
    assert(!xblake3_hasher_create_keyed_bytes(NULL, 0));
    key_value = xrtValueBytes((xbytesview){(const uint8*)key, 1u});
    assert(key_value != NULL);
    assert(!xblake3_hasher_create_keyed_bytes(key_value, 0));
    assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    xrtValueRelease(key_value);
    assert(!xblake3_hash_kdf_hex(input, 3, NULL, 0, 32)[0]);
    assert(!xblake3_hasher_create_kdf(NULL, 0, 0));
    assert(!xblake3_file_hex("", 0, 32, NULL)[0]);
    assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);

    /* 超预算在申请缓冲区前拒绝，不用超大分配作为尺寸检查。 */
    xrtResourceLimitsInit(&limits);
    assert(!xblake3_hash_str_hex("x", 1, (size_t)limits.iMaxOutputBytes + 1)[0]);
    assert(xrtErrorKind(xrtGetError()) == XERR_RANGE);
    assert(!xblake3_file_hex("missing", 0, (size_t)limits.iMaxOutputBytes + 1, NULL)[0]);
    assert(xrtErrorKind(xrtGetError()) == XERR_RANGE);

    assert(xrtBlake3Hash(input, 3, digest, sizeof(digest), NULL));
    assert(xrtHexEncode(digest, sizeof(digest), expected, sizeof(expected), &written, 0));
    assert(strcmp(xblake3_hash_hex(input, 3, 131), expected) == 0);
    bytes = xblake3_hash_bytes(input, 3, 131);
    assert(bytes && xrtBufferView(bytes).Size == 131 && memcmp(xrtBufferView(bytes).Data, digest, 131) == 0);
    xrtBufferDestroy(bytes);
    h = xblake3_hasher_create(3);
    assert(h && xblake3_hasher_adopt(h) == h && xblake3_hasher_update(h, input, 3));
    assert(xblake3_hasher_input_bytes(h) == 3);
    copy = xblake3_hasher_clone(h);
    assert(copy && strcmp(xblake3_hasher_digest_hex(copy, 131), expected) == 0);
    bytes = xblake3_hasher_digest_bytes(h, 131);
    assert(bytes && memcmp(xrtBufferView(bytes).Data, digest, 131) == 0);
    xrtBufferDestroy(bytes);
    assert(!xblake3_hasher_update(h, input, 1));
    assert(xrtErrorKind(xrtGetError()) == XERR_RANGE && xblake3_hasher_reset(h));
    assert(strcmp(xblake3_hasher_digest_hex(copy, 131), expected) == 0);
    xblake3_hasher_destroy(copy);
    xblake3_hasher_destroy(h);

    h = xblake3_hasher_create_keyed((void*)key, 32, 0);
    assert(h && xblake3_hasher_update_str(h, "abc", 3));
    assert(strcmp(xblake3_hasher_digest_hex(h, 65), xblake3_hash_mac_str_hex("abc", 3, key, 32, 65)) == 0);
    xblake3_hasher_destroy(h);
    key_value = xrtValueBytes((xbytesview){(const uint8*)key, 32u});
    assert(key_value != NULL);
    h = xblake3_hasher_create_keyed_bytes(key_value, 0);
    xrtValueRelease(key_value);
    assert(h && xblake3_hasher_update_str(h, "abc", 3));
    assert(strcmp(xblake3_hasher_digest_hex(h, 65),
        xblake3_hash_mac_str_hex("abc", 3, key, 32, 65)) == 0);
    xblake3_hasher_destroy(h);
    h = xblake3_hasher_create_kdf("xlang test", 10, 0);
    assert(h && xblake3_hasher_update_str(h, "abc", 3));
    assert(strcmp(xblake3_hasher_digest_hex(h, 65), xblake3_hash_kdf_str_hex("abc", 3, "xlang test", 10, 65)) == 0);
    xblake3_hasher_destroy(h);
    assert(xrtTempClear());
}

int main(void)
{
    worker workers[4];
    xthread* threads[4];
    unsigned int i;
    uint8 digest[16];
    size_t written;
    boundaries();
    for (i = 0; i < 4; ++i) {
        snprintf(workers[i].text, sizeof(workers[i].text), "worker-%u", i);
        assert(xrtBlake3Hash(workers[i].text, strlen(workers[i].text), digest, sizeof(digest), NULL));
        assert(xrtHexEncode(digest, sizeof(digest), workers[i].expected, sizeof(workers[i].expected), &written, 0));
        threads[i] = xrtThreadCreate(run_worker, &workers[i], 0);
        assert(threads[i]);
    }
    for (i = 0; i < 4; ++i) {
        assert(xrtThreadWait(threads[i]) == XWAIT_OK && xrtThreadExitCode(threads[i]) == 0);
        xrtThreadDestroy(threads[i]);
    }
    assert(xrtGetError() == NULL);
    xrtTempUnit(xrtTempCurrent());
    puts("blake3 native: invalid inputs, budgets, binary, 4 independent threads and temporary results passed");
    return 0;
}
