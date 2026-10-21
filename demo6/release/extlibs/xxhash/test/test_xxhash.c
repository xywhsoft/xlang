/* 适配必须先于 CRT；目标宏可能在后面的系统头里才定义。 */
#include "../include/xrt_xxhash_tcc_compat.h"
#if defined(__TINYC__) && !defined(_umul128)
#error "TCC multiply adapter must not depend on system header order"
#endif

/* 包含核心，以白盒验证计数溢出；测试构建不重复链接核心对象。 */
#include "../xrt_xxhash.c"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
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

static void known_answers(void)
{
    uint8 data[4096];
    uint64 generator = 2654435761u, hash64;
    uint32 hash32;
    size_t i, j;
    for (i = 0; i < sizeof(data); ++i) {
        data[i] = (uint8)(generator >> 56);
        generator *= UINT64_C(11400714785074694797);
    }
    for (i = 0; i < sizeof(xxhash_vectors) / sizeof(xxhash_vectors[0]); ++i) {
        const xxhash_vector* v = &xxhash_vectors[i];
        if (v->bits == 32) {
            assert(xrtXxhash32(data, v->size, (uint32)v->seed, &hash32, NULL));
            assert(hash32 == v->hash);
        } else {
            xrtxxhashoptions options;
            xrtxxhashhasher* h;
            xrtXxhashOptionsInit(&options);
            options.iSeed = v->seed;
            assert(xrtXxhash64(data, v->size, v->seed, &hash64, NULL) && hash64 == v->hash);
            h = xrtXxhashHasherCreate(&options);
            assert(h);
            for (j = 0; j < v->size; ++j) assert(xrtXxhashHasherUpdate(h, data + j, 1));
            assert(xrtXxhashHasherInputBytes(h) == v->size);
            assert(xrtXxhashHasherDigest(h, &hash64) && hash64 == v->hash);
            assert(xrtXxhashHasherDigest(h, &hash64) && hash64 == v->hash);
            assert(xrtXxhashHasherReset(h) && xrtXxhashHasherUpdate(h, data, v->size));
            assert(xrtXxhashHasherDigest(h, &hash64) && hash64 == v->hash);
            xrtXxhashHasherDestroy(h);
        }
    }
}

