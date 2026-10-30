/* 最小长度按最后一行的有效像素计算，padding 不能参与滤波或被修改。 */
#define XRT_MODULE_MEMORY_DEBUG
#include "../stb_image_resize2_native.c"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned failures, layoutCases, constantCases, allocationFailures;

typedef struct {
    int width, height, channels, outWidth, outHeight, stride;
    size_t size;
} resize_shape;

static void check(bool condition, const char* message)
{
    if (!condition) {
        ++failures;
        fprintf(stderr, "resize stride: %s\n", message);
    }
}

static void layout_case(int width, int height, int channels, int outWidth, int outHeight,
    int padding, int linear)
{
    size_t row = (size_t)width * (size_t)channels;
    size_t stride = row + (size_t)padding;
    size_t size = (size_t)(height - 1) * stride + row;
    size_t packedSize = row * (size_t)height;
    /* CRT 分配让 ASan 能检查最后一行之后的越界读取，不依赖 XRT 池的红区。 */
    unsigned char* packed = malloc(packedSize);
    unsigned char* padded = malloc(size);
    if (!packed || !padded) { check(false, "cannot allocate test pixels"); free(packed); free(padded); return; }
    memset(padded, 0xa5, size);
    for (int y = 0; y < height; ++y) {
        for (size_t x = 0; x < row; ++x)
            packed[(size_t)y * row + x] = (unsigned char)((x * 31u + (size_t)y * 19u) & 255u);
        memcpy(padded + (size_t)y * stride, packed + (size_t)y * row, row);
    }
    xbuffer* reference = xstbir_resize(packed, packedSize, width, height, channels, outWidth, outHeight, 0, linear);
    xbuffer* result = xstbir_resize(padded, size, width, height, channels, outWidth, outHeight, (int)stride, linear);
    size_t expected = (size_t)outWidth * (size_t)outHeight * (size_t)channels;
    check(reference && result && reference->Size == expected && result->Size == expected &&
        memcmp(reference->Data, result->Data, expected) == 0, "padding changed the resized pixels");
    check(result && result->Data != packed && result->Data != padded &&
        reference && reference->Data != result->Data, "resize results must be independently owned");
    for (int y = 0; y < height; ++y) {
        for (size_t x = 0; x < row; ++x) check(packed[(size_t)y * row + x] ==
            (unsigned char)((x * 31u + (size_t)y * 19u) & 255u), "packed source was modified");
        check(memcmp(padded + (size_t)y * stride, packed + (size_t)y * row, row) == 0,
            "padded source pixels were modified");
        if (y + 1 < height) for (size_t x = row; x < stride; ++x)
            check(padded[(size_t)y * stride + x] == 0xa5, "source padding was modified");
    }
    xbuffer* shortResult = xstbir_resize(padded, size - 1u, width, height, channels,
        outWidth, outHeight, (int)stride, linear);
    check(!shortResult && xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_RANGE,
        "one missing source byte must be rejected before sampling");
    xrtBufferDestroy(shortResult);
    xbuffer* recovered = xstbir_resize(padded, size, width, height, channels,
        outWidth, outHeight, (int)stride, linear);
    check(recovered && !xrtGetError() && reference && recovered->Size == expected &&
        memcmp(recovered->Data, reference->Data, expected) == 0, "resize did not recover after a short input");
    free(packed);
    free(padded);
    check(reference && result && reference->Size == expected && result->Size == expected &&
        memcmp(reference->Data, result->Data, expected) == 0,
        "output borrowed the released source");
    xrtBufferDestroy(recovered);
    xrtBufferDestroy(result);
    xrtBufferDestroy(reference);
    ++layoutCases;
}

