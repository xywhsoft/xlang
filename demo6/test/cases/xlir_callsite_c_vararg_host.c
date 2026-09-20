#include "../../build/test/xlir_callsite_c_vararg_generated.c"

#include <stdarg.h>
#include <string.h>

int xlir_c_vararg_sum(const char* tag, ...)
{
    int left;
    int right;
    char* text;
    va_list args;
    if (strcmp(tag, "sum") != 0) return -1;
    va_start(args, tag);
    left = va_arg(args, int);
    right = va_arg(args, int);
    text = va_arg(args, char*);
    va_end(args);
    return strcmp(text, "ok") == 0 ? left + right : -1;
}

int main(void)
{
    return xlir_callsite_c_vararg_probe() == 42 ? 0 : 1;
}
