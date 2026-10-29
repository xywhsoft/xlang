/* 白盒测试包含核心和算法，避免为不可达溢出边界增加公开调试 API。 */
#include "../xrt_sha3.c"
#include "../third_party/sha3.c"
#include "vectors.h"
#include <assert.h>

static void expect_hex(const uint8* digest, size_t size, const char* expected)
{
    const char* digits = "0123456789abcdef";
    size_t i;
    assert(strlen(expected) == size * 2);
    for (i = 0; i < size; ++i) {
        assert(expected[2 * i] == digits[digest[i] >> 4]);
        assert(expected[2 * i + 1] == digits[digest[i] & 15]);
    }
}
static void vectors(void)
{
    static uint8 data[65545], million[1000000];
    uint8 digest[1026];
    size_t i, offset, consumed, count, split;
    for (offset = 0; offset < 8; ++offset) {
        for (i = 0; i < 65537; ++i) data[offset + i] = (uint8)(i * 37 + 11);
        for (i = 0; i < sizeof(sha3_vectors) / sizeof(sha3_vectors[0]); ++i) {
            size_t size = sha3_vectors[i].size, length = sha3_vectors[i].output;
            int bits = sha3_vectors[i].bits;
            bool shake = sha3_vectors[i].shake != 0;
            xrtsha3hasher* h = shake ? xrtSha3ShakeHasherCreate(NULL, bits) : xrtSha3HasherCreate(NULL, bits);
            assert(h);
            memset(digest, 0xa5, sizeof(digest));
            assert(shake ? xrtShakeHash(data + offset, size, bits, digest + 1, length, NULL) :
                xrtSha3Hash(data + offset, size, bits, digest + 1, length, NULL));
            expect_hex(digest + 1, length, sha3_vectors[i].hex);
            assert(digest[0] == 0xa5 && digest[length + 1] == 0xa5);
            for (consumed = 0; consumed < size; consumed += count) {
                count = consumed % 193 + 1;
                if (count > size - consumed) count = size - consumed;
                assert(xrtSha3HasherUpdate(h, data + offset + consumed, count));
            }
            assert(xrtSha3HasherInputBytes(h) == size);
            assert(xrtSha3HasherOutBytes(h) == (shake ? 0 : bits / 8));
            assert(xrtSha3HasherDigest(h, digest + 1, length));
            expect_hex(digest + 1, length, sha3_vectors[i].hex);
            assert(xrtSha3HasherDigest(h, digest + 1, length));
            expect_hex(digest + 1, length, sha3_vectors[i].hex);
            assert(xrtSha3HasherReset(h) && xrtSha3HasherUpdate(h, data + offset, size));
            assert(xrtSha3HasherDigest(h, digest + 1, length));
            expect_hex(digest + 1, length, sha3_vectors[i].hex);
            assert(digest[0] == 0xa5 && digest[length + 1] == 0xa5);
            xrtSha3HasherDestroy(h);
        }
    }
    /* 每个短消息及 rate 边界的所有切点，均与独立已知答案比较。 */
    for (i = 0; i < sizeof(sha3_vectors) / sizeof(sha3_vectors[0]); ++i) {
        size_t size = sha3_vectors[i].size, length = sha3_vectors[i].output;
        if (size > 337) continue;
        for (split = 0; split <= size; ++split) {
            sha3_ctx h;
            if (sha3_vectors[i].shake) shake_init(&h, sha3_vectors[i].bits);
            else sha3_init(&h, sha3_vectors[i].bits);
            sha3_update(&h, data + 7, split);
            sha3_update(&h, data + 7 + split, size - split);
            sha3_final(&h, digest, length);
            expect_hex(digest, length, sha3_vectors[i].hex);
        }
    }
    memset(million, 'a', sizeof(million));
    for (i = 0; i < sizeof(sha3_known) / sizeof(sha3_known[0]); ++i) {
        const char utf8[] = "\xe4\xbd\xa0\xe5\xa5\xbd\0\xe4\xb8\x96\xe7\x95\x8c";
        const void* input = strcmp(sha3_known[i].name, "abc") == 0 ? (const void*)"abc" :
            strcmp(sha3_known[i].name, "utf8") == 0 ? (const void*)utf8 : (const void*)million;
        size_t size = input == million ? sizeof(million) : input == utf8 ? sizeof(utf8) - 1 : 3;
        size_t length = (size_t)sha3_known[i].bits / 8;
        assert(xrtSha3Hash(input, size, sha3_known[i].bits, digest, length, NULL));
        expect_hex(digest, length, sha3_known[i].hex);
    }
}
static void states(void)
{
    xrtsha3options options;
    xrtsha3error error;
    xrtsha3hasher *h, *copy;
    uint8 digest[64], sentinel[64];
    int bits;
    memset(sentinel, 0xa5, sizeof(sentinel)); memcpy(digest, sentinel, sizeof(digest));
    for (bits = -1; bits < 520; ++bits) {
        if (bits == 224 || bits == 256 || bits == 384 || bits == 512) continue;
        assert(!xrtSha3DigestSize(bits) && !xrtSha3HasherCreate(NULL, bits));
        assert(!xrtSha3Hash("abc", 3, bits, digest, 32, &error) && error.iCode == XRT_SHA3_ERROR_ARGUMENT);
        assert(!memcmp(digest, sentinel, sizeof(digest)));
    }
    assert(!xrtSha3ShakeHasherCreate(NULL, 224) && !xrtShakeHash(NULL, 0, 224, NULL, 0, &error));
    assert(!xrtSha3Hash(NULL, 1, 256, digest, 32, &error));
    assert(!xrtSha3Hash(NULL, 0, 256, digest, 31, &error));
    assert(!xrtSha3Hash(NULL, 0, 256, digest, 33, &error));
    assert(!xrtSha3Hash(NULL, 0, 256, NULL, 32, &error));
    assert(!xrtShakeHash(NULL, 1, 128, digest, 32, &error));
    assert(!xrtShakeHash(NULL, 0, 128, NULL, 1, &error));
    assert(xrtShakeHash(NULL, 0, 128, NULL, 0, &error) && !xrtGetError() && error.iCode == 0);
    assert(!xrtSha3HasherUpdate(NULL, NULL, 0) && !xrtSha3HasherReset(NULL));
    assert(!xrtSha3HasherDigest(NULL, digest, 32) && !xrtSha3HasherClone(NULL));
    assert(!xrtSha3HasherInputBytes(NULL) && !xrtSha3HasherOutBytes(NULL) && !xrtSha3HasherError(NULL));
    xrtSha3HasherDestroy(NULL);
    xrtSha3OptionsInit(&options); ++options.iVersion;
    assert(!xrtSha3HasherCreate(&options, 256));
    xrtSha3OptionsInit(&options); --options.iSize;
    assert(!xrtSha3HasherCreate(&options, 256));
    xrtSha3OptionsInit(&options); ++options.tLimits.iVersion;
    assert(!xrtSha3HasherCreate(&options, 256));
    xrtSha3OptionsInit(&options); --options.tLimits.iSize;
    assert(!xrtSha3HasherCreate(&options, 256));
    xrtSha3OptionsInit(&options); options.tLimits.iMaxInputBytes = 3;
    h = xrtSha3HasherCreate(&options, 256);
    assert(h && xrtSha3HasherUpdate(h, "a", 1));
    copy = xrtSha3HasherClone(h);
    assert(copy && xrtSha3HasherUpdate(copy, "bc", 2));
    assert(xrtSha3HasherDigest(copy, digest, 32));
    expect_hex(digest, 32, "3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532");
    assert(!xrtSha3HasherDigest(copy, digest, 64) && !copy->Failed);
    assert(!xrtSha3HasherDigest(copy, NULL, 32) && !copy->Failed);
    assert(xrtSha3HasherDigest(copy, digest, 32));
    assert(!xrtSha3HasherUpdate(copy, "x", 1) && copy->Failed);
    assert(!xrtSha3HasherClone(copy) && !xrtSha3HasherDigest(copy, digest, 32));
    assert(xrtErrorKind(xrtGetError()) == XERR_RANGE && !xrtSha3HasherUpdate(copy, NULL, 0));
    assert(xrtSha3HasherReset(copy) && xrtSha3HasherError(copy)->iCode == 0);
    assert(xrtSha3HasherInputBytes(h) == 1 && xrtSha3HasherInputBytes(copy) == 0);
    assert(!xrtSha3HasherUpdate(copy, NULL, 1) && xrtSha3HasherReset(copy));
    copy->Options.tLimits.iMaxInputBytes = 0; copy->InputBytes = UINT64_MAX;
    assert(!xrtSha3HasherUpdate(copy, "x", 1) && copy->InputBytes == UINT64_MAX);
    assert(xrtSha3HasherReset(copy));
    xrtSha3HasherDestroy(h); xrtSha3HasherDestroy(copy);
    options.tLimits.iMaxOutputBytes = 31;
    h = xrtSha3HasherCreate(&options, 256);
    assert(h && !xrtSha3HasherCheckOutput(h, 32) && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    assert(xrtSha3HasherUpdate(h, "abc", 3));
    xrtSha3HasherDestroy(h);
    h = xrtSha3ShakeHasherCreate(&options, 256);
    assert(h && xrtSha3HasherDigest(h, NULL, 0) && !xrtSha3HasherCheckOutput(h, 32));
    assert(xrtSha3HasherDigest(h, digest, 31));
    xrtSha3HasherDestroy(h);
}
typedef struct progress_state { uint64 last, total; int calls, final, cancel; bool known; } progress_state;
static bool progress(const xrtprogress* p, ptr user)
{
    progress_state* s = user;
    uint8 digest[32];
    assert(p->iInputBytes >= s->last && p->iTotalInputBytes == s->total && p->iOutputBytes == 0);
    assert(((p->iFlags & XRT_PROGRESS_TOTAL_KNOWN) != 0) == s->known);
    s->last = p->iInputBytes; ++s->calls;
    if (p->iFlags & XRT_PROGRESS_FINAL) ++s->final;
    assert(xrtSha3Hash("abc", 3, 256, digest, 32, NULL));
    if (s->cancel == 3) xrtSetErrorInfo(XERR_VALUE, "test.sha3", 42, "callback marker");
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

static void readers(void)
{
    static uint8 input[65538];
    uint8 digest[32], expected[32], sentinel[32];
    uint64 position;
    xreader* reader;
    xrtresourcelimits limits;
    xrtsha3error error;
    progress_state p;
    int mode;
    memset(sentinel, 0xa5, sizeof(sentinel));
    assert(xrtSha3Hash(input + 1, sizeof(input) - 1, 256, expected, 32, NULL));
    reader = xrtReaderFromMemory((xbytesview){input, sizeof(input)});
    assert(reader && xrtReaderSeek(reader, 1, XSEEK_START, &position));
    p = (progress_state){0, sizeof(input) - 1, 0, 0, 0, true};
    assert(xrtSha3File(reader, NULL, 256, digest, 32, progress, &p, &error));
    assert(!memcmp(digest, expected, 32) && p.calls == 3 && p.final == 1 && p.last == sizeof(input) - 1);
    assert(xrtReaderDestroy(reader));
    for (mode = 0; mode < 6; ++mode) {
        reader = xrtReaderFromMemory((xbytesview){input, sizeof(input)});
        memcpy(digest, sentinel, 32); xrtResourceLimitsInit(&limits);
        p = (progress_state){0, sizeof(input), 0, 0, 0, true};
        if (mode == 0) limits.iMaxInputBytes = sizeof(input) - 1;
        if (mode == 1) ++limits.iVersion;
        if (mode >= 2 && mode <= 4) p.cancel = mode - 1;
        if (mode == 5) limits.iMaxOutputBytes = 31;
        assert(!xrtSha3File(reader, &limits, 256, digest, 32, progress, &p, &error));
        assert(!memcmp(digest, sentinel, 32));
        if (mode <= 1 || mode == 5) assert(!p.calls && xrtReaderTell(reader, &position) && position == 0);
        if (mode == 4) assert(xrtErrorKind(xrtGetError()) == XERR_VALUE && !strcmp(error.sMessage, "callback marker"));
        assert(xrtReaderDestroy(reader));
    }
    reader = xrtReaderFromMemory((xbytesview){NULL, 0});
    p = (progress_state){0, 0, 0, 0, 0, true};
    assert(xrtSha3File(reader, NULL, 256, digest, 32, progress, &p, NULL) && p.calls == 1 && p.final == 1);
    assert(xrtReaderDestroy(reader));
    assert(!xrtSha3File(NULL, NULL, 256, digest, 32, NULL, NULL, NULL));
    for (mode = 0; mode < 5; ++mode) {
        reader_state s = {0, 0, mode};
        xreaderops ops = {read_some, NULL, NULL, NULL, close_reader};
        reader = xrtReaderCreate(&ops, &s);
        memcpy(digest, sentinel, 32); p = (progress_state){0, 0, 0, 0, 0, false};
        xrtResourceLimitsInit(&limits); limits.iMaxInputBytes = mode == 4 ? 1 : 3;
        assert(xrtSha3File(reader, &limits, 256, digest, 32, progress, &p, &error) == (mode == 0));
        assert(!s.closed);
        if (mode > 0) {
            assert(!memcmp(digest, sentinel, 32));
            if (mode == 2 || mode == 3) assert(!strcmp(xrtErrorMessage(xrtGetError()), "reader marker"));
            if (mode == 3) assert(xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            if (mode == 4) assert(xrtErrorKind(xrtGetError()) == XERR_RANGE && error.iInputOffset == 1 && p.calls == 1);
        } else assert(p.final == 1);
        assert(xrtReaderDestroy(reader) && s.closed == 1);
    }
    {
        reader_state s = {0, 0, 0};
        xreaderops ops = {read_some, seek_reader, tell_reader, size_reader, close_reader};
        reader = xrtReaderCreate(&ops, &s); p = (progress_state){0, 0, 0, 0, 0, false};
        assert(xrtSha3File(reader, NULL, 256, digest, 32, progress, &p, NULL));
        assert(p.last == 1 && p.final == 1 && !xrtGetError() && !s.closed);
        assert(xrtReaderDestroy(reader) && s.closed == 1);
    }
}
int main(void)
{
    vectors(); states(); readers(); xrtClearError();
    puts("sha3 core: 546 OpenSSL vectors x 8 alignments, 12 known answers, splits, states and readers passed");
    return 0;
}
