#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_MEMORY_DEBUG
#include "../include/qr_native.h"
#include "xrt.h"
#include <stdio.h>
#include <string.h>

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "qr C test failed at line %d: %s\n", __LINE__, #test); \
    return 1; \
} } while (0)

static int test_allocation_failures(void) {
    const char *text = "HELLO WORLD";
    unsigned failure;
    for (failure = 0u; failure < 16u; ++failure) {
        xvalue *matrix;
        bool triggered;
        xrtClearError();
        CHECK(xrtMemDebugFailAfter(failure));
        matrix = xqr_matrix_n(text, strlen(text), "M", 1u);
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            CHECK((matrix == NULL || xrtValueType(matrix) == XVALUE_NULL) &&
                xrtGetError() != NULL &&
                xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        } else CHECK(matrix != NULL && xrtValueType(matrix) == XVALUE_ARRAY);
        if (matrix != NULL) xrtValueRelease(matrix);
    }
    for (failure = 0u; failure < 8u; ++failure) {
        void *ascii;
        bool triggered;
        xrtClearError();
        CHECK(xrtMemDebugFailAfter(failure));
        ascii = xqr_ascii_n(text, strlen(text), "M", 1u, "##", 2u, "  ", 2u);
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            CHECK(ascii == NULL && xrtGetError() != NULL &&
                xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        } else CHECK(ascii != NULL);
        xrtFree(ascii);
    }
    xrtClearError();
    return 0;
}

int main(void) {
    const char *hello = "HELLO WORLD";
    const char *chinese = "你好，世界";
    char long_text[1001];
    char style[1001];
    xvalue *matrix;
    char *svg;
    char *ascii;
    char *utf8;
    char *second;
    memset(long_text, 'A', 1000);
    long_text[1000] = 0;
    memset(style, '#', 1000);
    style[1000] = 0;

    CHECK(xqr_version_n(hello, strlen(hello), "M", 1) == 1);
    CHECK(xqr_version_n(chinese, strlen(chinese), "M", 1) >= 2);
    matrix = xqr_matrix_n(hello, strlen(hello), "M", 1);
    CHECK(matrix != NULL && xrtValueCount(matrix) == 21);
    CHECK(xrtValueCount(xrtValueArrayGet(matrix, 0)) == 21);
    xrtValueRelease(matrix);

    svg = (char *)xqr_svg_n(hello, strlen(hello), "M", 1, 4, 4);
    ascii = (char *)xqr_ascii_n(hello, strlen(hello), "M", 1,
        "##", 2, "  ", 2);
    utf8 = (char *)xqr_utf8_n(hello, strlen(hello), "M", 1);
    CHECK(svg != NULL && ascii != NULL && utf8 != NULL);
    CHECK(strstr(svg, "</svg>") != NULL && strstr(ascii, "##") != NULL);
    CHECK(strstr(utf8, "\xe2\x96\x88") != NULL);
    second = (char *)xqr_svg_n(chinese, strlen(chinese), "H", 1, 4, 4);
    CHECK(second != NULL && second != svg);
    CHECK(strstr(svg, "width=\"116\" height=\"116\"") != NULL);
    CHECK(strstr(svg, "viewBox=\"0 0 116 116\"") != NULL);
    xrtFree(second);
    xrtFree(utf8);
    xrtFree(ascii);
    xrtFree(svg);

    svg = (char *)xqr_svg_n(long_text, 1000, "H", 1, 4, 4);
    CHECK(svg != NULL && strlen(svg) > 80000 && strstr(svg, "</svg>") != NULL);
    xrtFree(svg);
    CHECK(xqr_ascii_n(long_text, 1000, "H", 1,
        style, 1000, "  ", 2) == NULL);
    xrtClearError();
    CHECK(xqr_ascii_n(hello, strlen(hello), "M", 1,
        "#\n", 2, "  ", 2) == NULL);
    xrtClearError();
    CHECK(xqr_ascii_n(hello, strlen(hello), "M", 1,
        "##", 2, "\x1b[31m", 5) == NULL);
    xrtClearError();
    CHECK(xqr_ascii_n(hello, strlen(hello), "M", 1,
        "\xe2\x80\xa8", 3, "  ", 2) == NULL);
    xrtClearError();

    CHECK(xqr_version_n("", 0, "M", 1) == 0);
    xrtClearError();
    CHECK(xqr_version_n("x", 1, "M\0X", 3) == 0);
    xrtClearError();
    CHECK(xqr_version_n("a\0b", 3, "M", 1) == 0);
    xrtClearError();
    CHECK(xqr_version_n("\xc0\xaf", 2, "M", 1) == 0);
    xrtClearError();
    CHECK(xqr_svg_n(hello, strlen(hello), "M", 1, -1, 4) == NULL);
    xrtClearError();
    CHECK(xqr_svg_n(hello, strlen(hello), "M", 1, 4, 0) == NULL);
    CHECK(xrtGetError() != NULL);
    CHECK(xqr_version_n(hello, strlen(hello), "M", 1) == 1);
    CHECK(xrtGetError() == NULL);
    CHECK(xqr_ascii_n(hello, strlen(hello), "M", 1,
        "#\n", 2, "  ", 2) == NULL && xrtGetError() != NULL);
    ascii = (char *)xqr_ascii_n(hello, strlen(hello), "M", 1,
        "##", 2, "  ", 2);
    CHECK(ascii != NULL && xrtGetError() == NULL);
    xrtFree(ascii);
    CHECK(test_allocation_failures() == 0);
    puts("qr C boundaries: PASS");
    return 0;
}
