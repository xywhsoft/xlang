#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE
#include "../include/xrt_zstd.h"
#include "../include/zstd_native.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "zstd allocation test failed at %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

static int scan_failures(xbytesview plain, xbytesview packed, bool decode)
{
    xmemdebugsnapshot before, after;
    unsigned int index, rejected = 0, succeeded = 0;
    xrtMemDebugSnapshot(&before);
    for (index = 0; index < 128u && succeeded < 2u; ++index) {
        xbuffer* output = NULL;
        xrtzstderror error;
        bool ok, injected;
        CHECK(xrtMemDebugFailAfter(index));
        ok = decode ? xrtZstdDecompress(packed.Data, packed.Size, NULL, &output, &error) :
            xrtZstdCompress(plain.Data, plain.Size, NULL, &output, &error);
        injected = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (injected) {
            CHECK(!ok && output == NULL && error.iCode == XRT_ZSTD_ERROR_MEMORY);
            ++rejected;
        } else {
            CHECK(ok && output != NULL && error.iCode == XRT_ZSTD_OK);
            if (decode) {
                CHECK(output->Size == plain.Size &&
                    memcmp(output->Data, plain.Data, plain.Size) == 0);
            }
            ++succeeded;
        }
        xrtBufferDestroy(output);
        xrtClearError();
        xrtMemDebugSnapshot(&after);
        CHECK(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes);
        CHECK(after.InvalidFreeCount == before.InvalidFreeCount &&
            after.DoubleFreeCount == before.DoubleFreeCount &&
            after.OverflowCount == before.OverflowCount &&
            after.UnderflowCount == before.UnderflowCount);
    }
    CHECK(rejected > 0u && succeeded == 2u);
    return 0;
}

int main(void)
{
    unsigned char plain[4096];
    xbuffer* packed = NULL;
    xrtzstderror error;
    size_t i;
    CHECK(xrtMemDebugEnable(true));
    for (i = 0; i < sizeof(plain); ++i) plain[i] = (unsigned char)(i * 31u + i / 17u);
    CHECK(xrtZstdCompress(plain, sizeof(plain), NULL, &packed, &error));
    CHECK(scan_failures((xbytesview){plain, sizeof(plain)}, xrtBufferView(packed), false) == 0);
    CHECK(scan_failures((xbytesview){plain, sizeof(plain)}, xrtBufferView(packed), true) == 0);
    /* 桥接错误分类只读当前线程错误，不把旧失败带入下一次调用。 */
    CHECK(!xzstd_last_error_is_memory());
    CHECK(xrtMemDebugFailAfter(0u));
    CHECK(xzstd_compress(plain, sizeof(plain), 3, 0u, 0u) == NULL);
    CHECK(xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    CHECK(xzstd_last_error_is_memory());
    xrtClearError();
    CHECK(!xzstd_last_error_is_memory());
    CHECK(!xzstd_last_error_is_range());
    CHECK(xzstd_decompress(packed->Data, packed->Size, 0u, 1u, 1000u) == NULL);
    CHECK(xzstd_last_error_is_range());
    CHECK(!xzstd_last_error_is_memory());
    xrtClearError();
    CHECK(!xzstd_last_error_is_range());
    xrtBufferDestroy(packed);
    puts("zstd allocation failures: PASS");
    return 0;
}
