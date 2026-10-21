#include "imagehash_native.h"
#ifndef XRT_MODULE_DIR_TEMP
#define XRT_MODULE_DIR_TEMP
#endif
#define XRT_MODULE_FILE_WHOLE
#ifndef XRT_MODULE_PATH
#define XRT_MODULE_PATH
#endif
#include "xrt.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "imagehash C test failed at %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

int main(void) {
    const char *path = "release/examples/extlibs/assets/tiny.png";
    char *a = xihash_ahash(path), *d = xihash_dhash(path), *p = xihash_phash(path);
    CHECK(xihash_available() && a != NULL && d != NULL && p != NULL);
    CHECK(a != d && a != p && d != p);
    CHECK(xihash_valid(a, strlen(a)) && xihash_valid(d, strlen(d)) &&
        xihash_valid(p, strlen(p)));
    CHECK(strcmp(a, "ffffffffffffffff") == 0);
    CHECK(strcmp(d, "0000000000000000") == 0);
    CHECK(strcmp(p, "0000000000000000") == 0);
    CHECK(xihash_hamming(a, 16, a, 16) == 0);
    CHECK(xihash_hamming("0000000000000000", 16,
        "FFFFFFFFFFFFFFFF", 16) == 64);
    CHECK(xihash_hamming("0000000000000001", 16,
        "0000000000000000", 16) == 1);
    CHECK(xihash_hamming("0123456789abcdef", 16,
        "0123456789ABCDEF", 16) == 0);
    CHECK(xihash_hamming(a, 16, d, 16) >= 0);
    CHECK(xihash_hamming(a, 16, p, 16) >= 0);
    CHECK(!xihash_valid("0123456789abcdefx", 17));
    CHECK(!xihash_valid("0123456789abcde", 15));
    CHECK(!xihash_valid("01234567\0abcdefg", 16));
    CHECK(xihash_hamming(a, 16, "invalid", 7) == -1);
    CHECK(xihash_hamming(a, 16, "01234567\0abcdefg", 16) == -1);
    CHECK(strcmp(xihash_last_error(), "invalid hash string") == 0);
    CHECK(xihash_valid(a, 16));
    CHECK(strcmp(xihash_last_error(), "") == 0);
    CHECK(xihash_hamming(a, 16, "invalid", 7) == -1);
    CHECK(!xihash_valid("invalid", 7));
    CHECK(strcmp(xihash_last_error(), "") == 0);
    CHECK(xihash_ahash("release/examples/extlibs/assets/missing.png") == NULL);
    CHECK(strcmp(xihash_last_error(), "cannot read or decode image") == 0);
    CHECK(xrtGetError() == NULL);
    CHECK(xihash_phash("") == NULL);
    CHECK(strcmp(xihash_last_error(), "image path is empty") == 0);
    CHECK(xihash_hamming(a, 16, a, 16) == 0);
    CHECK(strcmp(xihash_last_error(), "") == 0);
    {
        unsigned point, failures = 0u, successes = 0u;
        for (point = 0u; point < 64u; ++point) {
            char *result;
            int triggered;
            CHECK(xrtMemDebugFailAfter(point));
            result = xihash_ahash(path);
            triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (result == NULL) {
                CHECK(triggered && xrtGetError() != NULL &&
                    xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                CHECK(strcmp(xihash_last_error(), "out of memory") == 0);
                ++failures;
            } else {
                CHECK(strcmp(result, a) == 0 && xrtGetError() == NULL);
                CHECK(strcmp(xihash_last_error(), "") == 0);
                xrtFree(result);
                ++successes;
            }
        }
        CHECK(failures >= 2u && successes != 0u);
    }
    {
        char* workdir = xrtDirTemp(NULL, "xlang_imagehash_test_", NULL);
        char* gradient_path;
        char* utf8_path;
        char* bad_path;
        unsigned char data[1050];
        int header = snprintf((char *)data, sizeof(data), "P5\n32 32\n255\n");
        char *gradient_hash;
        int x, y;
        CHECK(workdir != NULL);
        gradient_path = xrtPathJoin(workdir, "gradient.pgm");
        utf8_path = xrtPathJoin(workdir, "中文.png");
        bad_path = xrtPathJoin(workdir, "invalid.png");
        CHECK(gradient_path != NULL && utf8_path != NULL && bad_path != NULL);
        CHECK(header > 0 && header + 1024 <= (int)sizeof(data));
        for (y = 0; y < 32; ++y) {
            for (x = 0; x < 32; ++x) {
                data[header + y * 32 + x] =
                    (unsigned char)(x * 7 + y * 9 + (x * y) % 17);
            }
        }
        CHECK(xrtFileWriteAll(gradient_path,
            (xbytesview){data, (size_t)header + 1024u}));
        gradient_hash = xihash_phash(gradient_path);
        CHECK(gradient_hash != NULL && xihash_valid(gradient_hash, 16));
        CHECK(strcmp(gradient_hash, "75275b588676d08a") == 0);
        xrtFree(gradient_hash);
        CHECK(xrtFileDelete(gradient_path));
        {
            const unsigned char bad_data[] = {'b', 'a', 'd'};
            size_t size = 0;
            bytes encoded = xrtFileReadAll(path, &size);
            char *utf8_hash;
            CHECK(encoded != NULL && size > 0);
            CHECK(xrtFileWriteAll(utf8_path, (xbytesview){encoded, size}));
            utf8_hash = xihash_ahash(utf8_path);
            CHECK(utf8_hash != NULL && strcmp(a, utf8_hash) == 0);
            xrtFree(utf8_hash);
            CHECK(xrtFileDelete(utf8_path));
            CHECK(xrtFileWriteAll(bad_path, (xbytesview){bad_data, sizeof(bad_data)}));
            CHECK(xihash_ahash(bad_path) == NULL);
            CHECK(strcmp(xihash_last_error(), "cannot read or decode image") == 0);
            CHECK(xrtGetError() == NULL);
            CHECK(xrtFileDelete(bad_path));
            xrtFree(encoded);
        }
        CHECK(xrtDirRemove(workdir));
        xrtFree(gradient_path);
        xrtFree(utf8_path);
        xrtFree(bad_path);
        xrtFree(workdir);
    }
    xrtFree(a);
    xrtFree(d);
    xrtFree(p);
    puts("imagehash C test: PASS");
    return 0;
}