static void test_constants(void)
{
    for (int channels = 1; channels <= 4; ++channels) {
        unsigned char pixel[4] = {37, 111, 213, 255};
        if (channels == 2) pixel[1] = 255;
        for (int linear = 0; linear < 2; ++linear) {
            for (int explicitStride = 0; explicitStride < 2; ++explicitStride) {
                xbuffer* result = xstbir_resize(pixel, (size_t)channels, 1, 1, channels,
                    5, 3, explicitStride ? INT_MAX : 0, linear);
                check(result && result->Size == 15u * (size_t)channels,
                    "one source row must not require unused trailing padding");
                if (result) for (size_t i = 0; i < result->Size; ++i)
                    check(result->Data[i] == pixel[i % (size_t)channels], "opaque constant pixels changed");
                xrtBufferDestroy(result);
                ++constantCases;
            }
        }
    }
}

static void test_allocation_recovery(void)
{
    static const resize_shape shapes[] = {
        {7, 5, 4, 3, 2, 35, 168}, {7, 7, 4, 3, 3, 35, 238},
        {3, 2, 2, 7, 5, 11, 17}, {2, 31, 1, 31, 2, 9, 272}
    };
    unsigned char pixels[512];
    memset(pixels, 127, sizeof(pixels));
    /* 四种缩放布局分别测试 sRGB 与线性模式。 */
    for (unsigned caseIndex = 0; caseIndex < 8; ++caseIndex) {
        unsigned shape = caseIndex / 2;
        int linear = (int)(caseIndex % 2);
        const resize_shape* s = &shapes[shape];
        unsigned budget;
        for (budget = 0; budget < 64; ++budget) {
            xmemdebugsnapshot before, after;
            xrtClearError();
            xrtMemDebugSnapshot(&before);
            check(xrtMemDebugFailAfter(budget), "cannot inject allocation failure");
            xbuffer* result = xstbir_resize(pixels, s->size, s->width, s->height, s->channels,
                s->outWidth, s->outHeight, s->stride, linear);
            bool triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (triggered) {
                ++allocationFailures;
                check(!result && xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY,
                    "workspace failure lost its memory error");
            } else check(result && !xrtGetError(), "resize failed without an injected fault");
            xrtBufferDestroy(result);
            xrtClearError();
            if (triggered) {
                result = xstbir_resize(pixels, s->size, s->width, s->height, s->channels,
                    s->outWidth, s->outHeight, s->stride, linear);
                check(result && result->Size == (size_t)s->outWidth * (size_t)s->outHeight * (size_t)s->channels && !xrtGetError(),
                    "resize did not recover immediately after OOM");
                xrtBufferDestroy(result);
            }
            xrtClearError();
            xrtMemDebugSnapshot(&after);
            bool balanced = before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
                before.InvalidFreeCount == after.InvalidFreeCount && before.UseAfterFreeCount == after.UseAfterFreeCount &&
                before.OverflowCount == after.OverflowCount && before.UnderflowCount == after.UnderflowCount;
            if (!balanced) fprintf(stderr, "resize stride allocation shape=%u mode=%d budget=%u live=%lld bytes=%lld\n",
                shape, linear, budget, (long long)after.LiveCount - (long long)before.LiveCount,
                (long long)after.LiveBytes - (long long)before.LiveBytes);
            check(balanced, "allocation failure or recovery damaged live memory");
            if (!triggered) break;
        }
        check(budget && budget < 64, "allocation sweep did not reach normal completion");
    }
}

int main(void)
{
    static const int widths[] = {1, 2, 3, 7, 17}, heights[] = {1, 3, 2, 5, 9};
    static const int pads[] = {1, 7, 17};
    for (unsigned shape = 0; shape < 5; ++shape) {
        int width = widths[shape], height = heights[shape];
        for (int channels = 1; channels <= 4; ++channels) {
            for (int linear = 0; linear < 2; ++linear) {
                for (unsigned padding = 0; padding < 3; ++padding) {
                    layout_case(width, height, channels, width, height, pads[padding], linear);
                    layout_case(width, height, channels, width + 2, height + 1, pads[padding], linear);
                    layout_case(width, height, channels, (width + 1) / 2, (height + 1) / 2, pads[padding], linear);
                }
            }
        }
    }
    test_constants();
    test_allocation_recovery();
    if (!failures) printf("stb resize stride: %u layouts, %u constants, %u OOM points PASS\n",
        layoutCases, constantCases, allocationFailures);
    return failures ? 1 : 0;
}
