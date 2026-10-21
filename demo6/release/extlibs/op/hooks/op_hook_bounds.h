#ifndef OP_HOOK_BOUNDS_H
#define OP_HOOK_BOUNDS_H

#include <stddef.h>

#define OP_HOOK_MAP_SIZE (64u * 1024u * 1024u + 4096u)

static inline int op_hook_frame_fits(unsigned int width, unsigned int height,
                                     size_t map_size, size_t header_size)
{
    size_t pixels;
    if (width == 0u || height == 0u || width > 4096u || height > 4096u ||
        map_size < header_size) return 0;
    pixels = (map_size - header_size) / 4u;
    return (size_t)width <= pixels / (size_t)height;
}

#endif
