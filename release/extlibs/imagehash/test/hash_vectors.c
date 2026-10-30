/* 输出真实缩放像素与公开哈希；Python 独立按定义计算，不调用包的内部算法。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/imagehash_native.h"
#include "../../stb_image_resize2/include/stb_image_resize2_native.h"
#include <stdio.h>
#include <string.h>

#ifdef __TINYC__
#include "../../stb_image/stb_image_native.c"
#include "../../stb_image_resize2/stb_image_resize2_native.c"
#include "../imagehash_native.c"
#endif

static void hex(const xbuffer* buffer)
{
    putchar('"');
    for (size_t i = 0; i < buffer->Size; ++i) printf("%02x", buffer->Data[i]);
    putchar('"');
}

int main(int argc, char** argv)
{
    if (argc != 2 || !argv[1][0] || xrtPathExists(argv[1])) return 2;
    static const int sizes[][2] = {{8,8}, {9,8}, {32,32}, {43,17}};
    unsigned char pixels[1024];
    char previous[17] = "0000000000000000";
    for (unsigned size = 0; size < 4; ++size) {
        int w = sizes[size][0], h = sizes[size][1];
        for (unsigned pattern = 0; pattern < 5; ++pattern) {
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    unsigned value = pattern == 0 ? 0 : pattern == 1 ? 255 :
                        pattern == 2 ? (unsigned)(x * 7 + y * 9 + x * y % 17) :
                        pattern == 3 ? ((x + y) % 2 ? 255 : 0) :
                        (unsigned)(x * 73 + y * 151 + x * y * 19 + size * 47);
                    pixels[y * w + x] = (unsigned char)value;
                }
            }
            char header[64];
            int length = snprintf(header, sizeof(header), "P5\n%d %d\n255\n", w, h);
            xbuffer* input = xrtBufferCreate();
            if (!input || !xrtBufferAppend(input, (xbytesview){(cbytes)header, (size_t)length}) ||
                !xrtBufferAppend(input, (xbytesview){pixels, (size_t)w * (size_t)h}) ||
                !xrtFileWriteAll(argv[1], xrtBufferView(input))) return 1;
            xrtBufferDestroy(input);
            char* a = xihash_ahash(argv[1]);
            char* d = xihash_dhash(argv[1]);
            char* p = xihash_phash(argv[1]);
            xbuffer* resized[3];
            resized[0] = xstbir_resize(pixels, (size_t)w * h, w, h, 1, 8, 8, 0, 1);
            resized[1] = xstbir_resize(pixels, (size_t)w * h, w, h, 1, 9, 8, 0, 1);
            resized[2] = xstbir_resize(pixels, (size_t)w * h, w, h, 1, 32, 32, 0, 1);
            if (!a || !d || !p || !resized[0] || !resized[1] || !resized[2]) return 1;
            printf("{\"size\":%u,\"pattern\":%u,\"hashes\":[\"%s\",\"%s\",\"%s\"],\"distance\":%d,\"pixels\":[",
                size, pattern, a, d, p, xihash_hamming(previous, 16, p, 16));
            for (unsigned i = 0; i < 3; ++i) {
                if (i) putchar(',');
                hex(resized[i]);
                xrtBufferDestroy(resized[i]);
            }
            puts("]}");
            memcpy(previous, p, sizeof(previous));
            xrtFree(a); xrtFree(d); xrtFree(p);
            if (!xrtFileDelete(argv[1])) return 1;
        }
    }
    return 0;
}
