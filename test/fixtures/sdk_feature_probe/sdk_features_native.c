/* 模拟宿主先注入完整声明，原生扩展随后才选择自己的模块。 */
#include <__xl_runtime/xruntime_decl.h>

#ifndef XRT_MODULE_CODEC_PERCENT
#define XRT_MODULE_CODEC_PERCENT
#endif
#ifndef XRT_MODULE_DEFLATE
#define XRT_MODULE_DEFLATE
#endif
#ifndef XRT_MODULE_INFLATE
#define XRT_MODULE_INFLATE
#endif
#ifndef XRUNTIME_MODULE_TYPED_ARRAY
#define XRUNTIME_MODULE_TYPED_ARRAY
#endif
#include <__xl_runtime/xruntime_decl.h>
#include <xrt.h>
#include <xruntime.h>
#include <string.h>

#if !defined(XRT_FEATURE_CODEC_PERCENT) || !defined(XRT_FEATURE_DEFLATE) || \
    !defined(XRT_FEATURE_INFLATE) || !defined(XRUNTIME_FEATURE_TYPED_ARRAY)
#error "requested SDK feature closure is missing"
#endif

int xl_sdk_feature_probe(void)
{
    const uint8 input[] = {0xe4, 0xbd, 0xa0, 0, 0xff, ' ', 'x'};
    char encoded[64];
    uint8 decoded[sizeof(input)];
    size_t encodedSize = 0, decodedSize = 0, compressedSize = 0, restoredSize = 0;
    bytes compressed = NULL, restored = NULL;
    xdeflateconfig deflateConfig;
    xinflateconfig inflateConfig;
    xtypedarray* array = NULL;
    int64 number = INT64_C(1234567890123);
    int result = 1;

    /* 实际调用宿主符号，不能仅凭 FEATURE 宏推断运行库具备能力。 */
    if (!xrtPercentEncode(input, sizeof(input), (xstrview){NULL, 0},
            encoded, sizeof(encoded), &encodedSize)) goto done;
    if (strcmp(encoded, "%E4%BD%A0%00%FF%20x") != 0) goto done;
    if (!xrtPercentDecode((xstrview){encoded, encodedSize}, decoded,
            sizeof(decoded), &decodedSize)) goto done;
    if (decodedSize != sizeof(input) || memcmp(input, decoded, sizeof(input))) goto done;

    xrtDeflateConfigInit(&deflateConfig);
    xrtInflateConfigInit(&inflateConfig);
    deflateConfig.Format = XDEFLATE_ZLIB;
    inflateConfig.Format = XINFLATE_ZLIB;
    compressed = xrtDeflateAll((xbytesview){input, sizeof(input)}, &deflateConfig, &compressedSize);
    if (compressed == NULL) goto done;
    restored = xrtInflateAll((xbytesview){compressed, compressedSize}, &inflateConfig, &restoredSize);
    if (restored == NULL || restoredSize != sizeof(input) ||
            memcmp(input, restored, sizeof(input))) goto done;

    array = xrtTypedArrayCreate(xrtTypeInt64());
    if (array == NULL || !xrtTypedArrayPush(array, &number) ||
            xrtTypedArrayCount(array) != 1) goto done;
    if (*(const int64*)xrtTypedArrayConstGet(array, 0) != number) goto done;
    result = 0;

done:
    xrtTypedArrayDestroy(array);
    xrtFree(restored);
    xrtFree(compressed);
    return result;
}
