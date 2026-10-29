#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "xrt.h"

#include "../include/op.h"
#include "utf8_window_fixture.c"

static int same_text(void *result, const char *expected)
{
    int same = result && strcmp((const char *)result, expected) == 0;
    xrtFree(result);
    return same;
}

int main(void)
{
    const char *title = "\xe4\xbd\xa0\xe5\xa5\xbd OP";
    const char *invalid = "\xc3\x28";
    long long window = op_test_create_utf8_window();
    void *context = xop_create();
    WCHAR wide_title[64] = {0};
    char handle_text[32];
    int ok;

    if (!window || !context) return 1;
    snprintf(handle_text, sizeof(handle_text), "%lld", window);
    ok = xop_set_window_title(window, title) == 1 &&
         GetWindowTextW((HWND)(intptr_t)window, wide_title, 64) > 0 &&
         wcscmp(wide_title, L"\x4f60\x597d OP") == 0 &&
         same_text(xop_get_window_title(window), title) &&
         xop_find_window("op_utf8_title_test", title) == window &&
         same_text(xop_enum_window(0, title, "op_utf8_title_test", 0), handle_text) &&
         same_text(xop_enum_window(0, invalid, "op_utf8_title_test", 0), "") &&
         xop_set_window_title(window, invalid) == 0 &&
         same_text(xop_get_window_title(window), title) &&
         xop_send_string(window, "A\xe4\xbd\xa0\xe5\xa5\xbd") == 1 &&
         op_test_pump_utf8_window(window) == 3 &&
         op_test_received_utf16_at(0) == 'A' &&
         op_test_received_utf16_at(1) == 0x4F60 &&
         op_test_received_utf16_at(2) == 0x597D &&
         xop_send_string(window, invalid) == 0 &&
         op_test_pump_utf8_window(window) == 3 &&
         xop_bind_window(context, window, "normal", "normal", "windows", 0) == 1 &&
         xop_key_press_str(context, "B\xf0\x9f\x98\x80\xe5\xa5\xbd", 1) == 1 &&
         op_test_pump_utf8_window(window) == 7 &&
         op_test_received_utf16_at(3) == 'B' &&
         op_test_received_utf16_at(4) == 0xD83D &&
         op_test_received_utf16_at(5) == 0xDE00 &&
         op_test_received_utf16_at(6) == 0x597D &&
         xop_key_press_str(context, invalid, 1) == 0 &&
         op_test_pump_utf8_window(window) == 7 &&
         xop_key_char(context, 0x1F600) == 1 &&
         op_test_pump_utf8_window(window) == 9 &&
         op_test_received_utf16_at(7) == 0xD83D &&
         op_test_received_utf16_at(8) == 0xDE00 &&
         xop_key_char(context, 0xD800) == 0 &&
         xop_key_char(context, 0x110000) == 0 &&
         op_test_pump_utf8_window(window) == 9 &&
         xop_set_clipboard(invalid) == 0;
    xop_destroy(context);
    DestroyWindow((HWND)(intptr_t)window);
    if (!ok) return 2;
    puts("op utf8 window title: ok");
    return 0;
}
