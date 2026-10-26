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

const char* variadic_borrowed(int mode, ...)
{
    ++call_count;
    return mode ? "borrowed" : NULL;
}
char* variadic_failed(int mode, ...)
{
    (void)mode; ++call_count;
    /* Deliberately unterminated: failure must precede strlen. */
    char* text = xrtMalloc(16);
    if (text) memset(text, 0xff, 16);
    xrtSetErrorKind(XERR_RANGE);
    return text;
}

char* variadic_required(int mode, ...)
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

void xl6ForeignCallbackFail(void);
char* variadic_silent_failure(int mode, ...)
{
    (void)mode;
    char* text=xrtMalloc(16);
    if(text)memset(text,0xff,16);
    xl6ForeignCallbackFail(); xrtClearError();
    return text;
}
int variadic_fixed_text(const char* format, ...)
{
    va_list args; va_start(args,format);
    int value=va_arg(args,int);
    va_end(args); return (int)strlen(format)+value;
}
bool variadic_debug(void) { return xrtMemDebugEnabled() || xrtMemDebugEnable(true); }
int64_t variadic_live(void) { xmemdebugsnapshot snapshot={0}; xrtMemDebugSnapshot(&snapshot); return snapshot.LiveCount; }
void variadic_fail_after(int64_t count) { xrtMemDebugFailAfter((uint64_t)count); }
void variadic_clear_failure(void) { xrtMemDebugFailClear(); xrtClearError(); }
int variadic_scalar_failed(int mode, ...)
{
    (void)mode; xrtSetErrorKind(XERR_RANGE); return 123;
}
