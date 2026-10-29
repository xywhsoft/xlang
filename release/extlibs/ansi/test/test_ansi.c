#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/ansi_native.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "ansi C test failed at line %d: %s\n", __LINE__, #condition); \
    return 1; \
} } while (0)

static int same(void* owned, const char* expected)
{
    int result = owned != NULL && strcmp((char*)owned, expected) == 0;
    xrtFree(owned);
    return result;
}

int main(void)
{
    char long_text[4097];
    char* wrapped;
    char* saved;
    int i;

    CHECK(same(xansi_bold("hi"), "\x1b[1mhi\x1b[0m"));
    CHECK(same(xansi_fg("hi", 31), "\x1b[31mhi\x1b[0m"));
    CHECK(same(xansi_bg256("x", 17), "\x1b[48;5;17mx\x1b[0m"));
    CHECK(same(xansi_fg_true("x", 255, 128, 0),
        "\x1b[38;2;255;128;0mx\x1b[0m"));
    CHECK(same(xansi_up(2), "\x1b[2A"));
    CHECK(same(xansi_clear_line(), "\x1b[2K"));
    CHECK(same(xansi_title("标题"), "\x1b]0;标题\a"));

    memset(long_text, 'z', sizeof(long_text) - 1u);
    long_text[sizeof(long_text) - 1u] = '\0';
    wrapped = (char*)xansi_underline(long_text);
    CHECK(wrapped != NULL && strlen(wrapped) == strlen(long_text) + 8u);
    CHECK(strncmp(wrapped, "\x1b[4m", 4u) == 0);
    CHECK(strcmp(wrapped + strlen(wrapped) - 4u, "\x1b[0m") == 0);
    xrtFree(wrapped);

    saved = (char*)xansi_bold("keep");
    CHECK(saved != NULL);
    for (i = 0; i < 128; ++i) {
        void* next = xansi_fg256("next", i);
        CHECK(next != NULL && strcmp(saved, "\x1b[1mkeep\x1b[0m") == 0);
        xrtFree(next);
    }
    xrtFree(saved);

    CHECK(same(xansi_strip("a\x1b[31mb\x1b[0mc"), "abc"));
    CHECK(same(xansi_strip("\x1b]0;title\a" "text"), "text"));
    CHECK(same(xansi_strip("\x1b]8;;url\x1b\\link\x1b]8;;\x1b\\"), "link"));
    CHECK(xansi_display_width("A中文") == 5);
    CHECK(xansi_display_width("e\xcc\x81") == 1);
    CHECK(xansi_display_width("😀") == 2);
    CHECK(xansi_display_width("a\xe2\x80\x8b" "b") == 2);
    CHECK(xansi_display_width("a\xe2\x80\x8c" "b") == 2);
    CHECK(xansi_display_width("a\xe2\x81\xa0" "b") == 2);
    CHECK(xansi_display_width("\x1b[31m中文\x1b[0m") == 4);
    CHECK(xansi_display_width("\x1b]0;标题\a" "x") == 1);

    CHECK(xansi_fg("x", 0) == NULL);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    CHECK(xansi_bold("\xff") == NULL);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    CHECK(xansi_fg("\xff", 31) == NULL);
    CHECK(xansi_bg256("\xff", 17) == NULL);
    CHECK(xansi_fg_true("\xff", 1, 2, 3) == NULL);
    CHECK(same(xansi_bold("ok"), "\x1b[1mok\x1b[0m"));
    CHECK(xrtGetError() == NULL);
    CHECK(xansi_bg256("x", 256) == NULL);
    CHECK(xansi_fg_true("x", 1, 2, -1) == NULL);
    CHECK(xansi_column(0) == NULL);
    CHECK(xansi_title("unsafe\a") == NULL);
    CHECK(xansi_title("unsafe\xc2\x9c") == NULL);
    CHECK(xansi_title("safe\xe2\x80\xae" "hidden") == NULL);
    CHECK(xansi_title("safe\xe2\x80\xa8" "hidden") == NULL);
    CHECK(xansi_title("safe\xe2\x80\xa9" "hidden") == NULL);
    CHECK(xansi_strip("a\x1b[31") == NULL);
    CHECK(xansi_strip("a\x1bX") == NULL);
    CHECK(xansi_display_width("\x1b]0;unterminated") == -1);
    CHECK(xansi_display_width("a\n") == -1);
    CHECK(xansi_display_width("a\xc2\xad" "b") == -1); /* 软连字符宽度依赖换行。 */
    CHECK(xansi_display_width("a\xe2\x80\xae" "b") == -1);
    CHECK(xansi_display_width("a\xe2\x80\xa8" "b") == -1);
    CHECK(xansi_display_width("a\xe2\x80\xa9" "b") == -1);
    CHECK(xansi_display_width("\x1b]0;a\xe2\x80\xae" "b\a") == -1);
    CHECK(xansi_display_width("\x1b]0;a\xe2\x80\xa8" "b\a") == -1);
    CHECK(xansi_display_width("\xc2\x9c") == -1);
    CHECK(xansi_display_width("\xc3\x28") == -1);
    CHECK(xansi_strip("\xc3\x28") == NULL);
    CHECK(xansi_display_width("\x1b]0;\xff\a") == -1);
    CHECK(xansi_display_width("\x1b]8;;\xff\x1b\\") == -1);
    CHECK(xansi_strip("\x1b]0;\xff\a") == NULL);
    CHECK(xansi_display_width("\x1b]0;\x7f\a") == -1);
    CHECK(xansi_display_width("\x1b]0;\xc2\x9c\x1b\\") == -1);
    CHECK(xansi_strip("\x1b]0;\xc2\x9c\a") == NULL);
    CHECK(xansi_strip("\x1b]0;a\xe2\x80\xae" "b\a") == NULL);
    CHECK(xansi_strip("\x1b]0;a\xe2\x80\xa9" "b\a") == NULL);

    /* 失败后的正常结果不能携带旧的 XRT 线程错误。 */
    CHECK(xansi_fg("x", 0) == NULL && xrtGetError() != NULL);
    CHECK(same(xansi_bold("ok"), "\x1b[1mok\x1b[0m"));
    CHECK(xrtGetError() == NULL);
    CHECK(xansi_bg256("x", 256) == NULL && xrtGetError() != NULL);
    CHECK(same(xansi_clear_line(), "\x1b[2K"));
    CHECK(xrtGetError() == NULL);
    CHECK(xansi_column(0) == NULL && xrtGetError() != NULL);
    CHECK(same(xansi_up(1), "\x1b[1A"));
    CHECK(xrtGetError() == NULL);
    CHECK(xansi_title("unsafe\a") == NULL && xrtGetError() != NULL);
    CHECK(same(xansi_title("safe"), "\x1b]0;safe\a"));
    CHECK(xrtGetError() == NULL);
    CHECK(xansi_strip("a\x1bX") == NULL && xrtGetError() != NULL);
    CHECK(same(xansi_strip("plain"), "plain"));
    CHECK(xrtGetError() == NULL);
    CHECK(xansi_display_width("a\n") == -1 && xrtGetError() != NULL);
    CHECK(xansi_display_width("ok") == 2);
    CHECK(xrtGetError() == NULL);
    puts("ansi C boundaries: PASS");
    return 0;
}
