#include "../barcode_native.c"
#include <stdio.h>
#include <string.h>

/* 扫码测试只读取公开 ASCII 结果，不复制编码器内部位图。 */
int main(int argc, char **argv) {
    char *modules;
    int upc, i;
    if (argc != 3) return 2;
    upc = strcmp(argv[1], "upc") == 0;
    if (!upc && strcmp(argv[1], "ean") != 0) return 2;
    modules = xb_ascii(argv[2], strlen(argv[2]), upc != 0);
    if (modules == NULL) return 1;
    if (strlen(modules) != 95) {
        xrtFree(modules);
        return 1;
    }
    for (i = 0; i < 95; ++i) {
        if (modules[i] != '#' && modules[i] != ' ') {
            xrtFree(modules);
            return 1;
        }
        putchar(modules[i] == '#' ? '1' : '0');
    }
    putchar('\n');
    xrtFree(modules);
    return 0;
}
