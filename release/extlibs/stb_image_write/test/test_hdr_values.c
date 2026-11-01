#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#define XRT_MODULE_DIR_TEMP
#define XRT_MODULE_STRING_FORMAT
#ifndef XSTBIW_TEST_NATIVE
#define XSTBIW_TEST_NATIVE "../stb_image_write_native.c"
#endif
#include XSTBIW_TEST_NATIVE
#include <string.h>

static unsigned failures, invalidCases, validCases, faults, diagnosticFaults;

static void check(bool ok, cstr message)
{
    if (!ok) {
        if (failures < 16u) fprintf(stderr, "HDR values: %s\n", message);
        ++failures;
    }
}

static void set_bits(float* target, uint32 bits)
{
    _Static_assert(sizeof(float) == sizeof(uint32), "fixture needs float32");
    memcpy(target, &bits, sizeof(bits));
}

static bool balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    return before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes &&
        before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount &&
        before->OverflowCount == after.OverflowCount && before->UnderflowCount == after.UnderflowCount &&
        before->UseAfterFreeCount == after.UseAfterFreeCount;
}

/* 错误数据在输出分配前拒绝；另区分诊断对象本身的OOM，不虚称错误通道零分配。 */
static void invalid_values(void)
{
    static const uint32 bad[] = {0xbf800000u, 0x7fc00000u, 0x7f800000u,
        0xff800000u, 0x7f800001u, 0x7f000000u, 0x7f7fffffu};
    static const int widths[] = {1, 7, 8, 129};
    float pixels[2 * 129 * 4], saved[2 * 129 * 4];
    for (size_t w = 0; w < sizeof(widths) / sizeof(widths[0]); ++w) {
        for (int channels = 1; channels <= 4; ++channels) {
            size_t count = (size_t)widths[w] * 2u * (size_t)channels;
            for (int color = 0; color < (channels < 3 ? 1 : 3); ++color) {
                for (unsigned position = 0; position < 2u; ++position) {
                    for (unsigned b = 0; b < sizeof(bad) / sizeof(bad[0]); ++b) {
                        for (size_t i = 0; i < count; ++i) pixels[i] = 1.0f;
                        size_t pixel = position == 0u ? 0u : count - (size_t)channels;
                        set_bits(pixels + pixel + (size_t)color, bad[b]);
                        memcpy(saved, pixels, count * sizeof(float));
                        xerrkind expected = b < 5u ? XERR_VALUE : XERR_RANGE;
                        ++invalidCases;
                        for (unsigned inject = 0; inject < 3u; ++inject) {
                            xmemdebugsnapshot before;
                            xrtClearError();
                            xrtMemDebugSnapshot(&before);
                            if (inject) check(xrtMemDebugFailAfter(inject == 1u ? 0u : 4u), "fault setup");
                            xbuffer* result = xstbiw_encode(XSTBIW_HDR, pixels,
                                count * sizeof(float), widths[w], 2, channels, 0);
                            bool triggered = inject && xrtMemDebugFailTriggered();
                            xrtMemDebugFailClear();
                            if (inject == 1u && triggered) ++diagnosticFaults;
                            check(result == NULL && (inject == 1u ? triggered : !triggered) &&
                                xrtErrorKind(xrtGetError()) == (inject == 1u ? XERR_MEMORY : expected),
                                "invalid color was encoded, misclassified, or allocated output");
                            check(memcmp(saved, pixels, count * sizeof(float)) == 0, "source changed");
                            xrtBufferDestroy(result);
                            check(balanced(&before), "invalid color memory balance");
                        }
                    }
                }
            }
        }
    }
    float aligned[] = {1, 1, 1, 1};
    xbuffer* result = xstbiw_encode(XSTBIW_HDR, (unsigned char*)aligned + 1,
        12u, 1, 1, 3, 0);
    check(result == NULL && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "unaligned HDR pointer");
    xrtBufferDestroy(result);
}

