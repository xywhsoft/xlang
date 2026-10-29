#ifndef XRT_MODULE_CODEC_HEX
#define XRT_MODULE_CODEC_HEX
#endif
/* 包含核心以精确验证 uint64 计数溢出；此测试不再链接核心对象文件。 */
#include "../xrt_blake3.c"
/* 测试操作位于断言中，发布配置也必须执行。 */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include "vectors.h"

static void set_unused_limit(xrtresourcelimits* limits, int field)
{
    switch (field) {
    case 0: limits->iMaxItemBytes = 1; break;
    case 1: limits->iMaxEntries = 1; break;
    case 2: limits->iMaxNodes = 1; break;
    case 3: limits->iMaxDepth = 1; break;
    case 4: limits->iMaxCompressionRatio = 1; break;
    case 5: limits->iFlags = 1; break;
    }
}

static xrtblake3hasher* create(int mode, const xrtblake3options* options)
{
    if (mode == 1) return xrtBlake3HasherCreateKeyed(options, (const uint8*)vector_key);
    if (mode == 2) return xrtBlake3HasherCreateDeriveKey(options, vector_context, strlen(vector_context));
    return xrtBlake3HasherCreate(options);
}

static bool once(int mode, const void* data, size_t size, uint8* output, size_t outlen)
{
    if (mode == 1) return xrtBlake3HashKeyed(data, size, (const uint8*)vector_key, output, outlen, NULL);
    if (mode == 2) return xrtBlake3HashKdf(data, size, vector_context, strlen(vector_context), output, outlen, NULL);
    return xrtBlake3Hash(data, size, output, outlen, NULL);
}

static void expect(const uint8* data, size_t size, const char* hex)
{
    char text[263];
    size_t written;
    assert(xrtHexEncode(data, size, text, sizeof(text), &written, 0));
    assert(written == size * 2 && strncmp(text, hex, written) == 0);
}

static void known_answers(void)
{
    static uint8 input[102400];
    static const size_t sizes[] = {1, 16, 32, 64, 65, 131};
    uint8 output[133];
    size_t i, v, n;
    int mode;
    for (i = 0; i < sizeof(input); ++i) input[i] = (uint8)(i % 251);
    for (v = 0; v < sizeof(vectors) / sizeof(vectors[0]); ++v) {
        size_t size = vectors[v].size, offset = 0;
        assert(size <= sizeof(input));
        for (mode = 0; mode < 3; ++mode) {
            xrtblake3hasher* h = create(mode, NULL);
            xrtblake3hasher* copy;
            assert(h);
            offset = 0;
            while (offset < size / 2) {
                size_t step = 1 + (offset * 17 + 3) % 1031;
                if (step > size / 2 - offset) step = size / 2 - offset;
                assert(xrtBlake3HasherUpdate(h, input + offset, step));
                assert(xrtBlake3HasherUpdate(h, NULL, 0));
                offset += step;
            }
            copy = xrtBlake3HasherClone(h);
            assert(copy && xrtBlake3HasherUpdate(h, input + offset, size - offset));
            assert(xrtBlake3HasherUpdate(copy, input + offset, size - offset));
            for (n = 0; n < sizeof(sizes) / sizeof(sizes[0]); ++n) {
                memset(output, 0xa7, sizeof(output));
                assert(once(mode, input, size, output + 1, sizes[n]));
                expect(output + 1, sizes[n], vectors[v].hash[mode]);
                assert(output[0] == 0xa7 && output[sizes[n] + 1] == 0xa7);
                assert(xrtBlake3HasherDigest(h, output + 1, sizes[n]));
                expect(output + 1, sizes[n], vectors[v].hash[mode]);
                assert(xrtBlake3HasherDigest(copy, output + 1, sizes[n]));
                expect(output + 1, sizes[n], vectors[v].hash[mode]);
            }
            assert(xrtBlake3HasherInputBytes(h) == size);
            assert(xrtBlake3HasherReset(h) && xrtBlake3HasherInputBytes(h) == 0);
            assert(xrtBlake3HasherUpdate(h, input, size));
            assert(xrtBlake3HasherDigest(h, output, 131));
            expect(output, 131, vectors[v].hash[mode]);
            xrtBlake3HasherDestroy(copy);
            xrtBlake3HasherDestroy(h);
        }
    }
}

