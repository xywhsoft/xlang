#ifndef XRT_MODULE_CODEC_HEX
#define XRT_MODULE_CODEC_HEX
#endif
#include "../include/xrt_blake2.h"
/* 测试调用位于断言中，发布配置也必须执行。 */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "vectors.h"

static void set_unused_limit(xrtresourcelimits* limits, int field)
{
    switch (field) {
    case 0: limits->iMaxOutputBytes = 1; break;
    case 1: limits->iMaxItemBytes = 1; break;
    case 2: limits->iMaxEntries = 1; break;
    case 3: limits->iMaxNodes = 1; break;
    case 4: limits->iMaxDepth = 1; break;
    case 5: limits->iMaxCompressionRatio = 1; break;
    case 6: limits->iFlags = 1; break;
    }
}

/* 两种算法走各自的真实 C 原型，不用函数指针强转掩盖 ABI 差异。 */
typedef struct test_hash {
    bool large;
    xrtblake2bhasher* b;
    xrtblake2shasher* s;
} test_hash;

static test_hash create(bool large, const xrtblake2options* options,
    size_t size, const void* key, size_t keylen)
{
    test_hash h = {large, NULL, NULL};
    if (large) h.b = xrtBlake2bHasherCreate(options, size, key, keylen);
    else h.s = xrtBlake2sHasherCreate(options, size, key, keylen);
    return h;
}

static bool valid(test_hash h) { return h.large ? h.b != NULL : h.s != NULL; }
static bool update(test_hash h, const void* data, size_t size)
{ return h.large ? xrtBlake2bHasherUpdate(h.b, data, size) : xrtBlake2sHasherUpdate(h.s, data, size); }
static bool digest(test_hash h, uint8* out, size_t size)
{ return h.large ? xrtBlake2bHasherDigest(h.b, out, size) : xrtBlake2sHasherDigest(h.s, out, size); }
static bool reset(test_hash h)
{ return h.large ? xrtBlake2bHasherReset(h.b) : xrtBlake2sHasherReset(h.s); }
static uint64 consumed(test_hash h)
{ return h.large ? xrtBlake2bHasherInputBytes(h.b) : xrtBlake2sHasherInputBytes(h.s); }
static size_t configured(test_hash h)
{ return h.large ? xrtBlake2bHasherDigestSize(h.b) : xrtBlake2sHasherDigestSize(h.s); }
static const xrtblake2error* error(test_hash h)
{ return h.large ? xrtBlake2bHasherError(h.b) : xrtBlake2sHasherError(h.s); }
static void destroy(test_hash h)
{ if (h.large) xrtBlake2bHasherDestroy(h.b); else xrtBlake2sHasherDestroy(h.s); }
static test_hash copy(test_hash h)
{
    if (h.large) h.b = xrtBlake2bHasherClone(h.b);
    else h.s = xrtBlake2sHasherClone(h.s);
    return h;
}
static bool once(bool large, const void* data, size_t size, size_t outlen,
    const void* key, size_t keylen, uint8* out, xrtblake2error* e)
{
    return large ? xrtBlake2bHash(data, size, outlen, key, keylen, out, e) :
        xrtBlake2sHash(data, size, outlen, key, keylen, out, e);
}
static bool stream(bool large, xreader* reader, const xrtresourcelimits* limits,
    size_t size, uint8* out, xrtprogressproc progress, ptr user, xrtblake2error* e)
{
    return large ? xrtBlake2bFile(reader, limits, size, out, progress, user, e) :
        xrtBlake2sFile(reader, limits, size, out, progress, user, e);
}

static void expect_hex(const uint8* out, size_t size, const char* expected)
{
    char text[129];
    size_t written = 0;
    assert(xrtHexEncode(out, size, text, sizeof(text), &written, 0));
    assert(written == size * 2 && strcmp(text, expected) == 0);
}

