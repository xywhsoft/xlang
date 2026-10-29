#include <windows.h>
#include <stdint.h>

/* 测试专用入口：避免依赖 Explorer 是否创建 Progman 窗口。 */
long long op_test_desktop_window(void)
{
    return (long long)(intptr_t)GetDesktopWindow();
}
