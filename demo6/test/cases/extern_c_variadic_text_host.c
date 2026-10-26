#include <__xl_runtime/xruntime_decl.h>
#include <stdarg.h>
#include <string.h>

static int call_count;
static char* last;
char* variadic_text(int mode, ...)
{
    ++call_count;
    if (!mode) return NULL;
    va_list args; va_start(args, mode);
    const char* text = va_arg(args, const char*);
    int integer = va_arg(args, int);       /* default integer promotion */
    double real = va_arg(args, double);   /* float promotes to double */
    va_end(args);
    if (integer != 7 || real != 2.5) return NULL;
    last = xrtStrDup(text);
    return last;
}
int variadic_length(int mode, ...)
{
    ++call_count;
    va_list args; va_start(args, mode);
    const char* text = va_arg(args, const char*);
    int length = (int)strlen(text);
    va_end(args); return length;
}
void variadic_visit(int mode, ...)
{
    ++call_count;
    va_list args; va_start(args, mode);
    (void)va_arg(args, const char*);
    va_end(args);
}
int variadic_calls(void) { return call_count; }
bool variadic_original(const char* text) { return text == last; }
