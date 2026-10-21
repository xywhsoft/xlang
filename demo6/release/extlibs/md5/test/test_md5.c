/* 核心白盒入口不另链接 xrt_md5.c；向量来自独立 .NET MD5 实现。 */
#include "../xrt_md5.c"
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
    size_t i, offset, count;
    static const struct { const char* text; const char* hex; } fixed[] = {
        {"", "d41d8cd98f00b204e9800998ecf8427e"},
        {"a", "0cc175b9c0f1b6a831c399e269772661"},
        {"abc", "900150983cd24fb0d6963f7d28e17f72"},
        {"message digest", "f96b697d7cb7938d525a2f31aaf161d0"},
        {"abcdefghijklmnopqrstuvwxyz", "c3fcd3d76192e4007dfb496cca67e13b"},
        {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", "d174ab98d277d9f5a5611c2c9f419d9f"},
        {"12345678901234567890123456789012345678901234567890123456789012345678901234567890", "57edf4a22be3c955ac49da2e2107b67a"}
    };
    for (i = 0; i < sizeof(fixed) / sizeof(fixed[0]); ++i) {
        assert(xrtMd5Hash(fixed[i].text, strlen(fixed[i].text), digest, NULL));
        expect_hex(digest, fixed[i].hex);
    }
    for (offset = 0; offset < 8; ++offset) {
        for (i = 0; i < 65537; ++i) data[offset + i] = (uint8)(i * 37 + 11);
        for (i = 0; i < sizeof(md5_vectors) / sizeof(md5_vectors[0]); ++i) {
            size_t consumed = 0, size = md5_vectors[i].size;
            xrtmd5hasher* h = xrtMd5HasherCreate(NULL);
            assert(h && xrtMd5Hash(data + offset, size, digest, NULL));
            expect_hex(digest, md5_vectors[i].hex);
            while (consumed < size) {
                count = consumed % 97 + 1;
                if (count > size - consumed) count = size - consumed;
                assert(xrtMd5HasherUpdate(h, data + offset + consumed, count));
                consumed += count;
            }
            assert(xrtMd5HasherInputBytes(h) == size);
            assert(xrtMd5HasherDigest(h, digest));
            expect_hex(digest, md5_vectors[i].hex);
            assert(xrtMd5HasherDigest(h, digest) && xrtMd5HasherUpdate(h, NULL, 0));
            expect_hex(digest, md5_vectors[i].hex);
            assert(xrtMd5HasherReset(h) && xrtMd5HasherUpdate(h, data + offset, size));
            assert(xrtMd5HasherDigest(h, digest));
            expect_hex(digest, md5_vectors[i].hex);
            xrtMd5HasherDestroy(h);
        }
    }
    memset(data, 'a', 1000);
    {
        xrtmd5hasher* h = xrtMd5HasherCreate(NULL);
        assert(h);
        for (i = 0; i < 1000; ++i) assert(xrtMd5HasherUpdate(h, data, 1000));
        assert(xrtMd5HasherDigest(h, digest));
        expect_hex(digest, "7707d6ae4e027c70eea2a935c2296f21");
        xrtMd5HasherDestroy(h);
    }
}

