#include <stddef.h>

/* 模拟只实现旧版三个入口的桥接 DLL，验证装载器不会误报可用。 */
__declspec(dllexport) void* opencv_xl_imread(const char* path, int flags)
{
    (void)path;
    (void)flags;
    return NULL;
}

__declspec(dllexport) void* opencv_xl_resize(void* image, int width, int height)
{
    (void)image;
    (void)width;
    (void)height;
    return NULL;
}

__declspec(dllexport) void opencv_xl_release(void* image)
{
    (void)image;
}
