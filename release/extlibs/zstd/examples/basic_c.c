/* 独立 C 范例，与 basic.xl 不同名以免成为自动伴随源。 */
#include "../include/xrt_zstd.h"

#include <stdio.h>
#include <string.h>

#ifdef __TINYC__
/* 单文件 xl run 使用包内实现；正式包仍分别编译。 */
#include "../third_party/zstd.c"
#include "../xrt_zstd.c"
#endif

int main(void) {
    const char input[] = "hello zstd";
    xbuffer *packed = NULL, *plain = NULL;
    xrtzstderror error;
    xbytesview output;
    if (!xrtZstdCompress(input, sizeof(input) - 1u, NULL, &packed, &error))
        return 1;
    if (!xrtZstdDecompress(xrtBufferView(packed).Data,
            xrtBufferView(packed).Size, NULL, &plain, &error)) {
        xrtBufferDestroy(packed);
        return 1;
    }
    output = xrtBufferView(plain);
    if (output.Size != sizeof(input) - 1u ||
        memcmp(output.Data, input, output.Size) != 0) {
        xrtBufferDestroy(plain);
        xrtBufferDestroy(packed);
        return 1;
    }
    puts("zstd round trip: hello zstd");
    xrtBufferDestroy(plain);
    xrtBufferDestroy(packed);
    return 0;
}