/* 有效输入包括-0/次正规数/指数上边界；被忽略的alpha刻意放NaN。 */
static void valid_values(cstr corpus)
{
    static const uint32 good[] = {0u, 0x80000000u, 1u, 0x1e3ce508u, 0x3f800000u, 0x7effffffu};
    static const int widths[] = {1, 7, 8, 129};
    float pixels[2 * 129 * 4], saved[2 * 129 * 4];
    for (size_t w = 0; w < sizeof(widths) / sizeof(widths[0]); ++w) {
        for (int channels = 1; channels <= 4; ++channels) {
            size_t count = (size_t)widths[w] * 2u * (size_t)channels;
            for (unsigned g = 0; g < sizeof(good) / sizeof(good[0]); ++g) {
                for (size_t i = 0; i < count; ++i) {
                    set_bits(pixels + i, (channels == 2 || channels == 4) &&
                        i % (size_t)channels == (size_t)channels - 1u ? 0x7fc00000u : good[g]);
                }
                memcpy(saved, pixels, count * sizeof(float));
                xbuffer* result = xstbiw_encode(XSTBIW_HDR, pixels, count * sizeof(float), widths[w], 2, channels, 0);
                check(result != NULL && xrtGetError() == NULL, "valid HDR color rejected");
                check(memcmp(saved, pixels, count * sizeof(float)) == 0, "valid source changed");
                ++validCases;
                if (result != NULL && corpus != NULL) {
                    str path = xrtFormat("%s/w%d-c%d-b%08x.hdr", corpus, widths[w], channels, good[g]);
                    check(path != NULL && xrtFileWriteAll(path, xrtBufferView(result)), "write reference corpus");
                    xrtFree(path);
                }
                xrtBufferDestroy(result);
            }
        }
    }
}

static void allocation_sweep(void)
{
    float pixels[8 * 4];
    for (size_t i = 0; i < sizeof(pixels) / sizeof(pixels[0]); ++i) pixels[i] = 1.0f;
    for (int width = 1; width <= 8; width += 7) {
        for (int channels = 1; channels <= 4; ++channels) {
            bool complete = false;
            for (unsigned budget = 0; budget < 64u; ++budget) {
                xmemdebugsnapshot before;
                xrtClearError();
                xrtMemDebugSnapshot(&before);
                check(xrtMemDebugFailAfter(budget), "fault setup");
                xbuffer* result = xstbiw_encode(XSTBIW_HDR, pixels,
                    (size_t)width * (size_t)channels * sizeof(float), width, 1, channels, 0);
                bool triggered = xrtMemDebugFailTriggered();
                xrtMemDebugFailClear();
                check(triggered ? result == NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY :
                    result != NULL && xrtGetError() == NULL, "valid encoding OOM contract");
                xrtBufferDestroy(result);
                if (triggered) ++faults;
                result = xstbiw_encode(XSTBIW_HDR, pixels,
                    (size_t)width * (size_t)channels * sizeof(float), width, 1, channels, 0);
                check(result != NULL && xrtGetError() == NULL, "immediate OOM recovery");
                xrtBufferDestroy(result);
                check(balanced(&before), "valid OOM memory balance");
                if (!triggered) { complete = budget > 0u; break; }
            }
            check(complete, "allocation sweep did not complete");
        }
    }
}

static void file_preservation(void)
{
    str directory = xrtDirTemp(NULL, "stbiw-hdr-", NULL);
    str path = directory != NULL ? xrtPathJoin(directory, "HDR_边界.hdr") : NULL;
    check(path != NULL, "temporary path");
    if (path != NULL) {
        float pixels[] = {-1, 1, 1};
        check(xrtFileWriteAll(path, (xbytesview){(cbytes)"keep", 4u}), "old target setup");
        check(!xstbiw_write(path, XSTBIW_HDR, pixels, sizeof(pixels), 1, 1, 3, 0) &&
            xrtErrorKind(xrtGetError()) == XERR_VALUE, "invalid write accepted");
        size_t length = 0;
        bytes old = xrtFileReadAllLimit(path, 16u, &length);
        check(old != NULL && length == 4u && memcmp(old, "keep", 4u) == 0, "old target changed");
        xrtFree(old);
        check(xrtFileDelete(path), "target cleanup");
    }
    xrtFree(path);
    if (directory != NULL) check(xrtDirRemove(directory), "directory cleanup");
    xrtFree(directory);
}

int main(int argc, char** argv)
{
    if (!xrtMemDebugEnabled() || !xrtMemDebugFailAfter(UINT64_MAX)) return 1;
    xrtMemDebugFailClear();
    xrtSetErrorInfo(XERR_MEMORY, "test.hdr", 0, "warm error slot");
    invalid_values();
    valid_values(argc > 1 ? argv[1] : NULL);
    allocation_sweep();
    file_preservation();
    printf("HDR values: %u invalid cases, %u valid cases, %u output faults, %u diagnostic faults, %u failures\n",
        invalidCases, validCases, faults, diagnosticFaults, failures);
    return failures == 0u ? 0 : 1;
}
