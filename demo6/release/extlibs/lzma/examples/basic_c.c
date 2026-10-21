#include "../include/xrt_lzma.h"

#include <stdio.h>
#include <string.h>

#ifdef __TINYC__
/* 单文件 xl run 编入 XZ 后端；正式包仍按清单分离编译。 */
#include "../third_party/xz/7zCrc.c"
#include "../third_party/xz/7zCrcOpt.c"
#include "../third_party/xz/Alloc.c"
#include "../third_party/xz/Bra.c"
#include "../third_party/xz/Bra86.c"
#include "../third_party/xz/CpuArch.c"
#include "../third_party/xz/Delta.c"
#include "../third_party/xz/LzFind.c"
#include "../third_party/xz/LzmaDec.c"
#include "../third_party/xz/LzmaEnc.c"
#include "../third_party/xz/Lzma2Dec.c"
#include "../third_party/xz/Lzma2Enc.c"
#include "../third_party/xz/Sha256.c"
#include "../third_party/xz/Sha256Opt.c"
#include "../third_party/xz/Xz.c"
#include "../third_party/xz/XzCrc64.c"
#include "../third_party/xz/XzCrc64Opt.c"
#include "../third_party/xz/XzDec.c"
#include "../third_party/xz/XzEnc.c"
#include "../xrt_lzma_common.c"
#include "../xrt_lzma_encode.c"
#include "../xrt_lzma_decode.c"
#include "../xrt_lzma.c"
#endif

/* 独立 C 范例，与 basic.xl 不同名以免成为自动伴随源。 */
int main(void)
{
    static const char text[] = "hello lzma";
    xbuffer *packed = NULL, *plain = NULL;
    xrtlzmaerror error;
    xbytesview result;
    if (!xrtLzmaCompress(text, sizeof(text) - 1u, NULL, &packed, &error)) {
        fprintf(stderr, "%s\n", error.sMessage);
        return 1;
    }
    result = xrtBufferView(packed);
    if (!xrtLzmaDecompress(result.Data, result.Size, NULL, &plain, &error)) {
        fprintf(stderr, "%s\n", error.sMessage);
        xrtBufferDestroy(packed);
        return 1;
    }
    result = xrtBufferView(plain);
    if (result.Size != sizeof(text) - 1u || memcmp(result.Data, text, result.Size) != 0) {
        xrtBufferDestroy(plain);
        xrtBufferDestroy(packed);
        return 1;
    }
    puts("lzma round trip: hello lzma");
    xrtBufferDestroy(plain);
    xrtBufferDestroy(packed);
    return 0;
}
