#define XRT_MODULE_BUFFER
#define XRT_MODULE_RANDOM_SECURE
#include "xrt.h"
#include "../include/sharedmem_native.h"
#include <stdio.h>

int main(void)
{
    char name[64];
    uint64_t nonce;
    const unsigned char binary[] = {65, 0, 66};
    void* owner;
    void* reader;
    if (!xrtSecureRandom(&nonce, sizeof(nonce))) return 1;
    snprintf(name, sizeof(name), "xlang_sharedmem_example_%016llx",
        (unsigned long long)nonce);
    owner = xshm_create(name, 128);
    reader = owner != NULL ? xshm_open(xshm_name_of(owner)) : NULL;
    xbuffer* copy;
    int64_t integer = 0;
    if (reader == NULL || !xshm_write(owner, 0, "hello", 5) ||
        !xshm_write_int64(reader, 16, 42) ||
        !xshm_write(owner, 32, binary, 3)) goto fail;
    copy = (xbuffer*)xshm_read(reader, 0, 5);
    if (copy == NULL || !xshm_read_int64(owner, 16, &integer)) {
        xrtBufferDestroy(copy);
        goto fail;
    }
    printf("text: %.*s\n", (int)xrtBufferView(copy).Size,
        (const char*)xrtBufferView(copy).Data);
    printf("integer: %lld\n", (long long)integer);
    xrtBufferDestroy(copy);
    copy = (xbuffer*)xshm_read(reader, 32, 3);
    if (copy == NULL) goto fail;
    printf("binary size: %lu\n", (unsigned long)xrtBufferView(copy).Size);
    xrtBufferDestroy(copy);
    xshm_close(reader);
    xshm_close(owner);
    return 0;
fail:
    xshm_close(reader);
    xshm_close(owner);
    return 1;
}

/* 单文件 TCC 范例补入实现；正式包仍独立编译原生源码。 */
#ifdef __TINYC__
#include "../sharedmem_native.c"
#endif
