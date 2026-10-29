#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE
#include "../include/xrt_snappy.h"
#include "../include/snappy_native.h"

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

/* 文本转移不复制已解出的字节，失败后内存回到输入流基线。 */
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
    CHECK(xrtSnappyCompress(plain, sizeof(plain), NULL, &packed, NULL));
    xrtMemDebugSnapshot(&packedBefore);
    raw = xsnappy_decompress_text(packed->Data, packed->Size, 1024, 1024, 1000);
    CHECK(raw != NULL && xsnappy_text_size(raw) == sizeof(plain));
    expected = ((xbuffer*)raw)->Data;
    text = (char*)xsnappy_text_take(raw);
    CHECK(text == expected && memcmp(text, plain, sizeof(plain)) == 0 && text[sizeof(plain)] == 0);
    xrtFree(text);

    for (index = 0; index < 128 && succeeded < 2; ++index) {
        bool injected;
        CHECK(xrtMemDebugFailAfter(index));
        raw = xsnappy_decompress_text(packed->Data, packed->Size, 1024, 1024, 1000);
        injected = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (injected) {
            CHECK(raw == NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            ++rejected;
        } else {
            CHECK(raw != NULL);
            xrtFree(xsnappy_text_take(raw));
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

    CHECK(xrtSnappyCompress(invalid, sizeof(invalid), NULL, &packed, NULL));
    CHECK(xsnappy_decompress_text(packed->Data, packed->Size, 1024, 1024, 1000) == NULL);
    CHECK(xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtBufferDestroy(packed);
    xrtClearError();
    CHECK(xrtSnappyCompress("", 0, NULL, &packed, NULL));
    raw = xsnappy_decompress_text(packed->Data, packed->Size, 1024, 1024, 1000);
    CHECK(raw != NULL && xsnappy_text_size(raw) == 0);
    text = (char*)xsnappy_text_take(raw);
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
    CHECK(text_transfer() == 0);
    puts("snappy allocation failures: PASS");
    return 0;
}
