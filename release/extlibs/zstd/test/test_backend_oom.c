/* 使用后端正式 customMem 注入真实申请失败，不伪造返回码、不改 vendor。 */
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE
#define XRT_MODULE_ERROR_FORMAT
#define XRT_MODULE_FILE_TEMP
#define XRT_MODULE_PATH
#define XRT_MODULE_UNICODE
#define ZSTD_STATIC_LINKING_ONLY
#include "../include/xrt_zstd.h"
#include "../third_party/zstd.h"
#include "../third_party/zstd_errors.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static size_t failAt = SIZE_MAX, calls, live;
static bool triggered;
static unsigned failures, faultPoints, successful;

static void check(bool ok, const char* message)
{
    if (!ok) { ++failures; fprintf(stderr, "zstd backend OOM: %s\n", message); }
}

static void* allocate(void* context, size_t size)
{
    check(context == &failAt, "allocator context differs");
    if (calls++ >= failAt) { triggered = true; return NULL; }
    void* result = malloc(size);
    if (result) ++live;
    return result;
}

static void release(void* context, void* data)
{
    check(context == &failAt && (!data || live), "free context or live count differs");
    if (data) { --live; free(data); }
}

static ZSTD_CCtx* create_encoder(void)
{
    return ZSTD_createCCtx_advanced((ZSTD_customMem){allocate, release, &failAt});
}

static ZSTD_DCtx* create_decoder(void)
{
    return ZSTD_createDCtx_advanced((ZSTD_customMem){allocate, release, &failAt});
}

/* 只替换本测试编译单元的构造入口；实际编解码仍由原 Zstd 执行。 */
#define ZSTD_createCCtx create_encoder
#define ZSTD_createCStream create_encoder
#define ZSTD_createDStream create_decoder
#include "../xrt_zstd.c"
#undef ZSTD_createCCtx
#undef ZSTD_createCStream
#undef ZSTD_createDStream
#include "../zstd_native.c"
#ifdef __TINYC__
#include "../third_party/zstd.c"
#endif

static unsigned char source[196613];

static bool perform(unsigned mode, xbuffer* packed, xbuffer** output, xrtzstderror* error)
{
    xrtzstdoptions options;
    xrtZstdOptionsInit(&options);
    *output = NULL;
    xrtZstdErrorInit(error);
    if (mode <= 1) {
        if (mode == 1) options.tLimits.iMaxOutputBytes = packed->Size;
        return xrtZstdCompress(source, sizeof(source), &options, output, error);
    }
    if (mode == 2) return xrtZstdDecompress(packed->Data, packed->Size, &options, output, error);
    if (mode == 3) {
        xrtzstdcompressor* encoder = xrtZstdCompressorCreate(&options);
        bool ok = encoder && xrtZstdCompressorUpdate(encoder, source, sizeof(source)) &&
            xrtZstdCompressorFinish(encoder);
        if (ok) *output = xrtZstdCompressorTakeBuffer(encoder);
        else if (encoder) *error = *xrtZstdCompressorError(encoder);
        else error->iCode = XRT_ZSTD_ERROR_MEMORY;
        xrtZstdCompressorDestroy(encoder);
        return ok;
    }
    if (mode == 4) {
        xrtzstddecompressor* decoder = xrtZstdDecompressorCreate(&options);
        bool ok = decoder && xrtZstdDecompressorUpdate(decoder, packed->Data, packed->Size) &&
            xrtZstdDecompressorFinish(decoder);
        if (ok) *output = xrtZstdDecompressorTakeBuffer(decoder);
        else if (decoder) *error = *xrtZstdDecompressorError(decoder);
        else error->iCode = XRT_ZSTD_ERROR_MEMORY;
        xrtZstdDecompressorDestroy(decoder);
        return ok;
    }
    if (mode <= 6) {
        xbytesview input = mode == 5 ? (xbytesview){source, sizeof(source)} : xrtBufferView(packed);
        *output = xrtBufferCreate();
        xreader* reader = xrtReaderFromMemory(input);
        xwriter* writer = xrtWriterFromBuffer(*output);
        bool ok = mode == 5 ? xrtZstdCompressStream(reader, writer, &options, NULL, NULL, error) :
            xrtZstdDecompressStream(reader, writer, &options, NULL, NULL, error);
        check(xrtReaderDestroy(reader) && xrtWriterDestroy(writer), "cannot close test streams");
        return ok;
    }
    *output = mode == 7 ? xzstd_compress(source, sizeof(source), 3, 0, 0) :
        xzstd_decompress(packed->Data, packed->Size, 0, 0, 1000);
    if (!*output) {
        error->iCode = xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY ?
            XRT_ZSTD_ERROR_MEMORY : XRT_ZSTD_ERROR_DATA;
    }
    return *output != NULL;
}

