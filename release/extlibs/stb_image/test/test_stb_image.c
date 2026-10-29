#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/stb_image_native.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "stb_image C test failed at line %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

int main(void)
{
    const char* Path = "release/examples/extlibs/assets/tiny.png";
    char* WorkDir = NULL;
    char* Utf8Path = NULL;
    char* MissingPath = NULL;
    size_t Size = 0u;
    bytes Encoded = xrtFileReadAllLimit(Path, 128u * 1024u * 1024u, &Size);
    void* FileImage;
    void* MemoryImage;
    void* FloatImage;
    void* Info;

    CHECK(Encoded != NULL && Size > 0u && xstbi_available());
    CHECK(strcmp(xstbi_version(), "stb_image 2.30") == 0);
    CHECK(xstbi_is_hdr(Path) == 0);
    Info = xstbi_info_json(Path);
    CHECK(Info != NULL && strstr((const char*)Info, "\"width\":16") != NULL);
    xrtFree(Info);

    FileImage = xstbi_load(Path, 4);
    MemoryImage = xstbi_load_memory(Encoded, Size, 4, 0);
    FloatImage = xstbi_load_memory(Encoded, Size, 4, 1);
    CHECK(FileImage != NULL && MemoryImage != NULL && FloatImage != NULL);
    CHECK(xstbi_image_valid(FileImage) && xstbi_image_width(FileImage) == 16 &&
        xstbi_image_height(FileImage) == 16 && xstbi_image_channels(FileImage) == 4 &&
        xstbi_image_byte_size(FileImage) == 1024u &&
        memcmp(xstbi_image_data(FileImage), xstbi_image_data(MemoryImage), 1024u) == 0);
    CHECK(xstbi_image_is_float(FloatImage) && xstbi_image_byte_size(FloatImage) == 4096u);
    xstbi_image_release(FloatImage);
    xstbi_image_release(MemoryImage);
    xstbi_image_release(FileImage);
    {
        int asFloat;
        for (asFloat = 0; asFloat <= 1; ++asFloat) {
            unsigned point;
            unsigned failures = 0u;
            unsigned successes = 0u;
            for (point = 0u; point < 64u; ++point) {
                int triggered;
                void* Image;
                CHECK(xrtMemDebugFailAfter(point));
                Image = xstbi_load_memory(Encoded, Size, 4, asFloat);
                triggered = xrtMemDebugFailTriggered();
                xrtMemDebugFailClear();
                if (triggered) {
                    if (Image == NULL) {
                        ++failures;
                        CHECK(xrtGetError() != NULL &&
                            xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                    } else {
                        ++successes;
                        CHECK(xrtGetError() == NULL);
                        xstbi_image_release(Image);
                    }
                } else {
                    ++successes;
                    CHECK(Image != NULL && xrtGetError() == NULL);
                    xstbi_image_release(Image);
                }
            }
            CHECK(failures >= 2u && successes != 0u);
        }
    }

    CHECK(xstbi_load_memory(NULL, 0u, 4, 0) == NULL &&
        xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    CHECK(xstbi_load_memory(Encoded, Size, 5, 0) == NULL &&
        xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    CHECK(xstbi_load_memory(Encoded, 128u * 1024u * 1024u + 1u, 4, 0) == NULL &&
        xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    CHECK(xstbi_load_memory((void*)"bad", 3u, 4, 0) == NULL &&
        xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    WorkDir = xrtDirTemp(NULL, "xlang_stbi_c_test_", NULL);
    CHECK(WorkDir != NULL);
    Utf8Path = xrtPathJoin(WorkDir, "图片_中文.png");
    MissingPath = xrtPathJoin(WorkDir, "missing.png");
    CHECK(Utf8Path != NULL && MissingPath != NULL);
    CHECK(xstbi_load(MissingPath, 4) == NULL && xrtGetError() != NULL);

    CHECK(xrtFileWriteAll(Utf8Path, (xbytesview){ Encoded, Size }));
    FileImage = xstbi_load(Utf8Path, 4);
    CHECK(FileImage != NULL && xstbi_image_width(FileImage) == 16);
    xstbi_image_release(FileImage);
    CHECK(xrtFileDelete(Utf8Path));
    CHECK(xrtDirRemove(WorkDir));
    xrtFree(MissingPath);
    xrtFree(Utf8Path);
    xrtFree(WorkDir);
    xrtFree(Encoded);
    puts("stb_image C boundaries: PASS");
    return 0;
}
