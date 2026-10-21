/* 核心白盒测试只编译本文件，不再链接一份核心对象。 */
#include "../xrt_crc32.c"
#include <assert.h>

/* 独立逐位算法，不使用被测实现的表或 slice-by-8 装载规则。 */
static uint32 reference(const uint8* data, size_t size)
{
    uint32 crc = 0xFFFFFFFFu;
    size_t i;
    for (i = 0; i < size; ++i) {
        unsigned int bit;
        crc ^= data[i];
        for (bit = 0; bit < 8; ++bit)
            crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
    }
    return ~crc;
}

static void vectors(void)
{
    uint8 data[4112];
    uint32 seed = 17, crc;
    size_t i, size, offset;
    static const struct { const char* data; uint32 crc; } fixed[] = {
        {"", 0}, {"123456789", 0xCBF43926u}, {"abc", 0x352441C2u},
        {"The quick brown fox jumps over the lazy dog", 0x414FA339u}
    };
    for (i = 0; i < sizeof(fixed) / sizeof(fixed[0]); ++i) {
        assert(xrtCrc32Hash(fixed[i].data, strlen(fixed[i].data), &crc, NULL));
        assert(crc == fixed[i].crc);
    }
    for (i = 0; i < sizeof(data); ++i) { seed = seed * 1664525u + 1013904223u; data[i] = (uint8)(seed >> 24); }
    assert(crc32_le32((const uint8*)"\x01\x23\x45\x67") == 0x67452301u);
    for (offset = 0; offset < 16; ++offset) {
        for (size = 0; size <= 4096; size += size < 64 ? 1 : 127) {
            xrtcrc32hasher* h = xrtCrc32HasherCreate(NULL);
            uint32 expected = reference(data + offset, size);
            size_t consumed = 0;
            assert(h && xrtCrc32Hash(data + offset, size, &crc, NULL) && crc == expected);
            while (consumed < size) {
                size_t count = (consumed % 31) + 1;
                if (count > size - consumed) count = size - consumed;
                assert(xrtCrc32HasherUpdate(h, data + offset + consumed, count));
                consumed += count;
            }
            assert(xrtCrc32HasherInputBytes(h) == size);
            assert(xrtCrc32HasherDigest(h, &crc) && crc == expected);
            assert(xrtCrc32HasherDigest(h, &crc) && crc == expected);
            assert(xrtCrc32HasherReset(h) && xrtCrc32HasherUpdate(h, data + offset, size));
            assert(xrtCrc32HasherDigest(h, &crc) && crc == expected);
            xrtCrc32HasherDestroy(h);
        }
    }
}