static void state_boundaries(void)
{
    xrtxxhashoptions options;
    xrtxxhashhasher* h;
    xrtxxhasherror error;
    uint64 hash = 42;
    uint32 hash32 = 42;
    int field;
    assert(!xrtXxhash64(NULL, 1, 0, &hash, &error) && hash == 42);
    assert(error.iCode == XRT_XXHASH_ERROR_ARGUMENT && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(!xrtXxhash32(NULL, 1, 0, &hash32, NULL) && hash32 == 42);
    assert(!xrtXxhash64(NULL, 0, 0, NULL, NULL));
    assert(!xrtXxhash32(NULL, 0, 0, NULL, NULL));
    assert(xrtXxhash64(NULL, 0, 0, &hash, NULL) && hash == UINT64_C(0xef46db3751d8e999));
    assert(!xrtXxhashHasherUpdate(NULL, NULL, 0));
    assert(!xrtXxhashHasherDigest(NULL, &hash) && !xrtXxhashHasherReset(NULL));
    xrtClearError();
    assert(xrtXxhashHasherInputBytes(NULL) == 0 &&
        xrtErrorKind(xrtGetError()) == XERR_STATE &&
        xrtXxhashHasherError(NULL) == NULL);
    xrtXxhashHasherDestroy(NULL);
    xrtXxhashOptionsInit(&options);
    ++options.iVersion;
    assert(!xrtXxhashHasherCreate(&options));
    --options.iVersion;
    --options.iSize;
    assert(!xrtXxhashHasherCreate(&options));
    ++options.iSize;
    ++options.tLimits.iVersion;
    assert(!xrtXxhashHasherCreate(&options));
    xrtXxhashOptionsInit(&options);
    --options.tLimits.iSize;
    assert(!xrtXxhashHasherCreate(&options));
    xrtXxhashOptionsInit(&options);
    options.tLimits.iReserved = 1;
    assert(!xrtXxhashHasherCreate(&options) &&
        xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    for (field = 0; field < 7; ++field) {
        xrtXxhashOptionsInit(&options);
        set_unused_limit(&options.tLimits, field);
        assert(!xrtXxhashHasherCreate(&options) &&
            xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    }
    xrtXxhashOptionsInit(&options);
    options.iSeed = UINT64_MAX;
    options.tLimits.iMaxInputBytes = 3;
    h = xrtXxhashHasherCreate(&options);
    assert(h && xrtXxhashHasherUpdate(h, "abc", 3));
    assert(!xrtXxhashHasherDigest(h, NULL));
    assert(xrtXxhashHasherDigest(h, &hash));
    assert(!xrtXxhashHasherUpdate(h, "d", 1) && h->InputBytes == 3);
    assert(!xrtXxhashHasherDigest(h, &hash) && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    assert(xrtXxhashHasherReset(h) && !h->Failed && h->InputBytes == 0);
    assert(!xrtXxhashHasherUpdate(h, NULL, 1) && xrtXxhashHasherReset(h));
    h->Options.tLimits.iMaxInputBytes = 0;
    h->InputBytes = UINT64_MAX;
    assert(!xrtXxhashHasherUpdate(h, "x", 1) && h->InputBytes == UINT64_MAX);
    assert(xrtXxhashHasherReset(h) && xrtXxhashHasherUpdate(h, "abc", 3));
    assert(xrtXxhashHasherDigest(h, &hash) && hash == XXH64("abc", 3, UINT64_MAX));
    xrtXxhashHasherDestroy(h);
}

typedef struct progress_state { uint64 last, total; int calls, final, cancel; bool known; } progress_state;
static bool progress(const xrtprogress* p, ptr user)
{
    progress_state* s = user;
    uint64 nested;
    assert(p->iInputBytes >= s->last && p->iTotalInputBytes == s->total && p->iOutputBytes == 0);
    assert(((p->iFlags & XRT_PROGRESS_TOTAL_KNOWN) != 0) == s->known);
    s->last = p->iInputBytes;
    ++s->calls;
    if (p->iFlags & XRT_PROGRESS_FINAL) ++s->final;
    assert(xrtXxhash64("nested", 6, 1, &nested, NULL));
    if (s->cancel == 3) xrtSetErrorInfo(XERR_VALUE, "test.xxhash", 42, "callback marker");
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
    uint64 hash, expected, position;
    xreader* reader;
    xrtresourcelimits limits;
    xrtxxhasherror error;
    progress_state p;
    int mode, field;
    assert(xrtXxhash64(input + 1, sizeof(input) - 1, 1, &expected, NULL));
    reader = xrtReaderFromMemory((xbytesview){input, sizeof(input)});
    assert(reader && xrtReaderSeek(reader, 1, XSEEK_START, &position));
    p = (progress_state){0, sizeof(input) - 1, 0, 0, 0, true};
    assert(xrtXxhashFile64(reader, 1, NULL, &hash, progress, &p, &error));
    assert(hash == expected && p.calls == 3 && p.final == 1 && p.last == sizeof(input) - 1);
    assert(xrtReaderDestroy(reader));
    for (mode = 0; mode < 7; ++mode) {
        reader = xrtReaderFromMemory((xbytesview){input, sizeof(input)});
        hash = 42;
        xrtXxhashLimitsInit(&limits);
        p = (progress_state){0, sizeof(input), 0, 0, 0, true};
        if (mode == 0) limits.iMaxInputBytes = sizeof(input) - 1;
        if (mode == 1) ++limits.iVersion;
        if (mode >= 2 && mode <= 4) p.cancel = mode - 1;
        if (mode == 5) --limits.iSize;
        if (mode == 6) limits.iReserved = 1;
        assert(!xrtXxhashFile64(reader, 0, &limits, &hash, progress, &p, &error) && hash == 42);
        if (mode <= 1 || mode >= 5) {
            assert(p.calls == 0 && xrtReaderTell(reader, &position) && position == 0);
            if (mode >= 5) assert(error.iCode == XRT_XXHASH_ERROR_ARGUMENT &&
                xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
        }
        if (mode == 4) assert(xrtErrorKind(xrtGetError()) == XERR_VALUE && strcmp(error.sMessage, "callback marker") == 0);
        assert(xrtReaderDestroy(reader));
    }
    for (field = 0; field < 7; ++field) {
        reader = xrtReaderFromMemory((xbytesview){input, sizeof(input)});
        assert(reader != NULL);
        xrtXxhashLimitsInit(&limits);
        set_unused_limit(&limits, field);
        hash = 42;
        assert(!xrtXxhashFile64(reader, 0, &limits, &hash, NULL, NULL, &error) &&
            hash == 42 && error.iCode == XRT_XXHASH_ERROR_ARGUMENT &&
            xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
        assert(xrtReaderTell(reader, &position) && position == 0);
        assert(xrtReaderDestroy(reader));
    }
    reader = xrtReaderFromMemory((xbytesview){NULL, 0});
    p = (progress_state){0, 0, 0, 0, 0, true};
    assert(xrtXxhashFile64(reader, 0, NULL, &hash, progress, &p, NULL));
    assert(hash == UINT64_C(0xef46db3751d8e999) && p.calls == 1 && p.final == 1);
    assert(xrtReaderDestroy(reader));
    assert(!xrtXxhashFile64(NULL, 0, NULL, &hash, NULL, NULL, NULL));
    for (mode = 0; mode < 4; ++mode) {
        reader_state s = {0, 0, mode};
        xreaderops ops = {read_some, NULL, NULL, NULL, close_reader};
        reader = xrtReaderCreate(&ops, &s);
        hash = 42;
        p = (progress_state){0, 0, 0, 0, 0, false};
        assert(xrtXxhashFile64(reader, 0, NULL, &hash, progress, &p, &error) == (mode == 0));
        assert(s.closed == 0);
        if (mode) {
            assert(hash == 42);
            if (mode >= 2) assert(strcmp(xrtErrorMessage(xrtGetError()), "reader marker") == 0);
            if (mode == 3) assert(xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        } else assert(hash == XXH64("x", 1, 0) && p.final == 1);
        assert(xrtReaderDestroy(reader) && s.closed == 1);
    }
}

static void multiply_boundary(void)
{
    unsigned long long high, low;
    low = xrt_xxhash_umul128(0xffffffffULL, 0xffffffffULL, &high);
    assert(low == 0xfffffffe00000001ULL && high == 0);
    low = xrt_xxhash_umul128(UINT64_MAX, UINT64_MAX, &high);
    assert(low == 1 && high == UINT64_MAX - 1);
    /* 编译器可提供 128 位整数时，做独立算术差分。 */
#if defined(__SIZEOF_INT128__)
    {
        uint64 a = 17, b = 29;
        size_t i;
        for (i = 0; i < 10000; ++i) {
            __uint128_t product;
            a = a * UINT64_C(6364136223846793005) + 1;
            b = b * UINT64_C(1442695040888963407) + 3;
            product = (__uint128_t)a * b;
            low = xrt_xxhash_umul128(a, b, &high);
            assert(low == (uint64)product && high == (uint64)(product >> 64));
        }
    }
#endif
}

int main(void)
{
    known_answers();
    state_boundaries();
    reader_boundaries();
    multiply_boundary();
    xrtClearError();
    printf("xxhash core: %zu independent vectors, state/reader/multiply boundaries passed\n",
        sizeof(xxhash_vectors) / sizeof(xxhash_vectors[0]));
    return 0;
}
