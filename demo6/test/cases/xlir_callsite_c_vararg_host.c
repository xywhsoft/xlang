#ifdef XLIR_C_VARARG_TEXT_BOUNDARY
#include "../../build/test/xlir_c_vararg_text_boundary_generated.c"
#else
#include "../../build/test/xlir_callsite_c_vararg_generated.c"
#endif

#include <stdarg.h>
#include <string.h>

int xlir_c_vararg_sum(const char* tag, ...)
{
    int left;
    int right;
    const char* text;
    va_list args;
    if (strcmp(tag, "sum") != 0) return -1;
    va_start(args, tag);
    left = va_arg(args, int);
    right = va_arg(args, int);
    text = va_arg(args, const char*);
    va_end(args);
    return strcmp(text, "ok") == 0 ? left + right : -1;
}

#ifdef XLIR_C_VARARG_TEXT_BOUNDARY
static int native_calls;
int xlir_c_vararg_mixed(const char* tag, ...)
{
    va_list args;
    int boolean, byte;
    double number;
    const char *first, *second;
    uint64_t wide;
    uint32_t scalar;
    ++native_calls;
    if (strcmp(tag, "mixed")) return -1;
    va_start(args, tag);
    boolean = va_arg(args, int); byte = va_arg(args, int);
    number = va_arg(args, double);
    first = va_arg(args, const char*); second = va_arg(args, const char*);
    wide = va_arg(args, uint64_t); scalar = va_arg(args, uint32_t);
    va_end(args);
    return boolean == 1 && byte == 7 && number == 1.5 &&
        !strcmp(first, "a") && !strcmp(second, "b") &&
        wide == UINT64_MAX && scalar == UINT32_C(0x4f60) ? 42 : -1;
}
#endif

int main(void)
{
#ifdef XLIR_C_VARARG_TEXT_BOUNDARY
    if (!xlir_module_init()) return 1;
    if (xlir_c_vararg_text_good() != 42 || native_calls != 1) return 2;
    if (!xlir_c_vararg_text_nul() || native_calls != 1 || xrtGetError()) return 3;
    xlir_module_fini();
    return 0;
#else
    return xlir_callsite_c_vararg_probe() == 42 ? 0 : 1;
#endif
}