static void states(void)
{
    xrtmd5options options;
    xrtmd5error error;
    xrtmd5hasher* h;
    uint8 digest[16], sentinel[16];
    memset(sentinel, 0xa5, sizeof(sentinel));
    memcpy(digest, sentinel, sizeof(digest));
    assert(!xrtMd5Hash(NULL, 1, digest, &error) && memcmp(digest, sentinel, 16) == 0);
    assert(error.iCode == XRT_MD5_ERROR_ARGUMENT && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(!xrtMd5Hash(NULL, 0, NULL, NULL));
    assert(xrtMd5Hash(NULL, 0, digest, NULL));
    expect_hex(digest, "d41d8cd98f00b204e9800998ecf8427e");
    assert(!xrtMd5HasherUpdate(NULL, NULL, 0));
    assert(!xrtMd5HasherDigest(NULL, digest) && !xrtMd5HasherReset(NULL));
    assert(xrtMd5HasherInputBytes(NULL) == 0 && xrtMd5HasherError(NULL) == NULL);
    xrtMd5HasherDestroy(NULL);
    xrtMd5OptionsInit(&options);
    ++options.iVersion;
    assert(!xrtMd5HasherCreate(&options));
    --options.iVersion;
    --options.iSize;
    assert(!xrtMd5HasherCreate(&options));
    ++options.iSize;
    ++options.tLimits.iVersion;
    assert(!xrtMd5HasherCreate(&options));
    xrtMd5OptionsInit(&options);
    --options.tLimits.iSize;
    assert(!xrtMd5HasherCreate(&options));
    xrtMd5OptionsInit(&options);
    options.tLimits.iMaxInputBytes = 3;
    h = xrtMd5HasherCreate(&options);
    assert(h && xrtMd5HasherUpdate(h, "abc", 3));
    assert(!xrtMd5HasherDigest(h, NULL));
    assert(xrtMd5HasherDigest(h, digest));
    expect_hex(digest, "900150983cd24fb0d6963f7d28e17f72");
    assert(!xrtMd5HasherUpdate(h, "d", 1) && xrtMd5HasherInputBytes(h) == 3);
    assert(!xrtMd5HasherDigest(h, digest) && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    assert(!xrtMd5HasherUpdate(h, "", 0) && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    assert(xrtMd5HasherReset(h) && !h->Failed && xrtMd5HasherInputBytes(h) == 0);
    assert(!xrtMd5HasherUpdate(h, NULL, 1) && xrtMd5HasherReset(h));
    h->Options.tLimits.iMaxInputBytes = 0;
    h->State.Size = UINT64_MAX >> 3;
    h->State.BufferSize = (uint32)(h->State.Size % 64);
    assert(!xrtMd5HasherUpdate(h, "x", 1) && h->Failed);
    assert(xrtMd5HasherReset(h) && xrtMd5HasherUpdate(h, "a", 1));
    assert(xrtMd5HasherDigest(h, digest));
    expect_hex(digest, "0cc175b9c0f1b6a831c399e269772661");
    xrtMd5HasherDestroy(h);
}

typedef struct progress_state { uint64 last, total; int calls, final, cancel; bool known; } progress_state;
static bool progress(const xrtprogress* p, ptr user)
{
    progress_state* s = user;
    uint8 nested[16];
    assert(p->iInputBytes >= s->last && p->iTotalInputBytes == s->total && p->iOutputBytes == 0);
    assert(((p->iFlags & XRT_PROGRESS_TOTAL_KNOWN) != 0) == s->known);
    s->last = p->iInputBytes;
    ++s->calls;
    if (p->iFlags & XRT_PROGRESS_FINAL) ++s->final;
    assert(xrtMd5Hash("nested", 6, nested, NULL));
    if (s->cancel == 3) xrtSetErrorInfo(XERR_VALUE, "test.md5", 42, "callback marker");
    return s->cancel == 0 || (s->cancel == 2 && s->final == 0);
}
typedef struct reader_state { int calls, closed, fail; } reader_state;
static bool read_some(ptr user, ptr output, size_t requested, size_t* read)
{
    reader_state* s = user;
    assert(requested > 0);
    *read = 0;
    if (s->calls++ == 0) { *(uint8*)output = 'x'; *read = 1; return true; }
    if (s->fail >= 2) xrtSetErrorInfo(s->fail == 3 ? XERR_MEMORY : XERR_IO, "test.reader", 42, "reader marker");
    return !s->fail;
}
static bool close_reader(ptr user) { ++((reader_state*)user)->closed; return true; }

static void readers(void)
{
    static uint8 input[65538];
    uint8 digest[16], expected[16], sentinel[16];
    uint64 position;
    xreader* reader;
    xrtresourcelimits limits;
    xrtmd5error error;
    progress_state p;
    int mode;
    memset(sentinel, 0xa5, sizeof(sentinel));
    assert(xrtMd5Hash(input + 1, sizeof(input) - 1, expected, NULL));
    reader = xrtReaderFromMemory((xbytesview){input, sizeof(input)});
    assert(reader && xrtReaderSeek(reader, 1, XSEEK_START, &position));
    p = (progress_state){0, sizeof(input) - 1, 0, 0, 0, true};
    assert(xrtMd5File(reader, NULL, digest, progress, &p, &error));
    assert(memcmp(digest, expected, 16) == 0 && p.calls == 3 && p.final == 1 && p.last == sizeof(input) - 1);
    assert(xrtReaderDestroy(reader));
    for (mode = 0; mode < 5; ++mode) {
        reader = xrtReaderFromMemory((xbytesview){input, sizeof(input)});
        memcpy(digest, sentinel, 16);
        xrtResourceLimitsInit(&limits);
        p = (progress_state){0, sizeof(input), 0, 0, 0, true};
        if (mode == 0) limits.iMaxInputBytes = sizeof(input) - 1;
        if (mode == 1) ++limits.iVersion;
        if (mode >= 2) p.cancel = mode - 1;
        assert(!xrtMd5File(reader, &limits, digest, progress, &p, &error) && memcmp(digest, sentinel, 16) == 0);
        if (mode <= 1) assert(p.calls == 0 && xrtReaderTell(reader, &position) && position == 0);
        if (mode == 4) assert(xrtErrorKind(xrtGetError()) == XERR_VALUE && strcmp(error.sMessage, "callback marker") == 0);
        assert(xrtReaderDestroy(reader));
    }
    reader = xrtReaderFromMemory((xbytesview){NULL, 0});
    p = (progress_state){0, 0, 0, 0, 0, true};
    assert(xrtMd5File(reader, NULL, digest, progress, &p, NULL));
    expect_hex(digest, "d41d8cd98f00b204e9800998ecf8427e");
    assert(p.calls == 1 && p.final == 1);
    assert(!xrtMd5File(reader, NULL, NULL, NULL, NULL, NULL));
    assert(xrtReaderDestroy(reader));
    assert(!xrtMd5File(NULL, NULL, digest, NULL, NULL, NULL));
    for (mode = 0; mode < 4; ++mode) {
        reader_state s = {0, 0, mode};
        xreaderops ops = {read_some, NULL, NULL, NULL, close_reader};
        reader = xrtReaderCreate(&ops, &s);
        memcpy(digest, sentinel, 16);
        p = (progress_state){0, 0, 0, 0, 0, false};
        assert(xrtMd5File(reader, NULL, digest, progress, &p, &error) == (mode == 0));
        assert(s.closed == 0);
        if (mode) {
            assert(memcmp(digest, sentinel, 16) == 0);
            if (mode >= 2) assert(strcmp(xrtErrorMessage(xrtGetError()), "reader marker") == 0);
            if (mode == 3) assert(xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        } else { expect_hex(digest, "9dd4e461268c8034f5c8564e155c67a6"); assert(p.final == 1); }
        assert(xrtReaderDestroy(reader) && s.closed == 1);
    }
}

int main(void)
{
    vectors();
    states();
    readers();
    xrtClearError();
    puts("md5 core: independent vectors, padding/chunks, states/readers passed");
    return 0;
}
