#ifndef XSTBIW_TEST_NATIVE
#define XSTBIW_TEST_NATIVE "../stb_image_write_native.c"
#endif
#define XRT_MODULE_HASH64
#include XSTBIW_TEST_NATIVE

/* 固定正色值夹具，只量化完整进程成本；计时由外层有界运行器完成。 */
int main(void)
{
    const size_t count = 1024u * 1024u * 3u;
    float* pixels = xrtMalloc(count * sizeof(float));
    if (pixels == NULL) return 1;
    for (size_t i = 0; i < count; ++i) pixels[i] = (float)(i % 65535u) / 1024.0f;
    uint64 hash = 0;
    size_t size = 0;
    for (unsigned i = 0; i < 4u; ++i) {
        xbuffer* encoded = xstbiw_encode(XSTBIW_HDR, pixels, count * sizeof(float), 1024, 1024, 3, 0);
        if (encoded == NULL) { xrtFree(pixels); return 1; }
        size = encoded->Size;
        hash = xrtHash64(encoded->Data, encoded->Size);
        xrtBufferDestroy(encoded);
    }
    xrtFree(pixels);
    printf("HDR benchmark: %zu bytes, hash=%llu\n", size, (unsigned long long)hash);
    return 0;
}
