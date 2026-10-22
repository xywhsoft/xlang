#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "xrt.h"
#include "../include/op.h"
#include "input_window_fixture.c"

static int rejects_path(void *op, const char *path, long long delay)
{
    xrtClearError();
    return xop_drag_path(op, path, delay) == 0 && xrtGetError() != NULL;
}

static int rejects_combo(void *op, const char *combo)
{
    xrtClearError();
    return xop_key_combo(op, combo) == 0 && xrtGetError() != NULL;
}

static int rejects_bind(void *op, long long original, long long other,
                        const char *display, const char *mouse,
                        const char *keypad, long long mode)
{
    xrtClearError();
    return xop_bind_window(op, other, display, mouse, keypad, mode) == 0 &&
           xrtGetError() != NULL && xop_is_bind(op) == 1 &&
           xop_get_bind_window(op) == original;
}

int main(void)
{
    long long window = op_test_create_input_window();
    long long other = op_test_create_input_window();
    void *op = xop_create();
    char many_points[1100];
    char long_combo[300];
    int i, ok;

    if (!window || !other || !op) return 1;
    if (!xop_bind_window(op, window, "normal", "windows", "windows", 0)) return 2;
    for (i = 0; i < 513; i++) {
        many_points[i * 2] = '0';
        many_points[i * 2 + 1] = ',';
    }
    many_points[1025] = '\0';
    memset(long_combo, 'a', sizeof(long_combo) - 1);
    long_combo[sizeof(long_combo) - 1] = '\0';

    /* 仅向本进程隐藏窗口投递消息，绝不向真实桌面注入输入。 */
    ok = rejects_bind(op, window, other, "missing", "windows", "windows", 0) &&
         rejects_bind(op, window, other, "normal012345678901234567890123456789", "windows", "windows", 0) &&
         rejects_bind(op, window, other, "normal", "missing", "windows", 0) &&
         rejects_bind(op, window, other, "normal", "windows", "missing", 0) &&
         rejects_bind(op, window, other, "normal", "windows", "windows", 1) &&
         rejects_path(op, "0,0,1,1,2", 0) &&
         rejects_path(op, "0,0,1x,1", 0) &&
         rejects_path(op, "0,0,,1,1", 0) &&
         rejects_path(op, "0,0,1,1,", 0) &&
         rejects_path(op, "0,0,2147483648,1", 0) &&
         rejects_path(op, many_points, 0) &&
         rejects_path(op, "0,0,1,1", -1) &&
         rejects_path(op, "0,0,1,1", 2147483648LL) &&
         rejects_combo(op, "ctrl++a") &&
         rejects_combo(op, "ctrl+unknown") &&
         rejects_combo(op, "ctrl+") &&
         rejects_combo(op, "f12junk") &&
         rejects_combo(op, long_combo) &&
         rejects_combo(op, "a+b+c+d+e+f+g+h+i") &&
         xop_drag_path(op, "0,0,1,1", 0) == 1 &&
         xop_key_combo(op, "ctrl+a") == 1 &&
         xop_key_combo(op, "f24") == 1;

    xop_destroy(op);
    if (!op_test_destroy_input_window(window)) return 4;
    if (!op_test_destroy_input_window(other)) return 5;
    if (!ok) return 3;
    puts("op input parsing: ok");
    return 0;
}
