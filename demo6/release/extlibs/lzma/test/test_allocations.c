#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE
#include "../include/xrt_lzma.h"
#include "../include/lzma_native.h"

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

/* 解码后的文本直接接管缓冲；失败不能遗留 XRT 分配。 */
static int text_transfer(void)
{
    const unsigned char plain[] = {'A', 0, 'B', 0xe4, 0xbd, 0xa0, 0xe5, 0xa5, 0xbd};
    const unsigned char invalid[] = {0xff};
    xmemdebugsnapshot before, packedBefore, after;
    xbuffer* packed = NULL;
    void* raw;
    void* expected;
    char* text;
    unsigned int index, rejected = 0, succeeded = 0;
    xrtMemDebugSnapshot(&before);
    CHECK(xrtLzmaCompress(plain, sizeof(plain), NULL, &packed, NULL));
    xrtMemDebugSnapshot(&packedBefore);
    raw = xlzma_decompress_text(packed->Data, packed->Size, 1024, 1024, 1000);
    CHECK(raw != NULL && xlzma_text_size(raw) == sizeof(plain));
    expected = ((xbuffer*)raw)->Data;
    text = (char*)xlzma_text_take(raw);
    CHECK(text == expected && memcmp(text, plain, sizeof(plain)) == 0 && text[sizeof(plain)] == 0);
    xrtFree(text);

    for (index = 0; index < 256 && succeeded < 2; ++index) {
        bool injected;
        CHECK(xrtMemDebugFailAfter(index));
        raw = xlzma_decompress_text(packed->Data, packed->Size, 1024, 1024, 1000);
        injected = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (injected) {
            CHECK(raw == NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            ++rejected;
        } else {
            CHECK(raw != NULL);
            xrtFree(xlzma_text_take(raw));
            ++succeeded;
        }
        xrtClearError();
        xrtMemDebugSnapshot(&after);
        CHECK(after.LiveCount == packedBefore.LiveCount && after.LiveBytes == packedBefore.LiveBytes);
        CHECK(after.InvalidFreeCount == packedBefore.InvalidFreeCount &&
            after.DoubleFreeCount == packedBefore.DoubleFreeCount &&
            after.OverflowCount == packedBefore.OverflowCount &&
            after.UnderflowCount == packedBefore.UnderflowCount);
    }
    CHECK(rejected > 0 && succeeded == 2);
    xrtBufferDestroy(packed);
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    CHECK(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes);

    CHECK(xrtLzmaCompress(invalid, sizeof(invalid), NULL, &packed, NULL));
    CHECK(xlzma_decompress_text(packed->Data, packed->Size, 1024, 1024, 1000) == NULL);
    CHECK(xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtBufferDestroy(packed);
    xrtClearError();
    CHECK(xrtLzmaCompress("", 0, NULL, &packed, NULL));
    raw = xlzma_decompress_text(packed->Data, packed->Size, 1024, 1024, 1000);
    CHECK(raw != NULL && xlzma_text_size(raw) == 0);
    text = (char*)xlzma_text_take(raw);
    CHECK(text != NULL && text[0] == 0);
    xrtFree(text);
    xrtBufferDestroy(packed);
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    CHECK(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes);
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
    CHECK(text_transfer() == 0);
    puts("lzma allocation failures: PASS");
    return 0;
}
