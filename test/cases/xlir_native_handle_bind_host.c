/* The gate generates this file's included translation unit immediately before
 * compiling the host.  Keeping both sides together lets -Werror validate the
 * exact extern C signatures selected by XLIR. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef XLIR_NATIVE_HANDLE_BIND_RVALUE
#include "../../build/test/xlir_native_handle_bind_rvalue_generated.c"
#else
#include "../../build/test/xlir_native_handle_bind_generated.c"
#endif

typedef struct XLIR_NativeHandleBindBox {
    int64_t value;
} XLIR_NativeHandleBindBox;

static int64_t xlirNativeHandleBindCloseCount = 0;

void* make_borrowed_box(int64_t value)
{
    XLIR_NativeHandleBindBox* box =
        (XLIR_NativeHandleBindBox*)malloc(sizeof(*box));
    if ( box == NULL ) exit(2);
    box->value = value;
    return box;
}

void borrowed_box_close(void* handle)
{
    if ( handle == NULL ) exit(3);
    free(handle);
    xlirNativeHandleBindCloseCount += 1;
}

int64_t borrowed_box_value(void* handle)
{
    const XLIR_NativeHandleBindBox* box =
        (const XLIR_NativeHandleBindBox*)handle;
    if ( box == NULL ) exit(4);
    return box->value;
}

void expect_i64(int64_t actual, int64_t expected)
{
    if ( actual != expected ) {
        fprintf(stderr, "xlir_native_handle_bind: actual=%lld expected=%lld\n",
                (long long)actual, (long long)expected);
        exit(1);
    }
}

int64_t borrowed_close_count(void)
{
    return xlirNativeHandleBindCloseCount;
}

int main(void)
{
    return xlir_module_init() == 1 ? 0 : 1;
}
