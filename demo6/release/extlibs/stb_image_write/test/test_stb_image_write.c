#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/stb_image_write_native.h"
#include "../../stb_image/include/stb_image_native.h"

#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <signal.h>
#include <sys/resource.h>
#endif

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "stb_image_write C test failed at line %d: %s\n", \
        __LINE__, #expr); return 2; \
} } while (0)

static int test_output_limit(void)
{
    size_t size = 8192u * 8192u;
    unsigned char* pixels = xrtCalloc(size, 1u);
    xbuffer* encoded;
    CHECK(pixels != NULL);
    /* 灰度 BMP 写成 24 位 RGB：64 MiB 输入会超过 128 MiB 输出限额。 */
    encoded = (xbuffer*)xstbiw_encode(XSTBIW_BMP, pixels, size,
        8192, 8192, 1, 0);
    CHECK(encoded == NULL && xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtFree(pixels);
    xrtClearError();
    return 0;
}

#ifndef _WIN32
static int test_failed_write_preserves_target(const char* Path,
    const unsigned char* Pixels, size_t Length)
{
    static const unsigned char Old[] = "keep old";
    struct rlimit saved, limited;
    void (*handler)(int);
    size_t size = 0u;
    bytes data;
    int written;
    CHECK(xrtFileWriteAll(Path, (xbytesview){Old, sizeof(Old) - 1u}));
    CHECK(getrlimit(RLIMIT_FSIZE, &saved) == 0 && saved.rlim_cur > 4u);
    limited = saved;
    limited.rlim_cur = 4u;
    handler = signal(SIGXFSZ, SIG_IGN);
    CHECK(handler != SIG_ERR && setrlimit(RLIMIT_FSIZE, &limited) == 0);
    written = xstbiw_write(Path, XSTBIW_PNG, (void*)Pixels,
        Length, 2, 2, 4, 0);
    CHECK(setrlimit(RLIMIT_FSIZE, &saved) == 0);
    CHECK(signal(SIGXFSZ, handler) != SIG_ERR);
    CHECK(written == 0 && xrtGetError() != NULL);
    data = xrtFileReadAllLimit(Path, 64u, &size);
    CHECK(data != NULL && size == sizeof(Old) - 1u &&
        memcmp(data, Old, size) == 0);
    xrtFree(data);
    CHECK(xrtFileDelete(Path));
    return 0;
}
#endif

int main(void)
{
    char* WorkDir = xrtDirTemp(NULL, "xlang_stbiw_test_", NULL);
    char* Path;
    unsigned char Pixels[16] = {
        255, 0, 0, 255, 0, 255, 0, 255,
        0, 0, 255, 255, 255, 255, 255, 255
    };
    float FloatPixels[16] = {
        1, 0, 0, 1, 0, 1, 0, 1,
        0, 0, 1, 1, 1, 1, 1, 1
    };
    int Format;
    xbuffer* Encoded;
    void* Decoded;
    size_t FileSize = 0u;
    bytes FileData;

    CHECK(WorkDir != NULL);
    Path = xrtPathJoin(WorkDir, "编码_临时.png");
    CHECK(Path != NULL);
    CHECK(xstbiw_available() == 1 && xstbiw_version()[0] != '\0');
    for (Format = XSTBIW_PNG; Format <= XSTBIW_HDR; ++Format) {
        void* Source = Format == XSTBIW_HDR ? (void*)FloatPixels : (void*)Pixels;
        size_t Length = Format == XSTBIW_HDR ? sizeof(FloatPixels) : sizeof(Pixels);
        int Option = Format == XSTBIW_JPEG ? 90 : 0;
        Encoded = (xbuffer*)xstbiw_encode(Format, Source,
            Length, 2, 2, 4, Option);
        CHECK(Encoded != NULL && Encoded->Size > 8u);
        Decoded = xstbi_load_memory(Encoded->Data, Encoded->Size,
            4, Format == XSTBIW_HDR);
        CHECK(Decoded != NULL && xstbi_image_width(Decoded) == 2 &&
            xstbi_image_height(Decoded) == 2);
        if (Format != XSTBIW_JPEG && Format != XSTBIW_HDR)
            CHECK(memcmp(xstbi_image_data(Decoded), Pixels, sizeof(Pixels)) == 0);
        xstbi_image_release(Decoded);
        xrtBufferDestroy(Encoded);
    }

    Encoded = (xbuffer*)xstbiw_encode(XSTBIW_PNG, Pixels,
        sizeof(Pixels), 2, 2, 4, 0);
    CHECK(Encoded != NULL);
    CHECK(xstbiw_write(Path, XSTBIW_PNG, Pixels, sizeof(Pixels),
        2, 2, 4, 0) == 1);
    FileData = xrtFileReadAllLimit(Path, 1024u * 1024u, &FileSize);
    CHECK(FileData != NULL && FileSize == Encoded->Size &&
        memcmp(FileData, Encoded->Data, FileSize) == 0);
    xrtFree(FileData);
    xrtBufferDestroy(Encoded);
    CHECK(xrtFileDelete(Path));
#ifndef _WIN32
    CHECK(test_failed_write_preserves_target(Path, Pixels, sizeof(Pixels)) == 0);
#endif

    CHECK(xstbiw_encode(XSTBIW_PNG, Pixels, 15u, 2, 2, 4, 0) == NULL);
    CHECK(xrtGetError() != NULL);
    Encoded = (xbuffer*)xstbiw_encode(XSTBIW_PNG, Pixels,
        sizeof(Pixels), 2, 2, 4, 0);
    CHECK(Encoded != NULL && xrtGetError() == NULL);
    xrtBufferDestroy(Encoded);
    CHECK(xstbiw_encode(XSTBIW_PNG, Pixels, 16u, 2, 2, 4, 7) == NULL);
    CHECK(xstbiw_encode(XSTBIW_JPEG, Pixels, 16u, 2, 2, 4, 0) == NULL);
    CHECK(xstbiw_encode(XSTBIW_PNG, Pixels, 16u, 0, 2, 4, 0) == NULL);
    CHECK(xstbiw_encode(XSTBIW_PNG, Pixels, 16u, 16384, 16384, 4, 0) == NULL);
    CHECK(xstbiw_encode(99, Pixels, 16u, 2, 2, 4, 0) == NULL);
    CHECK(xstbiw_write("", XSTBIW_PNG, Pixels, 16u, 2, 2, 4, 0) == 0);
    CHECK(xrtGetError() != NULL);
    CHECK(xstbiw_write(Path, XSTBIW_PNG, Pixels, sizeof(Pixels),
        2, 2, 4, 0) == 1 && xrtGetError() == NULL);
    CHECK(xrtFileDelete(Path));
    for (Format = XSTBIW_PNG; Format <= XSTBIW_HDR; ++Format) {
        void* source = Format == XSTBIW_HDR ? (void*)FloatPixels : (void*)Pixels;
        size_t length = Format == XSTBIW_HDR ? sizeof(FloatPixels) : sizeof(Pixels);
        int option = Format == XSTBIW_JPEG ? 90 : 0;
        xbuffer* expected = (xbuffer*)xstbiw_encode(Format, source,
            length, 2, 2, 4, option);
        unsigned point, failures = 0u, successes = 0u;
        CHECK(expected != NULL);
        for (point = 0u; point < 64u; ++point) {
            int triggered;
            CHECK(xrtMemDebugFailAfter(point));
            Encoded = (xbuffer*)xstbiw_encode(Format, source,
                length, 2, 2, 4, option);
            triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (Encoded == NULL) {
                CHECK(triggered && xrtGetError() != NULL &&
                    xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                ++failures;
            } else {
                CHECK(Encoded->Size == expected->Size &&
                    memcmp(Encoded->Data, expected->Data, expected->Size) == 0 &&
                    xrtGetError() == NULL);
                xrtBufferDestroy(Encoded);
                ++successes;
            }
        }
        CHECK(failures != 0u && successes != 0u);
        xrtBufferDestroy(expected);
    }
    CHECK(test_output_limit() == 0);
    CHECK(xrtDirRemove(WorkDir));
    xrtFree(Path);
    xrtFree(WorkDir);
    puts("stb_image_write C boundaries: PASS");
    return 0;
}