static void state_boundaries(void)
{
    uint8 out[132], expected[131], sentinel[132];
    char context[2049];
    xrtblake3options options;
    xrtblake3hasher* h;
    xrtblake3hasher* copy;
    size_t i;
    int field;
    memset(sentinel, 0x7c, sizeof(sentinel));
    memcpy(out, sentinel, sizeof(out));
    assert(!xrtBlake3HashKeyed(NULL, 1, (const uint8*)vector_key, out, 32, NULL));
    assert(!xrtBlake3HashKdf(NULL, 1, vector_context, strlen(vector_context), out, 32, NULL));
    assert(!xrtBlake3HashKeyed("x", 1, NULL, out, 32, NULL));
    assert(!xrtBlake3HashKdf("x", 1, NULL, 0, out, 32, NULL));
    assert(!xrtBlake3Hash(NULL, 0, NULL, 32, NULL));
    assert(!xrtBlake3Hash(NULL, 0, out, 0, NULL));
    assert(memcmp(out, sentinel, sizeof(out)) == 0);
    assert(!xrtBlake3HasherUpdate(NULL, NULL, 0));
    assert(!xrtBlake3HasherReset(NULL));
    assert(!xrtBlake3HasherDigest(NULL, out, 32));
    assert(!xrtBlake3HasherClone(NULL));
    assert(xrtBlake3HasherError(NULL) == NULL && xrtBlake3HasherInputBytes(NULL) == 0);
    xrtBlake3HasherDestroy(NULL);

    xrtBlake3OptionsInit(&options);
    ++options.iVersion;
    assert(!xrtBlake3HasherCreate(&options));
    --options.iVersion;
    --options.iSize;
    assert(!xrtBlake3HasherCreate(&options));
    ++options.iSize;
    ++options.tLimits.iVersion;
    assert(!xrtBlake3HasherCreate(&options));
    xrtBlake3OptionsInit(&options);
    --options.tLimits.iSize;
    assert(!xrtBlake3HasherCreate(&options));
    xrtBlake3OptionsInit(&options);
    options.tLimits.iReserved = 1;
    assert(!xrtBlake3HasherCreate(&options) &&
        xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    for (field = 0; field < 6; ++field) {
        xrtBlake3OptionsInit(&options);
        set_unused_limit(&options.tLimits, field);
        assert(!xrtBlake3HasherCreate(&options) &&
            xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    }
    assert(!xrtBlake3HasherCreateKeyed(NULL, NULL));
    assert(!xrtBlake3HasherCreateDeriveKey(NULL, NULL, 0));

    xrtBlake3OptionsInit(&options);
    options.tLimits.iMaxInputBytes = 3;
    options.tLimits.iMaxOutputBytes = 32;
    h = xrtBlake3HasherCreate(&options);
    assert(h && xrtBlake3HasherUpdate(h, "abc", 3));
    assert(!xrtBlake3HasherDigest(h, out, 33));
    assert(xrtBlake3HasherError(h)->iCode == XRT_BLAKE3_ERROR_LIMIT);
    assert(xrtBlake3HasherDigest(h, out, 32));
    assert(!xrtBlake3HasherUpdate(h, "d", 1));
    assert(xrtBlake3HasherInputBytes(h) == 3 && !xrtBlake3HasherDigest(h, out, 32));
    copy = xrtBlake3HasherClone(h);
    assert(copy && !xrtBlake3HasherUpdate(copy, NULL, 0));
    assert(xrtBlake3HasherReset(copy) && xrtBlake3HasherUpdate(copy, "x", 1));
    xrtBlake3HasherDestroy(copy);
    assert(xrtBlake3HasherReset(h));
    assert(!xrtBlake3HasherUpdate(h, NULL, 1));
    assert(xrtBlake3HasherReset(h));
    h->Options.tLimits.iMaxInputBytes = 0;
    h->InputBytes = UINT64_MAX;
    assert(!xrtBlake3HasherUpdate(h, "x", 1));
    assert(h->InputBytes == UINT64_MAX && h->Error.iCode == XRT_BLAKE3_ERROR_LIMIT);
    assert(xrtBlake3HasherReset(h) && xrtBlake3HasherUpdate(h, "abc", 3));
    xrtBlake3HasherDestroy(h);

    /* 长上下文先派生一次，reset/clone 不得换成截断后的上下文。 */
    for (i = 0; i + 1 < sizeof(context); ++i) context[i] = (char)('a' + i % 26);
    context[sizeof(context) - 1] = 0;
    assert(xrtBlake3HashKdf("secret", 6, context, strlen(context), expected, sizeof(expected), NULL));
    h = xrtBlake3HasherCreateDeriveKey(NULL, context, strlen(context));
    assert(h);
    for (i = 0; i < 3; ++i) {
        assert(xrtBlake3HasherUpdate(h, "secret", 6));
        copy = xrtBlake3HasherClone(h);
        assert(copy && xrtBlake3HasherDigest(copy, out, 131));
        assert(memcmp(out, expected, 131) == 0);
        xrtBlake3HasherDestroy(copy);
        assert(xrtBlake3HasherReset(h));
    }
    xrtBlake3HasherDestroy(h);
}

static void binary_context(void)
{
    static const char context[] = {'p', 'u', 'r', 'p', 'o', 's', 'e', 0,
        's', 'e', 'c', 'o', 'n', 'd'};
    uint8 expected[32], output[32], prefix[32];
    blake3_hasher reference;
    xrtblake3hasher* h;

    /* 上下文是带长度的字节序列，中间的 NUL 不能截断派生域。 */
    blake3_hasher_init_derive_key_raw(&reference, context, sizeof(context));
    blake3_hasher_update(&reference, "secret", 6);
    blake3_hasher_finalize(&reference, expected, sizeof(expected));
    assert(xrtBlake3HashKdf("secret", 6, context, sizeof(context), output, sizeof(output), NULL));
    assert(memcmp(output, expected, sizeof(output)) == 0);
    assert(xrtBlake3HashKdf("secret", 6, "purpose", 7, prefix, sizeof(prefix), NULL));
    assert(memcmp(output, prefix, sizeof(output)) != 0);
    h = xrtBlake3HasherCreateDeriveKey(NULL, context, sizeof(context));
    assert(h && xrtBlake3HasherUpdate(h, "secret", 6));
    assert(xrtBlake3HasherDigest(h, output, sizeof(output)));
    assert(memcmp(output, expected, sizeof(output)) == 0);
    xrtBlake3HasherDestroy(h);
}

typedef struct progress_state { uint64 last, total; int calls, final, cancel; bool known; } progress_state;
static bool progress(const xrtprogress* p, ptr user)
{
    progress_state* s = user;
    assert(p->iInputBytes >= s->last && p->iTotalInputBytes == s->total && p->iOutputBytes == 0);
    assert(((p->iFlags & XRT_PROGRESS_TOTAL_KNOWN) != 0) == s->known);
    s->last = p->iInputBytes;
    ++s->calls;
    if (p->iFlags & XRT_PROGRESS_FINAL) ++s->final;
    if (s->cancel == 3) xrtSetErrorInfo(XERR_VALUE, "test.blake3", 0, "callback marker");
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

static void reader_boundaries(void)
{
    static uint8 input[65538];
    uint8 out[131], expected[131], sentinel[131];
    xreader* reader;
    xrtresourcelimits limits;
    xrtblake3error error;
    progress_state p;
    uint64 position;
    int mode, field;
    assert(xrtBlake3Hash(input + 1, sizeof(input) - 1, expected, 131, NULL));
    reader = xrtReaderFromMemory((xbytesview){input, sizeof(input)});
    assert(reader && xrtReaderSeek(reader, 1, XSEEK_START, &position));
    p = (progress_state){0, sizeof(input) - 1, 0, 0, 0, true};
    assert(xrtBlake3File(reader, NULL, out, 131, progress, &p, &error));
    assert(p.calls == 3 && p.final == 1 && p.last == sizeof(input) - 1);
    assert(memcmp(out, expected, 131) == 0);
    xrtReaderDestroy(reader);
    memset(sentinel, 0x7c, sizeof(sentinel));
    for (mode = 0; mode < 9; ++mode) {
        reader = xrtReaderFromMemory((xbytesview){input, sizeof(input)});
        assert(reader);
        memcpy(out, sentinel, sizeof(out));
        xrtBlake3LimitsInit(&limits);
        p = (progress_state){0, sizeof(input), 0, 0, 0, true};
        if (mode == 0) limits.iMaxInputBytes = sizeof(input) - 1;
        if (mode == 1) ++limits.iVersion;
        if (mode == 2) limits.iMaxOutputBytes = 130;
        if (mode >= 3 && mode <= 5) p.cancel = mode - 2;
        if (mode == 7) --limits.iSize;
        if (mode == 8) limits.iReserved = 1;
        assert(!xrtBlake3File(reader, &limits, out, mode == 6 ? 0 : 131, progress, &p, &error));
        assert(memcmp(out, sentinel, sizeof(out)) == 0);
        if (mode == 5) assert(xrtErrorKind(xrtGetError()) == XERR_VALUE && strcmp(xrtErrorMessage(xrtGetError()), "callback marker") == 0);
        if (mode == 1 || mode == 2 || mode >= 6)
            assert(p.calls == 0 && xrtReaderTell(reader, &position) && position == 0);
        if (mode >= 7) assert(error.iCode == XRT_BLAKE3_ERROR_ARGUMENT &&
            xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
        xrtReaderDestroy(reader);
    }
    for (field = 0; field < 6; ++field) {
        reader = xrtReaderFromMemory((xbytesview){input, sizeof(input)});
        assert(reader != NULL);
        xrtBlake3LimitsInit(&limits);
        set_unused_limit(&limits, field);
        memcpy(out, sentinel, sizeof(out));
        assert(!xrtBlake3File(reader, &limits, out, 131, NULL, NULL, &error) &&
            error.iCode == XRT_BLAKE3_ERROR_ARGUMENT &&
            xrtErrorKind(xrtGetError()) == XERR_ARGUMENT &&
            memcmp(out, sentinel, sizeof(out)) == 0 &&
            xrtReaderTell(reader, &position) && position == 0);
        assert(xrtReaderDestroy(reader));
    }
    reader = xrtReaderFromMemory((xbytesview){NULL, 0});
    p = (progress_state){0, 0, 0, 0, 0, true};
    assert(reader && xrtBlake3File(reader, NULL, out, 131, progress, &p, NULL));
    assert(p.calls == 1 && p.final == 1);
    expect(out, 131, vectors[0].hash[0]);
    xrtReaderDestroy(reader);
    for (mode = 0; mode < 4; ++mode) {
        reader_state s = {0, 0, mode};
        xreaderops ops = {read_some, NULL, NULL, NULL, close_reader};
        reader = xrtReaderCreate(&ops, &s);
        assert(reader);
        memcpy(out, sentinel, sizeof(out));
        p = (progress_state){0, 0, 0, 0, 0, false};
        assert(xrtBlake3File(reader, NULL, out, 131, progress, &p, &error) == (mode == 0));
        assert(s.closed == 0);
        if (mode) {
            assert(error.iCode == (mode == 3 ? XRT_BLAKE3_ERROR_MEMORY : XRT_BLAKE3_ERROR_IO));
            assert(memcmp(out, sentinel, sizeof(out)) == 0);
            if (mode >= 2) {
                assert(xrtErrorKind(xrtGetError()) == (mode == 3 ? XERR_MEMORY : XERR_IO));
                assert(strcmp(xrtErrorMessage(xrtGetError()), "reader marker") == 0);
                assert(strcmp(error.sMessage, "reader marker") == 0);
            }
        }
        else {
            assert(p.calls == 2 && p.final == 1 && xrtBlake3Hash("x", 1, expected, 131, NULL));
            assert(memcmp(out, expected, 131) == 0);
        }
        xrtReaderDestroy(reader);
        assert(s.closed == 1);
    }
}

int main(void)
{
    known_answers();
    state_boundaries();
    binary_context();
    reader_boundaries();
    xrtClearError();
    printf("blake3 C: %zu official vectors, three modes, six output sizes, state and reader boundaries passed\n",
        sizeof(vectors) / sizeof(vectors[0]));
    return 0;
}
