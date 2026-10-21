/* 白盒边界可检查计数溢出，不为测试扩张公开 API；参考实现仅编入测试。 */
#include "../xrt_siphash.c"
#include "../third_party/siphash.c"
#include "reference/siphash.c"
#include "reference/vectors.h"
#include <assert.h>

static uint64 numeric(const uint8 bytes[8])
{
    uint64 value = 0;
    unsigned i;
    for (i = 0; i < 8; ++i) value |= (uint64)bytes[i] << (i * 8);
    return value;
}
static void check(const uint8* data, size_t size, const uint8 key[16], bool allSplits)
{
    uint8 expected64[8], expected128[16], digest[18];
    xrtsiphasher* h;
    size_t consumed, count, split;
    assert(siphash(data, size, key, expected64, 8) == 0);
    assert(siphash(data, size, key, expected128, 16) == 0);
    assert(xrtSipHash64(data, size, key, NULL) == numeric(expected64) && !xrtGetError());
    memset(digest, 0xa5, sizeof(digest));
    assert(xrtSipHash128(data, size, key, digest + 1, NULL));
    assert(!memcmp(digest + 1, expected128, 16) && digest[0] == 0xa5 && digest[17] == 0xa5);
    h = xrtSipHasherCreate(NULL, key); assert(h);
    for (consumed = 0; consumed < size; consumed += count) {
        count = consumed % 193 + 1;
        if (count > size - consumed) count = size - consumed;
        assert(xrtSipHasherUpdate(h, data + consumed, count));
    }
    assert(xrtSipHasherInputBytes(h) == size);
    assert(xrtSipHasherDigest64(h) == numeric(expected64));
    assert(xrtSipHasherDigest128(h, digest + 1) && !memcmp(digest + 1, expected128, 16));
    assert(xrtSipHasherDigest64(h) == numeric(expected64));
    assert(xrtSipHasherUpdate(h, NULL, 0));
    assert(xrtSipHasherDigest128(h, digest + 1) && !memcmp(digest + 1, expected128, 16));
    if (allSplits) for (split = 0; split <= size; ++split) {
        assert(xrtSipHasherReset(h) && xrtSipHasherUpdate(h, data, split));
        assert(xrtSipHasherUpdate(h, data + split, size - split));
        assert(xrtSipHasherDigest64(h) == numeric(expected64));
        assert(xrtSipHasherDigest128(h, digest + 1) && !memcmp(digest + 1, expected128, 16));
    }
    xrtSipHasherDestroy(h);
}
static void vectors(void)
{
    static uint8 data[65545], million[1000000];
    const size_t lengths[] = {64,65,127,128,129,255,256,257,511,512,513,4095,4096,4097,65535,65536,65537};
    const uint8 utf8[] = "\xe4\xbd\xa0\xe5\xa5\xbd\0\xe4\xb8\x96\xe7\x95\x8c";
    uint8 key[16], out[16];
    size_t i, mode, offset;
    for (i = 0; i < 16; ++i) key[i] = (uint8)i;
    for (i = 0; i < 64; ++i) data[i] = (uint8)i;
    for (i = 0; i < 64; ++i) {
        assert(xrtSipHash64(data, i, key, NULL) == numeric(vectors_sip64[i]));
        assert(xrtSipHash128(data, i, key, out, NULL) && !memcmp(out, vectors_sip128[i], 16));
        assert(siphash(data, i, key, out, 8) == 0 && !memcmp(out, vectors_sip64[i], 8));
        assert(siphash(data, i, key, out, 16) == 0 && !memcmp(out, vectors_sip128[i], 16));
    }
    for (mode = 0; mode < 4; ++mode) {
        for (i = 0; i < 16; ++i) key[i] = mode == 0 ? 0 : mode == 1 ? 255 :
            mode == 2 ? (uint8)i : (uint8)(i * 37 + 128);
        for (offset = 0; offset < 8; ++offset) {
            for (i = 0; i < 65537; ++i) data[offset + i] = (uint8)(i * 37 + 11);
            for (i = 0; i < 64; ++i) check(data + offset, i, key, true);
            for (i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i)
                check(data + offset, lengths[i], key, lengths[i] <= 129);
        }
        check(utf8, sizeof(utf8) - 1, key, true);
        memset(million, 'a', sizeof(million)); check(million, sizeof(million), key, false);
    }
}
static void states(void)
{
    uint8 key[16] = {0}, saved[16] = {0}, out[16], guard[16];
    xrtsiphashoptions options;
    xrtsiphasherror error;
    xrtsiphasher *h, *copy;
    memset(out, 0xa5, sizeof(out)); memcpy(guard, out, sizeof(out));
    assert(!xrtSipHash128(NULL, 1, key, out, &error) && error.iCode == XRT_SIPHASH_ERROR_ARGUMENT);
    assert(!memcmp(out, guard, 16));
    assert(!xrtSipHash128(NULL, 0, NULL, out, NULL) && !xrtSipHash128(NULL, 0, key, NULL, NULL));
    assert(!xrtSipHash64(NULL, 1, key, &error) && error.iCode != 0);
    assert(!xrtSipHash64(NULL, 0, NULL, &error) && error.iCode != 0);
    assert(xrtSipHash128(NULL, 0, key, out, &error) && !error.iCode);
    assert(!xrtSipHasherCreate(NULL, NULL));
    xrtSipHashOptionsInit(&options); ++options.iSize; assert(!xrtSipHasherCreate(&options, key));
    xrtSipHashOptionsInit(&options); ++options.iVersion; assert(!xrtSipHasherCreate(&options, key));
    xrtSipHashOptionsInit(&options); ++options.tLimits.iSize; assert(!xrtSipHasherCreate(&options, key));
    xrtSipHashOptionsInit(&options); ++options.tLimits.iVersion; assert(!xrtSipHasherCreate(&options, key));
    assert(!xrtSipHasherUpdate(NULL, NULL, 0) && !xrtSipHasherReset(NULL));
    assert(!xrtSipHasherDigest64(NULL) && !xrtSipHasherDigest128(NULL, out));
    assert(!xrtSipHasherClone(NULL) && !xrtSipHasherInputBytes(NULL) && !xrtSipHasherError(NULL));
    xrtSipHasherDestroy(NULL);
    xrtSipHashOptionsInit(NULL); xrtSipHashErrorInit(NULL);
    xrtSipHashOptionsInit(&options); options.tLimits.iMaxInputBytes = 3;
    h = xrtSipHasherCreate(&options, key); assert(h);
    memset(key, 255, 16); /* 生命周期由对象自己的密钥副本保证。 */
    assert(xrtSipHasherUpdate(h, "a", 1));
    copy = xrtSipHasherClone(h); assert(copy && xrtSipHasherUpdate(copy, "bc", 2));
    assert(xrtSipHasherDigest64(copy) == xrtSipHash64("abc", 3, saved, NULL));
    assert(xrtSipHasherInputBytes(h) == 1 && xrtSipHasherInputBytes(copy) == 3);
    assert(!xrtSipHasherDigest128(copy, NULL) && !copy->Failed);
    assert(!xrtSipHasherUpdate(copy, "x", 1) && copy->Failed);
    assert(!xrtSipHasherClone(copy) && !xrtSipHasherDigest64(copy));
    assert(xrtErrorKind(xrtGetError()) == XERR_RANGE);
    assert(!xrtSipHasherDigest128(copy, out));
    assert(xrtSipHasherReset(copy) && !xrtSipHasherError(copy)->iCode && !xrtSipHasherInputBytes(copy));
    assert(!xrtSipHasherUpdate(copy, NULL, 1) && xrtSipHasherReset(copy));
    assert(xrtSipHasherDigest64(copy) == xrtSipHash64(NULL, 0, saved, NULL));
    copy->Options.tLimits.iMaxInputBytes = 0; copy->InputBytes = UINT64_MAX;
    assert(!xrtSipHasherUpdate(copy, "a", 1) && copy->InputBytes == UINT64_MAX);
    assert(xrtSipHasherReset(copy));
    copy->Options.tLimits.iMaxOutputBytes = 7;
    assert(!xrtSipHasherDigest64(copy) && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    copy->Options.tLimits.iMaxOutputBytes = 8;
    assert(xrtSipHasherDigest64(copy) == xrtSipHash64(NULL, 0, saved, NULL));
    assert(!xrtSipHasherDigest128(copy, out) && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    assert(!copy->Failed && xrtSipHasherUpdate(copy, "x", 1));
    xrtSipHasherDestroy(h); xrtSipHasherDestroy(copy);
}
typedef struct progress_state { uint64 last, total; int calls, final, cancel; bool known; } progress_state;
static bool progress(const xrtprogress* p, ptr user)
{
    progress_state* s = user;
    uint8 key[16] = {0};
    assert(p->iInputBytes >= s->last && p->iTotalInputBytes == s->total && p->iOutputBytes == 0);
    assert(((p->iFlags & XRT_PROGRESS_TOTAL_KNOWN) != 0) == s->known);
    s->last = p->iInputBytes; ++s->calls;
    if (p->iFlags & XRT_PROGRESS_FINAL) ++s->final;
    (void)xrtSipHash64("abc", 3, key, NULL); assert(!xrtGetError());
    if (s->cancel == 3) xrtSetErrorInfo(XERR_VALUE, "test.siphash", 42, "callback marker");
    return s->cancel == 0 || (s->cancel == 2 && s->final == 0);
}
typedef struct reader_state { int calls, closed, fail; } reader_state;
static bool read_some(ptr user, ptr output, size_t requested, size_t* read)
{
    reader_state* s = user;
    assert(requested > 0); *read = 0;
    if (s->calls++ == 0 || (s->fail == 4 && s->calls == 2)) { *(uint8*)output = 'x'; *read = 1; return true; }
    if (s->fail == 4) return true;
    if (s->fail >= 2) xrtSetErrorInfo(s->fail == 3 ? XERR_MEMORY : XERR_IO, "test.reader", 42, "reader marker");
    return !s->fail;
}
static bool close_reader(ptr user) { ++((reader_state*)user)->closed; return true; }
static bool seek_reader(ptr user, int64 offset, xseek origin, uint64* position)
{ (void)user; (void)offset; (void)origin; *position = 0; return true; }
static bool tell_reader(ptr user, uint64* position) { (void)user; *position = 0; return true; }
static bool size_reader(ptr user, uint64* size)
{ (void)user; (void)size; xrtSetErrorInfo(XERR_IO, "test.reader", 42, "size unavailable"); return false; }
static bool file_hash(xreader* r, const xrtresourcelimits* limits, const uint8 key[16],
    uint8 out[16], bool wide, progress_state* p, xrtsiphasherror* error)
{
    uint64 value;
    if (wide) return xrtSipHashFile128(r, limits, key, out, progress, p, error);
    value = xrtSipHashFile64(r, limits, key, progress, p, error);
    if (xrtGetError()) return false;
    sip_store(out, value); return true;
}
static void readers(void)
{
    static uint8 input[65538];
    uint8 key[16] = {0}, out[16], expected[16], guard[16];
    uint64 position;
    xreader* r;
    xrtresourcelimits limits;
    xrtsiphasherror error;
    progress_state p;
    int mode, wide;
    memset(guard, 0xa5, sizeof(guard));
    for (wide = 0; wide < 2; ++wide) {
        assert(siphash(input + 1, sizeof(input) - 1, key, expected, wide ? 16 : 8) == 0);
        r = xrtReaderFromMemory((xbytesview){input, sizeof(input)});
        assert(r && xrtReaderSeek(r, 1, XSEEK_START, &position));
        p = (progress_state){0, sizeof(input) - 1, 0, 0, 0, true};
        assert(file_hash(r, NULL, key, out, wide != 0, &p, &error));
        assert(!memcmp(out, expected, wide ? 16 : 8) && p.calls == 3 && p.final == 1);
        assert(xrtReaderDestroy(r));
        for (mode = 0; mode < 6; ++mode) {
            r = xrtReaderFromMemory((xbytesview){input, sizeof(input)});
            memcpy(out, guard, 16); xrtResourceLimitsInit(&limits);
            p = (progress_state){0, sizeof(input), 0, 0, 0, true};
            if (mode == 0) limits.iMaxInputBytes = sizeof(input) - 1;
            if (mode == 1) ++limits.iVersion;
            if (mode >= 2 && mode <= 4) p.cancel = mode - 1;
            if (mode == 5) limits.iMaxOutputBytes = 7;
            assert(!file_hash(r, &limits, key, out, wide != 0, &p, &error));
            assert(!memcmp(out, guard, 16));
            if (mode <= 1 || mode == 5) assert(!p.calls && xrtReaderTell(r, &position) && position == 0);
            if (mode == 4) assert(xrtErrorKind(xrtGetError()) == XERR_VALUE && !strcmp(error.sMessage, "callback marker"));
            assert(xrtReaderDestroy(r));
        }
        r = xrtReaderFromMemory((xbytesview){NULL, 0}); p = (progress_state){0, 0, 0, 0, 0, true};
        assert(file_hash(r, NULL, key, out, wide != 0, &p, NULL) && p.calls == 1 && p.final == 1);
        assert(xrtReaderDestroy(r));
        assert(!file_hash(NULL, NULL, key, out, wide != 0, &p, NULL));
        for (mode = 0; mode < 5; ++mode) {
            reader_state s = {0, 0, mode};
            xreaderops ops = {read_some, NULL, NULL, NULL, close_reader};
            r = xrtReaderCreate(&ops, &s); memcpy(out, guard, 16);
            p = (progress_state){0, 0, 0, 0, 0, false};
            xrtResourceLimitsInit(&limits); limits.iMaxInputBytes = mode == 4 ? 1 : 3;
            assert(file_hash(r, &limits, key, out, wide != 0, &p, &error) == (mode == 0));
            assert(!s.closed);
            if (mode > 0) {
                assert(!memcmp(out, guard, 16));
                if (mode == 2 || mode == 3) assert(!strcmp(xrtErrorMessage(xrtGetError()), "reader marker"));
                if (mode == 3) assert(xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                if (mode == 4) assert(xrtErrorKind(xrtGetError()) == XERR_RANGE && error.iInputOffset == 1);
            } else assert(p.final == 1);
            assert(xrtReaderDestroy(r) && s.closed == 1);
        }
        {
            reader_state s = {0, 0, 0};
            xreaderops ops = {read_some, seek_reader, tell_reader, size_reader, close_reader};
            r = xrtReaderCreate(&ops, &s); p = (progress_state){0, 0, 0, 0, 0, false};
            assert(file_hash(r, NULL, key, out, wide != 0, &p, NULL) && p.last == 1 && p.final == 1);
            assert(!xrtGetError() && !s.closed && xrtReaderDestroy(r));
        }
    }
}
int main(void)
{
    vectors(); states(); readers(); xrtClearError();
    puts("siphash core: 128 official answers, 4 keys x 8 alignments x 81 lengths, splits, states and readers passed");
    return 0;
}
