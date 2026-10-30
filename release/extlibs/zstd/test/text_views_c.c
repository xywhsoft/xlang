/* 两个 view 入口须在正式头中声明；借用的长度不能退回 strlen。 */
#include "../include/xrt_zstd.h"
#include "../include/zstd_native.h"
#include <stdio.h>
#include <string.h>
int main(void) {
    static const char input[] = {'A',0,(char)0xe4,(char)0xb8,(char)0xad};
    xstrview text = {input, sizeof(input)};
    xbuffer *packed = xzstd_compress_text(text, 3, 1048576, 1048576), *plain = NULL;
    ptr encoder = NULL;
    xrtzstderror error;
    bool ok = false;
    if (!packed || !xrtZstdDecompress(packed->Data, packed->Size, NULL, &plain, &error) ||
        !plain || plain->Size != sizeof(input) || memcmp(plain->Data, input, sizeof(input))) goto cleanup;
    xrtBufferDestroy(plain); plain = NULL; xrtBufferDestroy(packed); packed = NULL;
    encoder = xzstd_compressor_create(3, 1048576, 1048576);
    if (!encoder || !xzstd_compressor_update_text(encoder, text) || !xzstd_compressor_finish(encoder)) goto cleanup;
    packed = xzstd_compressor_drain(encoder);
    if (!packed || !xrtZstdDecompress(packed->Data, packed->Size, NULL, &plain, &error) ||
        !plain || plain->Size != sizeof(input) || memcmp(plain->Data, input, sizeof(input))) goto cleanup;
    ok = true;
cleanup:
    xzstd_compressor_destroy(encoder); xrtBufferDestroy(packed); xrtBufferDestroy(plain);
    if (ok) puts("zstd C text views: PASS");
    else fputs("zstd C text views: FAIL\n", stderr);
    return ok ? 0 : 1;
}
