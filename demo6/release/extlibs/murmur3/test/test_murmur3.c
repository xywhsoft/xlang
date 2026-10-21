/* 白盒入口包含算法与核心，不另外链接这两个源文件。 */
#include "../xrt_murmur3.c"
#include "../third_party/MurmurHash3.c"
#include "vectors.h"
#include <assert.h>

static void expect_hex(const uint8 digest[16], const char* expected)
{
    char hex[33];
    unsigned i;
    for (i = 0; i < 16; ++i) snprintf(hex + i * 2, 3, "%02x", digest[i]);
    assert(strcmp(hex, expected) == 0);
}

static void vectors(void)
{
    static uint8 data[65545];
    uint8 digest[16];
    size_t i, offset, consumed, count;
    for (offset = 0; offset < 8; ++offset) {
        for (i = 0; i < 65537; ++i) data[offset + i] = (uint8)(i * 37 + 11);
        for (i = 0; i < sizeof(murmur3_vectors) / sizeof(murmur3_vectors[0]); ++i) {
            size_t size = murmur3_vectors[i].size;
            uint32 seed = murmur3_vectors[i].seed;
            xrtmurmur3hasher* h = xrtMurmur3HasherCreate(NULL, seed);
            murmur3_32_state narrow;
            assert(h);
            assert(xrtMurmur3Hash32(data + offset, size, seed, NULL) == murmur3_vectors[i].h32);
            assert(xrtMurmur3Hash128(data + offset, size, seed, digest, NULL));
            expect_hex(digest, murmur3_vectors[i].hex128);
            murmur3_32_init(&narrow, seed);
            for (consumed = 0; consumed < size; consumed += count) {
                count = consumed % 97 + 1;
                if (count > size - consumed) count = size - consumed;
                assert(xrtMurmur3HasherUpdate(h, data + offset + consumed, count));
                murmur3_32_update(&narrow, data + offset + consumed, count);
            }
            assert(murmur3_32_final(&narrow) == murmur3_vectors[i].h32);
            assert(xrtMurmur3HasherInputBytes(h) == size);
            assert(xrtMurmur3HasherDigest(h, digest));
            expect_hex(digest, murmur3_vectors[i].hex128);
            assert(xrtMurmur3HasherDigest(h, digest) && xrtMurmur3HasherUpdate(h, NULL, 0));
            expect_hex(digest, murmur3_vectors[i].hex128);
            assert(xrtMurmur3HasherReset(h) && xrtMurmur3HasherUpdate(h, data + offset, size));
            assert(xrtMurmur3HasherDigest(h, digest));
            expect_hex(digest, murmur3_vectors[i].hex128);
            xrtMurmur3HasherDestroy(h);
        }
    }
    /* 所有短消息切点覆盖半块填充和直接块处理的交界。 */
    for (i = 0; i <= 64; ++i) for (offset = 0; offset <= i; ++offset) {
        murmur3_128_state wide;
        murmur3_32_state narrow;
        uint8 expected[16];
        murmur3_128_init(&wide, UINT32_MAX);
        murmur3_32_init(&narrow, UINT32_MAX);
        murmur3_128_update(&wide, data, offset);
        murmur3_128_update(&wide, data + offset, i - offset);
        murmur3_32_update(&narrow, data, offset);
        murmur3_32_update(&narrow, data + offset, i - offset);
        murmur3_128_final(&wide, digest);
        assert(xrtMurmur3Hash128(data, i, UINT32_MAX, expected, NULL));
        assert(memcmp(digest, expected, 16) == 0);
        assert(murmur3_32_final(&narrow) == xrtMurmur3Hash32(data, i, UINT32_MAX, NULL));
    }
}