static void known_answers(void)
{
    static uint8 input[65538];
    uint8 key[64], borrowed[64], out[66], prefix[64];
    size_t i, v;
    for (i = 0; i < sizeof(input); ++i) input[i] = (uint8)(i * 29 + 7);
    for (i = 0; i < sizeof(key); ++i) key[i] = (uint8)i;
    for (v = 0; v < sizeof(vectors) / sizeof(vectors[0]); ++v) {
        bool large = vectors[v].algorithm == 'b';
        size_t size = vectors[v].size, n = vectors[v].output, k = vectors[v].key;
        size_t split = size / 2;
        test_hash h, other;
        xrtblake2error e;
        memset(out, 0xa5, sizeof(out));
        assert(once(large, input, size, n, key, k, out + 1, &e));
        assert(e.iCode == XRT_BLAKE2_OK && out[0] == 0xa5 && out[n + 1] == 0xa5);
        expect_hex(out + 1, n, vectors[v].hex);

        memcpy(borrowed, key, sizeof(key));
        h = create(large, NULL, n, borrowed, k);
        memset(borrowed, 0xcc, sizeof(borrowed));
        assert(valid(h) && configured(h) == n && update(h, NULL, 0));
        assert(update(h, input, split));
        assert(digest(h, out, n));
        assert(once(large, input, split, n, key, k, prefix, NULL));
        assert(memcmp(out, prefix, n) == 0 && consumed(h) == split);
        other = copy(h);
        assert(valid(other));
        assert(update(h, input + split, size - split));
        assert(update(other, input + split, size + 1 - split));
        assert(digest(h, out, n));
        expect_hex(out, n, vectors[v].hex);
        assert(digest(h, out, n));
        expect_hex(out, n, vectors[v].hex);
        assert(digest(other, out, n));
        assert(once(large, input, size + 1, n, key, k, prefix, NULL));
        assert(memcmp(out, prefix, n) == 0);
        destroy(other);

        /* 重置必须保留构造时复制的密钥，不借用已经变化的外部内存。 */
        assert(reset(h) && consumed(h) == 0);
        assert(update(h, input, size) && digest(h, out, n));
        expect_hex(out, n, vectors[v].hex);
        destroy(h);
    }
}