static void states(void)
{
    xrtcrc32options options;
    xrtcrc32error error;
    xrtcrc32hasher* h;
    uint32 crc = 42;
    assert(!xrtCrc32Hash(NULL, 1, &crc, &error) && crc == 42);
    assert(error.iCode == XRT_CRC32_ERROR_ARGUMENT && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(!xrtCrc32Hash(NULL, 0, NULL, NULL));
    assert(xrtCrc32Hash(NULL, 0, &crc, NULL) && crc == 0);
    assert(!xrtCrc32HasherUpdate(NULL, NULL, 0));
    assert(!xrtCrc32HasherDigest(NULL, &crc) && !xrtCrc32HasherReset(NULL));
    assert(xrtCrc32HasherInputBytes(NULL) == 0 && xrtCrc32HasherError(NULL) == NULL);
    xrtCrc32HasherDestroy(NULL);
    xrtCrc32OptionsInit(&options);
    ++options.iVersion;
    assert(!xrtCrc32HasherCreate(&options));
    --options.iVersion;
    --options.iSize;
    assert(!xrtCrc32HasherCreate(&options));
    ++options.iSize;
    ++options.tLimits.iVersion;
    assert(!xrtCrc32HasherCreate(&options));
    xrtCrc32OptionsInit(&options);
    --options.tLimits.iSize;
    assert(!xrtCrc32HasherCreate(&options));
    xrtCrc32OptionsInit(&options);
    options.tLimits.iMaxInputBytes = 3;
    h = xrtCrc32HasherCreate(&options);
    assert(h && xrtCrc32HasherUpdate(h, "abc", 3));
    assert(!xrtCrc32HasherDigest(h, NULL));
    assert(xrtCrc32HasherDigest(h, &crc) && crc == 0x352441C2u);
    assert(!xrtCrc32HasherUpdate(h, "d", 1) && h->InputBytes == 3);
    assert(!xrtCrc32HasherDigest(h, &crc) && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    assert(!xrtCrc32HasherUpdate(h, "", 0) && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    assert(xrtCrc32HasherReset(h) && !h->Failed && h->InputBytes == 0);
    assert(!xrtCrc32HasherUpdate(h, NULL, 1) && xrtCrc32HasherReset(h));
    h->Options.tLimits.iMaxInputBytes = 0;
    h->InputBytes = UINT64_MAX;
    assert(!xrtCrc32HasherUpdate(h, "x", 1) && h->InputBytes == UINT64_MAX);
    assert(xrtCrc32HasherReset(h) && xrtCrc32HasherUpdate(h, "123456789", 9));
    assert(xrtCrc32HasherDigest(h, &crc) && crc == 0xCBF43926u);
    xrtCrc32HasherDestroy(h);
}

typedef struct progress_state { uint64 last, total; int calls, final, cancel; bool known; } progress_state;
static bool progress(const xrtprogress* p, ptr user)
{
    progress_state* s = user;
    uint32 nested;
    assert(p->iInputBytes >= s->last && p->iTotalInputBytes == s->total && p->iOutputBytes == 0);
    assert(((p->iFlags & XRT_PROGRESS_TOTAL_KNOWN) != 0) == s->known);
    s->last = p->iInputBytes;
    ++s->calls;
    if (p->iFlags & XRT_PROGRESS_FINAL) ++s->final;
    assert(xrtCrc32Hash("nested", 6, &nested, NULL));
    if (s->cancel == 3) xrtSetErrorInfo(XERR_VALUE, "test.crc32", 42, "callback marker");
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
    uint32 crc, expected;
    uint64 position;
    xreader* reader;
    xrtresourcelimits limits;
    xrtcrc32error error;
    progress_state p;
    int mode;
    assert(xrtCrc32Hash(input + 1, sizeof(input) - 1, &expected, NULL));
    reader = xrtReaderFromMemory((xbytesview){input, sizeof(input)});
    assert(reader && xrtReaderSeek(reader, 1, XSEEK_START, &position));
    p = (progress_state){0, sizeof(input) - 1, 0, 0, 0, true};
    assert(xrtCrc32File(reader, NULL, &crc, progress, &p, &error));
    assert(crc == expected && p.calls == 3 && p.final == 1 && p.last == sizeof(input) - 1);
    assert(xrtReaderDestroy(reader));
    for (mode = 0; mode < 5; ++mode) {
        reader = xrtReaderFromMemory((xbytesview){input, sizeof(input)});
        crc = 42;
        xrtResourceLimitsInit(&limits);
        p = (progress_state){0, sizeof(input), 0, 0, 0, true};
        if (mode == 0) limits.iMaxInputBytes = sizeof(input) - 1;
        if (mode == 1) ++limits.iVersion;
        if (mode >= 2) p.cancel = mode - 1;
        assert(!xrtCrc32File(reader, &limits, &crc, progress, &p, &error) && crc == 42);
        if (mode <= 1) assert(p.calls == 0 && xrtReaderTell(reader, &position) && position == 0);
        if (mode == 4) assert(xrtErrorKind(xrtGetError()) == XERR_VALUE && strcmp(error.sMessage, "callback marker") == 0);
        assert(xrtReaderDestroy(reader));
    }
    reader = xrtReaderFromMemory((xbytesview){NULL, 0});
    p = (progress_state){0, 0, 0, 0, 0, true};
    assert(xrtCrc32File(reader, NULL, &crc, progress, &p, NULL));
    assert(crc == 0 && p.calls == 1 && p.final == 1);
    assert(!xrtCrc32File(reader, NULL, NULL, NULL, NULL, NULL));
    assert(xrtReaderDestroy(reader));
    assert(!xrtCrc32File(NULL, NULL, &crc, NULL, NULL, NULL));
    for (mode = 0; mode < 4; ++mode) {
        reader_state s = {0, 0, mode};
        xreaderops ops = {read_some, NULL, NULL, NULL, close_reader};
        reader = xrtReaderCreate(&ops, &s);
        crc = 42;
        p = (progress_state){0, 0, 0, 0, 0, false};
        assert(xrtCrc32File(reader, NULL, &crc, progress, &p, &error) == (mode == 0));
        assert(s.closed == 0);
        if (mode) {
            assert(crc == 42);
            if (mode >= 2) assert(strcmp(xrtErrorMessage(xrtGetError()), "reader marker") == 0);
            if (mode == 3) assert(xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        } else assert(crc == reference((const uint8*)"x", 1) && p.final == 1);
        assert(xrtReaderDestroy(reader) && s.closed == 1);
    }
}

int main(void)
{
    vectors();
    states();
    readers();
    xrtClearError();
    puts("crc32 core: known answers, 1536 independent differential cases, states/readers passed");
    return 0;
}
