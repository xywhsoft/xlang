/*
 * op_hook_shared.h - Shared memory protocol between op_native.c and hook DLLs.
 * The hook DLL writes frames into a named file mapping; op_native.c reads them.
 */
#ifndef OP_HOOK_SHARED_H
#define OP_HOOK_SHARED_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "op_hook_bounds.h"

#define OP_HOOK_MAGIC 0x4F504832  /* "OPH2" */

typedef struct {
    unsigned int magic;         /* must be OP_HOOK_MAGIC */
    unsigned int width;         /* frame width in pixels */
    unsigned int height;        /* frame height in pixels */
    unsigned int frame_counter; /* odd while writing, even when complete */
    unsigned int format;        /* 0 = BGRA (32-bit) */
    /* pixel data follows: width * height * 4 bytes (BGRA) */
} op_hook_header;

/* Helper: get pixel data pointer from mapped base */
static inline unsigned char *op_hook_pixels(void *base) {
    return (unsigned char *)base + sizeof(op_hook_header);
}

static inline void op_hook_begin_frame(op_hook_header *hdr, unsigned int w, unsigned int h) {
    InterlockedIncrement((volatile LONG *)&hdr->frame_counter);
    hdr->magic = 0u;
    hdr->width = w;
    hdr->height = h;
    hdr->format = 0u;
}

static inline void op_hook_end_frame(op_hook_header *hdr) {
    hdr->magic = OP_HOOK_MAGIC;
    InterlockedIncrement((volatile LONG *)&hdr->frame_counter);
}

/* Helper: write a frame into shared memory */
static inline void op_hook_write_frame(void *base, unsigned int w, unsigned int h,
                                const void *pixels, unsigned int pitch) {
    if (base == NULL || pixels == NULL ||
        !op_hook_frame_fits(w, h, OP_HOOK_MAP_SIZE, sizeof(op_hook_header)) ||
        pitch < w * 4u) return;
    op_hook_header *hdr = (op_hook_header *)base;
    op_hook_begin_frame(hdr, w, h);

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
    op_hook_end_frame(hdr);
}

/* 成功时才输出尺寸；版本变化意味着帧仍在写入，调用方应重试。 */
static inline int op_hook_snapshot(void *base, void *pixels, size_t capacity,
                                   unsigned int *width, unsigned int *height) {
    op_hook_header *hdr = (op_hook_header *)base;
    LONG before;
    LONG after;
    unsigned int w;
    unsigned int h;
    size_t bytes;
    if (hdr == NULL || pixels == NULL || width == NULL || height == NULL) return 0;
    before = InterlockedCompareExchange((volatile LONG *)&hdr->frame_counter, 0, 0);
    if ((before & 1) != 0 || hdr->magic != OP_HOOK_MAGIC || hdr->format != 0u) return 0;
    w = hdr->width;
    h = hdr->height;
    if (!op_hook_frame_fits(w, h, OP_HOOK_MAP_SIZE, sizeof(op_hook_header))) return 0;
    bytes = (size_t)w * (size_t)h * 4u;
    if (capacity < bytes) return 0;
    memcpy(pixels, op_hook_pixels(base), bytes);
    after = InterlockedCompareExchange((volatile LONG *)&hdr->frame_counter, 0, 0);
    if (before != after || (after & 1) != 0) return 0;
    *width = w;
    *height = h;
    return 1;
}

/* Helper: open shared memory by PID (used by hook DLL) */
static inline void *op_hook_open_shared(unsigned long pid, unsigned int map_size) {
    char name[64];
    snprintf(name, sizeof(name), "op_hook_v2_%lu", pid);
    HANDLE h = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name);
    if (!h) return NULL;
    void *ptr = MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, map_size);
    CloseHandle(h);
    return ptr;
}

#endif /* OP_HOOK_SHARED_H */
