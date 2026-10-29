#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#include "../include/barcode_native.h"
#include "xrt.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    const char code[] = "690123456789";
    const char isbn[] = "9780306406157";
    xvalue *modules = xb_ean13(code, 12);
    void *ascii = xb_ascii(code, 12, false);
    void *upc_ascii = xb_ascii("12345678901", 11, true);
    void *svg = xb_svg("12345678901", 11, true, 50, 2);
    int ok = modules != NULL && xrtValueType(modules) == XVALUE_ARRAY &&
        ascii != NULL && upc_ascii != NULL && svg != NULL;
    if (ok) {
        printf("code: %s check digit: %lld\n", code,
            (long long)xb_checksum_ean(code, 12));
        printf("modules: %lld\n", (long long)xrtValueCount(modules));
        puts((const char *)ascii);
        printf("isbn valid: %s\n", xb_validate_ean(isbn, 13) ? "true" : "false");
        puts((const char *)upc_ascii);
        printf("upc svg bytes: %s\n", strlen((const char *)svg) > 0 ? "true" : "false");
        puts("barcode example: PASS");
    }
    xrtValueRelease(modules);
    xrtFree(ascii);
    xrtFree(upc_ascii);
    xrtFree(svg);
    return ok ? 0 : 1;
}

/* TCC 单文件范例编入实现；正式包仍按清单单独编译。 */
#ifdef __TINYC__
#include "../barcode_native.c"
#endif
