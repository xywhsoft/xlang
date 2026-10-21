#define XRT_MODULE_MEMORY_DEBUG
#include "../include/xrt_lzma.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "lzma allocation test failed at %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

/* XZ 编解码的每个 XRT 分配失败都不得留下半份结果。 */
static int scan_failures(xbytesview plain, xbytesview packed, bool decode)
{
    xmemdebugsnapshot before, after;
    unsigned int index, rejected = 0, succeeded = 0;
    xrtMemDebugSnapshot(&before);
    for (index = 0; index < 256u && succeeded < 2u; ++index) {
        xbuffer* output = NULL;
        xrtlzmaerror error;
        bool ok, injected;
        CHECK(xrtMemDebugFailAfter(index));
        ok = decode ? xrtLzmaDecompress(packed.Data, packed.Size, NULL, &output, &error) :
            xrtLzmaCompress(plain.Data, plain.Size, NULL, &output, &error);
        injected = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (injected) {
            if (ok || output != NULL || error.iCode != XRT_LZMA_ERROR_MEMORY)
                fprintf(stderr, "lzma injected failure: decode=%d index=%u ok=%d output=%p code=%d xrt=%d\n",
                    (int)decode, index, (int)ok, (void*)output, error.iCode,
                    xrtGetError() != NULL ? (int)xrtErrorKind(xrtGetError()) : -1);
            CHECK(!ok && output == NULL && error.iCode == XRT_LZMA_ERROR_MEMORY);
            ++rejected;
        } else {
            CHECK(ok && output != NULL && error.iCode == XRT_LZMA_OK);
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
    unsigned char plain[256];
    xbuffer* packed = NULL;
    xrtlzmaerror error;
    size_t i;
    CHECK(xrtMemDebugEnable(true));
    for (i = 0; i < sizeof(plain); ++i) plain[i] = (unsigned char)(i * 31u + i / 17u);
    CHECK(xrtLzmaCompress(plain, sizeof(plain), NULL, &packed, &error));
    CHECK(scan_failures((xbytesview){plain, sizeof(plain)}, xrtBufferView(packed), false) == 0);
    CHECK(scan_failures((xbytesview){plain, sizeof(plain)}, xrtBufferView(packed), true) == 0);
    xrtBufferDestroy(packed);
    puts("lzma allocation failures: PASS");
    return 0;
}
