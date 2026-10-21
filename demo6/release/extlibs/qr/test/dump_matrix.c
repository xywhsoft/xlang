#include "../qr_native.c"
#include <stdio.h>

/* 独立扫码器可读取此 0/1 矩阵；仅作测试夹具，不属于公开 API。 */
int main(int argc, char **argv) {
    XqrMatrix matrix;
    char input[8192];
    const char *text;
    const char *level;
    size_t length;
    int x, y;
    if (argc == 3) {
        text = argv[1];
        level = argv[2];
        length = strlen(text);
    } else if (argc == 2) {
        length = fread(input, 1, sizeof(input), stdin);
        text = input;
        level = argv[1];
    } else return 1;
    if (!xqr_build_checked(text, length, level, strlen(level), &matrix)) return 1;
    printf("%d\n", matrix.size);
    for (y = 0; y < matrix.size; ++y) {
        for (x = 0; x < matrix.size; ++x) putchar('0' + matrix.modules[y][x]);
        putchar('\n');
    }
    return 0;
}