static void sweep(unsigned mode, xbuffer* packed)
{
    for (size_t budget = 0; budget < 32; ++budget) {
        xmemdebugsnapshot before, after;
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        failAt = budget;
        calls = 0;
        triggered = false;
        xbuffer* output = NULL;
        xrtzstderror error;
        bool ok = perform(mode, packed, &output, &error);
        bool injected = triggered;
        failAt = SIZE_MAX;
        if (injected) {
            ++faultPoints;
            if (budget && mode < 7) {
                check(error.iBackendCode && ZSTD_getErrorCode((size_t)error.iBackendCode) ==
                    ZSTD_error_memory_allocation && error.sMessage[0], "original backend diagnostic was lost");
            }
            if (ok || error.iCode != XRT_ZSTD_ERROR_MEMORY) {
                ++failures;
                fprintf(stderr, "zstd backend OOM: mode=%u budget=%zu code=%d backend=%d\n",
                    mode, budget, error.iCode, error.iBackendCode);
            }
        } else {
            ++successful;
            check(ok && output && error.iCode == XRT_ZSTD_OK, "valid operation did not complete");
        }
        xrtBufferDestroy(output);
        xrtClearError();
        // 清除故障后立即重试同一路径，不沿用已失败的流对象。
        output = NULL;
        check(perform(mode, packed, &output, &error) && output, "same operation did not recover");
        if (output) {
            bool decode = mode == 2 || mode == 4 || mode == 6 || mode == 8;
            xbuffer* plain = NULL;
            if (decode) {
                check(output->Size == sizeof(source) && memcmp(output->Data, source, sizeof(source)) == 0,
                    "decoded recovery differs");
            } else {
                check(xrtZstdDecompress(output->Data, output->Size, NULL, &plain, &error) &&
                    plain && plain->Size == sizeof(source) && memcmp(plain->Data, source, sizeof(source)) == 0,
                    "compressed recovery differs");
            }
            xrtBufferDestroy(plain);
        }
        xrtBufferDestroy(output);
        xrtClearError();
        xrtMemDebugSnapshot(&after);
        check(!live && before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
            before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount &&
            before.UseAfterFreeCount == after.UseAfterFreeCount, "fault or recovery leaked memory");
        if (!injected) return;
    }
    check(false, "backend allocation sweep did not complete");
}

int main(void)
{
    uint32 state = 0x8b27u;
    for (size_t i = 0; i < sizeof(source); ++i) {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        source[i] = (unsigned char)state;
    }
    xbuffer* packed = NULL;
    xrtzstderror error;
    check(xrtZstdCompress(source, sizeof(source), NULL, &packed, &error), "cannot prepare standard frame");
    if (!packed) return 1;
    for (unsigned mode = 0; mode < 9; ++mode) sweep(mode, packed);
    xrtzstdoptions options;
    xrtZstdOptionsInit(&options);
    options.tLimits.iMaxOutputBytes = 1;
    xbuffer* output = NULL;
    check(!xrtZstdCompress(source, sizeof(source), &options, &output, &error) && !output &&
        error.iCode == XRT_ZSTD_ERROR_LIMIT, "real output limit lost its category");
    packed->Data[packed->Size - 1] ^= 1;
    check(!xrtZstdDecompress(packed->Data, packed->Size, NULL, &output, &error) && !output &&
        error.iCode == XRT_ZSTD_ERROR_DATA, "real damaged checksum lost its category");
    xrtBufferDestroy(packed);
    xrtClearError();
    if (!failures) printf("zstd backend OOM: 9 paths, %u faults, %u completions PASS\n", faultPoints, successful);
    return failures ? 1 : 0;
}