static void invalid_inputs(bool large)
{
    size_t n = large ? 64 : 32;
    uint8 key[65] = {0}, out[65], untouched[65];
    xrtblake2error e;
    xrtblake2options options;
    test_hash h, bad;
    int field;
    memset(out, 0x5a, sizeof(out));
    memcpy(untouched, out, sizeof(out));
    assert(!once(large, NULL, 1, n, NULL, 0, out, &e));
    assert(!once(large, "x", 1, n, NULL, 1, out, &e));
    assert(!once(large, "x", 1, n, key, n + 1, out, &e));
    assert(!once(large, "x", 1, 0, NULL, 0, out, &e));
    assert(!once(large, "x", 1, n + 1, NULL, 0, out, &e));
    assert(!once(large, "x", 1, n, NULL, 0, NULL, &e));
    assert(e.iCode == XRT_BLAKE2_ERROR_ARGUMENT);
    assert(memcmp(out, untouched, sizeof(out)) == 0);
    bad = create(large, NULL, n, key, n + 1);
    assert(!valid(bad) && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    xrtBlake2OptionsInit(&options);
    ++options.iVersion;
    assert(!valid(create(large, &options, n, NULL, 0)));
    xrtBlake2OptionsInit(&options);
    --options.iSize;
    assert(!valid(create(large, &options, n, NULL, 0)));
    xrtBlake2OptionsInit(&options);
    ++options.tLimits.iVersion;
    assert(!valid(create(large, &options, n, NULL, 0)));
    xrtBlake2OptionsInit(&options);
    --options.tLimits.iSize;
    assert(!valid(create(large, &options, n, NULL, 0)));
    xrtBlake2OptionsInit(&options);
    options.tLimits.iReserved = 1;
    assert(!valid(create(large, &options, n, NULL, 0)) &&
        xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    for (field = 0; field < 7; ++field) {
        xrtBlake2OptionsInit(&options);
        set_unused_limit(&options.tLimits, field);
        assert(!valid(create(large, &options, n, NULL, 0)) &&
            xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    }
    xrtClearError();

    h = create(large, NULL, 16, NULL, 0);
    assert(valid(h));
    assert(!digest(h, out, 15) && !digest(h, out, 17) && !digest(h, NULL, 16));
    assert(error(h)->iCode == XRT_BLAKE2_ERROR_ARGUMENT);
    assert(memcmp(out, untouched, sizeof(out)) == 0);
    assert(digest(h, out, 16) && update(h, "abc", 3));
    assert(!update(h, NULL, 1));
    assert(!digest(h, out, 16) && !update(h, "d", 1));
    assert(error(h)->iCode == XRT_BLAKE2_ERROR_ARGUMENT && consumed(h) == 3);
    assert(!valid(copy(h)) && xrtErrorKind(xrtGetError()) == XERR_STATE);
    xrtClearError();
    assert(reset(h) && update(h, "abc", 3) && digest(h, out, 16));
    destroy(h);

    xrtBlake2OptionsInit(&options);
    options.tLimits.iMaxInputBytes = 3;
    h = create(large, &options, n, key, n);
    assert(valid(h) && update(h, "abc", 3) && digest(h, out, n));
    assert(!update(h, "d", 1) && error(h)->iCode == XRT_BLAKE2_ERROR_LIMIT);
    assert(consumed(h) == 3 && !digest(h, out, n));
    assert(reset(h) && update(h, "abc", 3));
    destroy(h);

    /* 无业务限额时也先校验计数溢出，不能读取这个刻意过大的输入。 */
    if (SIZE_MAX == UINT64_MAX) {
        options.tLimits.iMaxInputBytes = 0;
        h = create(large, &options, n, NULL, 0);
        assert(valid(h) && update(h, "x", 1));
        assert(!update(h, "x", SIZE_MAX));
        assert(error(h)->iCode == XRT_BLAKE2_ERROR_LIMIT && consumed(h) == 1);
        assert(reset(h) && update(h, NULL, 0));
        destroy(h);
    }
    bad = (test_hash){large, NULL, NULL};
    assert(!update(bad, NULL, 0) && !digest(bad, out, n) && !reset(bad));
    assert(consumed(bad) == 0 && configured(bad) == 0 && error(bad) == NULL);
    destroy(bad);
}

typedef struct progress_state {
    unsigned int calls, finals;
    uint64 expected, last;
    bool known;
    int cancel;
} progress_state;

static bool progress(const xrtprogress* p, ptr user)
{
    progress_state* s = user;
    assert(p->iSize == sizeof(*p) && p->iVersion == XRT_PROGRESS_VERSION);
    assert(p->iInputBytes >= s->last && p->iOutputBytes == 0);
    assert(p->iTotalInputBytes == (s->known ? s->expected : 0));
    assert(((p->iFlags & XRT_PROGRESS_TOTAL_KNOWN) != 0) == s->known);
    s->last = p->iInputBytes;
    ++s->calls;
    if (p->iFlags & XRT_PROGRESS_FINAL) ++s->finals;
    return s->cancel != 1 && !(s->cancel == 2 && s->finals != 0);
}

typedef struct reader_state { unsigned int reads, closed; bool fail; } reader_state;
static bool read_some(ptr user, ptr out, size_t requested, size_t* read)
{
    reader_state* s = user;
    assert(requested > 0);
    *read = 0;
    if (s->reads++ == 0) { *(uint8*)out = 'x'; *read = 1; return true; }
    if (s->fail) { xrtSetErrorInfo(XERR_IO, "test.blake2", 0, "read marker"); return false; }
    return true;
}
static bool close_reader(ptr user) { ++((reader_state*)user)->closed; return true; }

static void reader_boundaries(bool large)
{
    static uint8 input[65537];
    uint8 out[64], expected[64], sentinel[64];
    size_t n = large ? 64 : 32;
    xrtresourcelimits limits;
    xrtblake2error e;
    xreader* reader;
    progress_state p;
    uint64 position;
    int mode, field;
    xrtBlake2LimitsInit(&limits);
    assert(once(large, input + 1, sizeof(input) - 1, n, NULL, 0, expected, NULL));
    reader = xrtReaderFromMemory((xbytesview){input, sizeof(input)});
    assert(reader && xrtReaderSeek(reader, 1, XSEEK_START, &position) && position == 1);
    p = (progress_state){0, 0, sizeof(input) - 1, 0, true, 0};
    assert(stream(large, reader, &limits, n, out, progress, &p, &e));
    assert(p.calls == 2 && p.finals == 1 && p.last == sizeof(input) - 1);
    assert(memcmp(out, expected, n) == 0);
    xrtReaderDestroy(reader);

    memset(sentinel, 0x7c, sizeof(sentinel));
    for (mode = 0; mode < 7; ++mode) {
        reader = xrtReaderFromMemory((xbytesview){input, sizeof(input)});
        assert(reader);
        memcpy(out, sentinel, sizeof(out));
        xrtBlake2LimitsInit(&limits);
        p = (progress_state){0, 0, sizeof(input), 0, true, 0};
        if (mode == 0) limits.iMaxInputBytes = sizeof(input) - 1;
        else if (mode == 1) ++limits.iVersion;
        else if (mode == 2) p.cancel = 1;
        else if (mode == 3) p.cancel = 2;
        else if (mode == 5) --limits.iSize;
        else if (mode == 6) limits.iReserved = 1;
        assert(!stream(large, reader, &limits, mode == 4 ? 0 : n, out, progress, &p, &e));
        assert(memcmp(out, sentinel, sizeof(out)) == 0);
        assert(e.iCode == (mode == 0 ? XRT_BLAKE2_ERROR_LIMIT :
            (mode == 2 || mode == 3) ? XRT_BLAKE2_ERROR_CANCELLED : XRT_BLAKE2_ERROR_ARGUMENT));
        if (mode == 1 || mode >= 4) {
            assert(p.calls == 0 && xrtReaderTell(reader, &position) && position == 0);
        }
        xrtReaderDestroy(reader);
    }
    for (field = 0; field < 7; ++field) {
        reader = xrtReaderFromMemory((xbytesview){input, sizeof(input)});
        assert(reader != NULL);
        xrtBlake2LimitsInit(&limits);
        set_unused_limit(&limits, field);
        memcpy(out, sentinel, sizeof(out));
        assert(!stream(large, reader, &limits, n, out, NULL, NULL, &e) &&
            e.iCode == XRT_BLAKE2_ERROR_ARGUMENT &&
            memcmp(out, sentinel, sizeof(out)) == 0 &&
            xrtReaderTell(reader, &position) && position == 0);
        assert(xrtReaderDestroy(reader));
    }
    reader = xrtReaderFromMemory((xbytesview){NULL, 0});
    p = (progress_state){0, 0, 0, 0, true, 0};
    assert(reader && stream(large, reader, NULL, n, out, progress, &p, &e));
    assert(p.calls == 1 && p.finals == 1);
    assert(once(large, NULL, 0, n, NULL, 0, expected, NULL));
    assert(memcmp(out, expected, n) == 0);
    xrtReaderDestroy(reader);

    /* 非定位流和中途 IO 失败不能依赖文件大小探测，也不能接管 reader 寿命。 */
    for (mode = 0; mode < 2; ++mode) {
        reader_state state = {0, 0, mode != 0};
        xreaderops ops = {read_some, NULL, NULL, NULL, close_reader};
        reader = xrtReaderCreate(&ops, &state);
        assert(reader);
        memcpy(out, sentinel, sizeof(out));
        p = (progress_state){0, 0, 0, 0, false, 0};
        assert(stream(large, reader, NULL, n, out, progress, &p, &e) == (mode == 0));
        assert(state.closed == 0);
        if (mode) assert(e.iCode == XRT_BLAKE2_ERROR_IO && memcmp(out, sentinel, sizeof(out)) == 0);
        else {
            assert(p.calls == 2 && p.finals == 1 && p.last == 1);
            assert(once(large, "x", 1, n, NULL, 0, expected, NULL));
            assert(memcmp(out, expected, n) == 0);
        }
        xrtReaderDestroy(reader);
        assert(state.closed == 1);
        xrtClearError();
    }
}

int main(void)
{
    known_answers();
    invalid_inputs(true);
    invalid_inputs(false);
    reader_boundaries(true);
    reader_boundaries(false);
    xrtClearError();
    printf("blake2 C: %zu known answers, streaming/state/reader boundaries passed\n",
        sizeof(vectors) / sizeof(vectors[0]));
    return 0;
}
