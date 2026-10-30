#ifndef XSTBIW_NATIVE_H
#define XSTBIW_NATIVE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    XSTBIW_PNG = 1,
    XSTBIW_JPEG = 2,
    XSTBIW_BMP = 3,
    XSTBIW_TGA = 4,
    XSTBIW_HDR = 5
};

int xstbiw_available(void);
const char* xstbiw_version(void);

/* 返回 xbuffer*，由调用方 xrtBufferDestroy；void* 保持 XL point ABI 一致。 */
/* 像素只借用；BMP 的 RGB 展开/行填充在分配输出前检查 128 MiB 限额。 */
void* xstbiw_encode(int Format, void* Pixels, size_t Length,
    int Width, int Height, int Channels, int Option);

/* PNG 的 Option 为字节步长(0=紧凑)；JPEG 为质量；其他格式忽略。 */
int xstbiw_write(const char* Path, int Format, void* Pixels,
    size_t Length, int Width, int Height, int Channels, int Option);

#ifdef __cplusplus
}
#endif
#endif
