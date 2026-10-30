/* BMP 输出大小能在采样前确定，不能把确定超限误报成分配失败。 */
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_DIR_TEMP
#include "../stb_image_write_native.c"
#include <string.h>

static unsigned failures, headers, preflightCases, oomPoints;

static void check(bool condition, const char* message)
{
    if (!condition) { ++failures; fprintf(stderr, "BMP budget: %s\n", message); }
}

static uint32 read_le32(const unsigned char* bytes)
{
    return (uint32)bytes[0] | (uint32)bytes[1] << 8 | (uint32)bytes[2] << 16 | (uint32)bytes[3] << 24;
}

static void test_headers(void)
{
    unsigned char pixels[9 * 3 * 4];
    for (size_t i = 0; i < sizeof(pixels); ++i) pixels[i] = (unsigned char)(i * 37u + 11u);
    for (int width = 1; width <= 9; ++width) {
        for (int height = 1; height <= 3; ++height) {
            for (int channels = 1; channels <= 4; ++channels) {
                size_t header = channels == 4 ? 122u : 54u;
                size_t row = ((size_t)width * (channels == 4 ? 4u : 3u) + 3u) & ~(size_t)3u;
                size_t expected = header + row * (size_t)height;
                xbuffer* encoded = xstbiw_encode(XSTBIW_BMP, pixels,
                    (size_t)width * (size_t)height * (size_t)channels, width, height, channels, 0);
                check(encoded && !xrtGetError() && encoded->Size == expected &&
                    encoded->Data[0] == 'B' && encoded->Data[1] == 'M' &&
                    read_le32(encoded->Data + 2) == expected && read_le32(encoded->Data + 10) == header,
                    "BMP header, channel expansion or four-byte row alignment differs");
                xrtBufferDestroy(encoded);
                ++headers;
            }
        }
    }
}

static void test_preflight(void)
{
    const size_t size = 64u * 1024u * 1024u;
    unsigned char* pixels = xrtCalloc(size, 1u);
    char* directory = xrtDirTemp(NULL, "xlang-bmp-budget-", "");
    char* path = directory ? xrtPathJoin(directory, "preserved.bmp") : NULL;
    static const unsigned char old[] = "keep old";
    bool prepared = pixels && path && xrtFileWriteAll(path, (xbytesview){old, sizeof(old) - 1});
    check(prepared, "cannot prepare quota fixture");
    if (!prepared) goto done;
    for (unsigned shape = 0; shape < 2; ++shape) {
        int height = shape ? 5462 : 8192;
        for (unsigned budget = 4; budget <= 8; budget += 4) {
            for (unsigned write = 0; write < 2; ++write) {
                xmemdebugsnapshot before, after;
                xrtClearError();
                xrtMemDebugSnapshot(&before);
                check(xrtMemDebugFailAfter(budget), "cannot inject quota allocation budget");
                xbuffer* output = NULL;
                int written = 0;
                if (write) written = xstbiw_write(path, XSTBIW_BMP, pixels, size, 8192, height, 1, 0);
                else output = xstbiw_encode(XSTBIW_BMP, pixels, size, 8192, height, 1, 0);
                bool triggered = xrtMemDebugFailTriggered();
                xrtMemDebugFailClear();
                check(!output && !written && !triggered && xrtGetError() &&
                    xrtErrorKind(xrtGetError()) == XERR_RANGE,
                    "known output overflow allocated image data or became a memory error");
                xrtBufferDestroy(output);
                xrtClearError();
                if (write) {
                    size_t length = 0;
                    bytes stored = xrtFileReadAllLimit(path, 64u, &length);
                    check(stored && length == sizeof(old) - 1 && memcmp(stored, old, length) == 0,
                        "rejected encoding modified the target file");
                    xrtFree(stored);
                }
                unsigned char white = 255;
                output = xstbiw_encode(XSTBIW_BMP, &white, 1, 1, 1, 1, 0);
                check(output && output->Size == 58u && !xrtGetError(), "encoding did not recover after rejection");
                xrtBufferDestroy(output);
                xrtClearError();
                xrtMemDebugSnapshot(&after);
                check(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
                    before.InvalidFreeCount == after.InvalidFreeCount && before.UseAfterFreeCount == after.UseAfterFreeCount,
                    "quota rejection or recovery leaked memory");
                ++preflightCases;
            }
        }
    }
done:
    if (prepared) check(xrtFileDelete(path), "cannot remove quota target");
    if (directory) check(xrtDirRemove(directory), "cannot remove quota directory");
    xrtFree(path);
    xrtFree(directory);
    xrtFree(pixels);
    xrtClearError();
}

static void test_real_oom(void)
{
    unsigned char white = 255;
    unsigned budget;
    for (budget = 0; budget < 32; ++budget) {
        xmemdebugsnapshot before, after;
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        check(xrtMemDebugFailAfter(budget), "cannot inject small BMP allocation failure");
        xbuffer* output = xstbiw_encode(XSTBIW_BMP, &white, 1, 1, 1, 1, 0);
        bool triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            ++oomPoints;
            check(!output && xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY,
                "a real allocation failure must remain a memory error");
        } else check(output && output->Size == 58 && !xrtGetError(), "valid small BMP did not complete");
        xrtBufferDestroy(output);
        xrtClearError();
        output = xstbiw_encode(XSTBIW_BMP, &white, 1, 1, 1, 1, 0);
        check(output && output->Size == 58 && !xrtGetError() && white == 255,
            "valid encoding did not recover after allocation failure");
        xrtBufferDestroy(output);
        xrtClearError();
        xrtMemDebugSnapshot(&after);
        check(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
            before.InvalidFreeCount == after.InvalidFreeCount && before.UseAfterFreeCount == after.UseAfterFreeCount,
            "real allocation failure or recovery leaked memory");
        if (!triggered) break;
    }
    check(oomPoints && budget < 32, "small BMP allocation sweep never completed");
}

int main(void)
{
    test_headers();
    test_preflight();
    test_real_oom();
    if (!failures) printf("stb write BMP budgets: %u headers, %u preflight cases, %u OOM points PASS\n",
        headers, preflightCases, oomPoints);
    return failures ? 1 : 0;
}
