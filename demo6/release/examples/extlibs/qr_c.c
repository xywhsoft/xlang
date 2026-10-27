#include "../../extlibs/qr/include/qr_native.h"
#include "../../extlibs/qr/qr_native.c"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *text = "HELLO WORLD";
    const char *mixed = "1234567890123456789012345678901234567890"
        "a1234567890123456789012345678901234567890";
    char *svg = (char *)xqr_svg_n(text, strlen(text), "M", 1, 4, 4);
    if (svg == NULL) return 1;
    printf("version: %" PRId64 "\nsvg bytes: %zu\nmixed version: %" PRId64
        "\nqr example: PASS\n",
        xqr_version_n(text, strlen(text), "M", 1), strlen(svg),
        xqr_version_n(mixed, strlen(mixed), "M", 1));
    xrtFree(svg);
    return 0;
}
