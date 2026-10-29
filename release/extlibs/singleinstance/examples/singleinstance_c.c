#include "xrt.h"
#include "../include/singleinstance_native.h"
#ifdef __TINYC__
/* 单文件范例直接使用包内实现；正式包仍按清单独立编译。 */
#include "../singleinstance_native.c"
#endif
#include <stdio.h>

int main(void)
{
    int result = xsi_acquire("my_app");
    if (result < 0) return 2;
    if (result == 0) {
        puts("Another instance is already running!");
        return 1;
    }
    puts("Acquired single instance lock");
    printf("isAcquired: %s\n", xsi_is_acquired() ? "true" : "false");
    if (!xsi_release()) return 2;
    puts("Released lock");
    puts("singleinstance example: PASS");
    return 0;
}
