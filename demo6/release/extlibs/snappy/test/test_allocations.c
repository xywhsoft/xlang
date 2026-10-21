#define XRT_MODULE_MEMORY_DEBUG
#include "../include/xrt_snappy.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "snappy allocation test failed at %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

/* 每次故障后检查结果所有权和 XRT 内存统计，随后直接重试。 */
static int scan_failures(xbytesview plain, xbytesview packed, bool decode)
{
    xmemdebugsnapshot before, after;
    unsigned int index, rejected = 0, succeeded = 0;
    xrtMemDebugSnapshot(&before);
    for (index = 0; index < 128u && succeeded < 2u; ++index) {
        xbuffer* output = NULL;
        xrtsnappyerror error;
        bool ok, injected;
        CHECK(xrtMemDebugFailAfter(index));
        ok = decode ? xrtSnappyDecompress(packed.Data, packed.Size, NULL, &output, &error) :
            xrtSnappyCompress(plain.Data, plain.Size, NULL, &output, &error);
        injected = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (injected) {
            CHECK(!ok && output == NULL && error.iCode == XRT_SNAPPY_ERROR_MEMORY);
            ++rejected;
        } else {
            CHECK(ok && output != NULL && error.iCode == XRT_SNAPPY_OK);
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
    xrtsnappyerror error;
    size_t i;
    CHECK(xrtMemDebugEnable(true));
    for (i = 0; i < sizeof(plain); ++i) plain[i] = (unsigned char)(i * 31u + i / 17u);
    CHECK(xrtSnappyCompress(plain, sizeof(plain), NULL, &packed, &error));
    CHECK(scan_failures((xbytesview){plain, sizeof(plain)}, xrtBufferView(packed), false) == 0);
    CHECK(scan_failures((xbytesview){plain, sizeof(plain)}, xrtBufferView(packed), true) == 0);
    xrtBufferDestroy(packed);
    puts("snappy allocation failures: PASS");
    return 0;
}
