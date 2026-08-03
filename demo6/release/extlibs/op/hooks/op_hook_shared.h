/*
 * op_hook_shared.h - Shared memory protocol between op_native.c and hook DLLs.
 * The hook DLL writes frames into a named file mapping; op_native.c reads them.
 */
#ifndef OP_HOOK_SHARED_H
#define OP_HOOK_SHARED_H

#include <windows.h>

#define OP_HOOK_MAGIC 0x4F50484B  /* "OPHK" */

typedef struct {
    unsigned int magic;         /* must be OP_HOOK_MAGIC */
    unsigned int width;         /* frame width in pixels */
    unsigned int height;        /* frame height in pixels */
    unsigned int frame_counter; /* incremented each frame */
    unsigned int format;        /* 0 = BGRA (32-bit) */
    /* pixel data follows: width * height * 4 bytes (BGRA) */
} op_hook_header;

/* Helper: get pixel data pointer from mapped base */
static unsigned char *op_hook_pixels(void *base) {
    return (unsigned char *)base + sizeof(op_hook_header);
}

/* Helper: write a frame into shared memory */
static void op_hook_write_frame(void *base, unsigned int w, unsigned int h,
                                const void *pixels, unsigned int pitch) {
    op_hook_header *hdr = (op_hook_header *)base;
    hdr->magic = OP_HOOK_MAGIC;
    hdr->width = w;
    hdr->height = h;
    hdr->format = 0; /* BGRA */
    hdr->frame_counter++;

    unsigned char *dst = op_hook_pixels(base);
    if (pitch == w * 4) {
        memcpy(dst, pixels, w * h * 4);
    } else {
        unsigned int row;
        for (row = 0; row < h; row++) {
            memcpy(dst + row * w * 4,
                   (const unsigned char *)pixels + row * pitch,
                   w * 4);
        }
    }
}

/* Helper: open shared memory by PID (used by hook DLL) */
static void *op_hook_open_shared(unsigned long pid, unsigned int map_size) {
    char name[64];
    snprintf(name, sizeof(name), "op_hook_%lu", pid);
    HANDLE h = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name);
    if (!h) return NULL;
    void *ptr = MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, map_size);
    CloseHandle(h);
    return ptr;
}

#endif /* OP_HOOK_SHARED_H */