static void states(void)
{
    xrtmurmur3options options;
    xrtmurmur3error error;
    xrtmurmur3hasher *h, *copy;
    uint8 digest[16], sentinel[16];
    memset(sentinel, 0xa5, 16);
    memcpy(digest, sentinel, 16);
    assert(!xrtMurmur3Hash128(NULL, 1, 0, digest, &error) && memcmp(digest, sentinel, 16) == 0);
    assert(error.iCode == XRT_MURMUR3_ERROR_ARGUMENT && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(xrtMurmur3Hash32(NULL, 1, 0, NULL) == 0 && xrtGetError() != NULL);
    assert(!xrtMurmur3Hash128(NULL, 0, 0, NULL, NULL));
    assert(xrtMurmur3Hash32(NULL, 0, 0, &error) == 0 && xrtGetError() == NULL && error.iCode == 0);
    assert(xrtMurmur3Hash128(NULL, 0, 0, digest, NULL));
    expect_hex(digest, "00000000000000000000000000000000");
    assert(!xrtMurmur3HasherUpdate(NULL, NULL, 0));
    assert(!xrtMurmur3HasherDigest(NULL, digest) && !xrtMurmur3HasherReset(NULL));
    assert(!xrtMurmur3HasherClone(NULL) && xrtErrorKind(xrtGetError()) == XERR_STATE);
    assert(xrtMurmur3HasherInputBytes(NULL) == 0 && xrtMurmur3HasherError(NULL) == NULL);
    xrtMurmur3HasherDestroy(NULL);
    xrtMurmur3OptionsInit(&options);
    ++options.iVersion;
    assert(!xrtMurmur3HasherCreate(&options, 0));
    --options.iVersion; --options.iSize;
    assert(!xrtMurmur3HasherCreate(&options, 0));
    ++options.iSize; ++options.tLimits.iVersion;
    assert(!xrtMurmur3HasherCreate(&options, 0));
    xrtMurmur3OptionsInit(&options);
    --options.tLimits.iSize;
    assert(!xrtMurmur3HasherCreate(&options, 0));
    xrtMurmur3OptionsInit(&options);
    options.tLimits.iMaxInputBytes = 3;
    h = xrtMurmur3HasherCreate(&options, 42);
    assert(h && xrtMurmur3HasherUpdate(h, "a", 1));
    copy = xrtMurmur3HasherClone(h);
    assert(copy && xrtMurmur3HasherUpdate(h, "bc", 2));
    assert(xrtMurmur3HasherDigest(h, digest));
    expect_hex(digest, "d6f7cfb39f08850d303d35422b711075");
    assert(!xrtMurmur3HasherDigest(h, NULL) && xrtMurmur3HasherDigest(h, digest));
    assert(!xrtMurmur3HasherUpdate(h, "d", 1) && xrtMurmur3HasherInputBytes(h) == 3);
    assert(!xrtMurmur3HasherClone(h) && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    assert(!xrtMurmur3HasherDigest(h, digest) && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    assert(!xrtMurmur3HasherUpdate(h, "", 0) && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    assert(xrtMurmur3HasherReset(h) && !h->Failed && xrtMurmur3HasherInputBytes(h) == 0);
    assert(xrtMurmur3HasherUpdate(copy, "bc", 2) && xrtMurmur3HasherDigest(copy, digest));
    expect_hex(digest, "d6f7cfb39f08850d303d35422b711075");
    xrtMurmur3HasherDestroy(copy);
    assert(!xrtMurmur3HasherUpdate(h, NULL, 1) && xrtMurmur3HasherReset(h));
    h->Options.tLimits.iMaxInputBytes = 0;
    h->State.total = UINT64_MAX - 1;
    assert(!xrtMurmur3HasherUpdate(h, "xx", 2) && h->Failed);
    assert(xrtMurmur3HasherReset(h) && xrtMurmur3HasherUpdate(h, "abc", 3));
    assert(xrtMurmur3HasherDigest(h, digest));
    expect_hex(digest, "d6f7cfb39f08850d303d35422b711075");
    xrtMurmur3HasherDestroy(h);
}

typedef struct progress_state { uint64 last, total; int calls, final, cancel; bool known; } progress_state;
static bool progress(const xrtprogress* p, ptr user)
{
    progress_state* s = user;
    assert(p->iInputBytes >= s->last && p->iTotalInputBytes == s->total && p->iOutputBytes == 0);
    assert(((p->iFlags & XRT_PROGRESS_TOTAL_KNOWN) != 0) == s->known);
    s->last = p->iInputBytes; ++s->calls;
    if (p->iFlags & XRT_PROGRESS_FINAL) ++s->final;
    assert(xrtMurmur3Hash32("foo", 3, 0, NULL) == UINT32_C(4138058784));
    if (s->cancel == 3) xrtSetErrorInfo(XERR_VALUE, "test.murmur3", 42, "callback marker");
    return s->cancel == 0 || (s->cancel == 2 && s->final == 0);
}
typedef struct reader_state { int calls, closed, fail; } reader_state;
static bool read_some(ptr user, ptr output, size_t requested, size_t* read)
{
    reader_state* s = user;
    assert(requested > 0);
    *read = 0;
    if (s->calls++ == 0 || (s->fail == 4 && s->calls == 2)) {
        *(uint8*)output = 'x'; *read = 1; return true;
    }
    if (s->fail == 4) return true;
    if (s->fail >= 2) xrtSetErrorInfo(s->fail == 3 ? XERR_MEMORY : XERR_IO, "test.reader", 42, "reader marker");
    return !s->fail;
}
static bool close_reader(ptr user) { ++((reader_state*)user)->closed; return true; }

/* 定位能力存在，不代表大小查询一定成功；失败时改用未知总长。 */
static bool seek_reader(ptr user, int64 offset, xseek origin, uint64* position)
{ (void)user; (void)offset; (void)origin; *position = 0; return true; }
static bool tell_reader(ptr user, uint64* position)
{ (void)user; *position = 0; return true; }
static bool size_reader(ptr user, uint64* size)
{
    (void)user; (void)size;
    xrtSetErrorInfo(XERR_IO, "test.reader", 42, "size unavailable");
    return false;
}

static bool file_digest(xreader* reader, const xrtresourcelimits* limits, bool wide,
    uint8 out[16], progress_state* state, xrtmurmur3error* error)
{
    if (wide) return xrtMurmur3File128(reader, limits, 42, out, progress, state, error);
    else {
        uint32 hash = xrtMurmur3File32(reader, limits, 42, progress, state, error);
        if (xrtGetError() != NULL) return false;
        memcpy(out, &hash, 4);
        return true;
    }
}
static void readers(void)
{
    static uint8 input[65538];
    uint8 digest[16], expected[16], sentinel[16];
    uint64 position;
    xreader* reader;
    xrtresourcelimits limits;
    xrtmurmur3error error;
    progress_state p;
    int mode, wide;
    memset(sentinel, 0xa5, 16);
    for (wide = 0; wide < 2; ++wide) {
        uint32 narrow = xrtMurmur3Hash32(input + 1, sizeof(input) - 1, 42, NULL);
        size_t size = wide ? 16 : 4;
        if (wide) assert(xrtMurmur3Hash128(input + 1, sizeof(input) - 1, 42, expected, NULL));
        else memcpy(expected, &narrow, 4);
        reader = xrtReaderFromMemory((xbytesview){input, sizeof(input)});
        assert(reader && xrtReaderSeek(reader, 1, XSEEK_START, &position));
        p = (progress_state){0, sizeof(input) - 1, 0, 0, 0, true};
        assert(file_digest(reader, NULL, wide, digest, &p, &error));
        assert(memcmp(digest, expected, size) == 0 && p.calls == 3 && p.final == 1);
        assert(p.last == sizeof(input) - 1 && xrtReaderDestroy(reader));
        for (mode = 0; mode < 5; ++mode) {
            reader = xrtReaderFromMemory((xbytesview){input, sizeof(input)});
            memcpy(digest, sentinel, 16);
            xrtResourceLimitsInit(&limits);
            p = (progress_state){0, sizeof(input), 0, 0, 0, true};
            if (mode == 0) limits.iMaxInputBytes = sizeof(input) - 1;
            if (mode == 1) ++limits.iVersion;
            if (mode >= 2) p.cancel = mode - 1;
            assert(!file_digest(reader, &limits, wide, digest, &p, &error) && memcmp(digest, sentinel, 16) == 0);
            if (mode <= 1) assert(p.calls == 0 && xrtReaderTell(reader, &position) && position == 0);
            if (mode == 4) assert(xrtErrorKind(xrtGetError()) == XERR_VALUE && strcmp(error.sMessage, "callback marker") == 0);
            assert(xrtReaderDestroy(reader));
        }
        reader = xrtReaderFromMemory((xbytesview){NULL, 0});
        p = (progress_state){0, 0, 0, 0, 0, true};
        assert(file_digest(reader, NULL, wide, digest, &p, NULL) && p.calls == 1 && p.final == 1);
        assert(xrtReaderDestroy(reader));
        assert(!file_digest(NULL, NULL, wide, digest, &p, NULL));
        for (mode = 0; mode < 5; ++mode) {
            reader_state s = {0, 0, mode};
            xreaderops ops = {read_some, NULL, NULL, NULL, close_reader};
            reader = xrtReaderCreate(&ops, &s);
            memcpy(digest, sentinel, 16);
            p = (progress_state){0, 0, 0, 0, 0, false};
            xrtResourceLimitsInit(&limits);
            /* 非定位 Reader 的已读量仍严格受预算限制。 */
            limits.iMaxInputBytes = mode == 4 ? 1 : 3;
            assert(file_digest(reader, &limits, wide, digest, &p, &error) == (mode == 0));
            assert(s.closed == 0);
            if (mode > 0) {
                assert(memcmp(digest, sentinel, 16) == 0);
                if (mode == 2 || mode == 3) assert(strcmp(xrtErrorMessage(xrtGetError()), "reader marker") == 0);
                if (mode == 3) assert(xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                if (mode == 4) {
                    assert(xrtErrorKind(xrtGetError()) == XERR_RANGE && error.iInputOffset == 1);
                    assert(p.calls == 1 && p.last == 1 && p.final == 0 && s.calls == 2);
                }
            } else assert(p.final == 1);
            assert(xrtReaderDestroy(reader) && s.closed == 1);
        }
        {
            reader_state s = {0, 0, 0};
            xreaderops ops = {read_some, seek_reader, tell_reader, size_reader, close_reader};
            reader = xrtReaderCreate(&ops, &s);
            p = (progress_state){0, 0, 0, 0, 0, false};
            assert(file_digest(reader, NULL, wide, digest, &p, NULL));
            assert(p.last == 1 && p.final == 1 && xrtGetError() == NULL && s.closed == 0);
            assert(xrtReaderDestroy(reader) && s.closed == 1);
        }
    }
    reader = xrtReaderFromMemory((xbytesview){input, sizeof(input)});
    assert(!xrtMurmur3File128(reader, NULL, 0, NULL, NULL, NULL, NULL));
    assert(xrtReaderDestroy(reader));
}

int main(void)
{
    vectors(); states(); readers();
    xrtClearError();
    puts("murmur3 core: 576 upstream vectors x 8 alignments, splits, states and readers passed");
    return 0;
}
