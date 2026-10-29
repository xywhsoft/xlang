#include <__xl_runtime/xruntime_decl.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>

void xl6ForeignCallbackFail(void);

char* owned_task_text(int mode)
{
    if (!mode) return NULL;
    if (mode == -2) {
        /* The worker has its own allocation-injection state. */
        if (!xrtMemDebugFailAfter(0)) return NULL;
        char* text = xrtStrDup("native text");
        xrtMemDebugFailClear();
        return text;
    }
    if (mode < 0) {
        char* text = xrtMalloc(16);
        if (text) memset(text, 0xff, 16); /* No terminator on failure. */
        if (mode == -3) { xl6ForeignCallbackFail(); xrtClearError(); }
        else xrtSetErrorKind(XERR_RANGE);
        return text;
    }
    return xrtStrDup("native text");
}
char* owned_task_optional(int mode) { return mode ? owned_task_text(mode) : NULL; }

char* owned_task_scalars(char a, signed char b, unsigned char c, short d,
    unsigned short e, int f, unsigned int g, long h, unsigned long i,
    long long j, unsigned long long k, size_t l, intptr_t m, ptrdiff_t n,
    intptr_t o, uintptr_t p, bool q, float r, double s, wchar_t t)
{
    if (a != CHAR_MIN || b != -128 || c != 255 || d != -32768 || e != 65535 ||
        f != -2147483647 || g != 4294967295U || h != LONG_MIN || i != ULONG_MAX ||
        j != -9223372036854775807LL || k != UINT64_MAX || l != SIZE_MAX ||
        m != INTPTR_MIN || n != PTRDIFF_MIN || o != INTPTR_MIN || p != UINTPTR_MAX ||
        q || r != -1.25f || s != -2.5 || t != WCHAR_MAX) {
        xrtSetErrorKind(XERR_VALUE);
        return NULL;
    }
    return xrtStrDup("native scalars");
}
char owned_task_char_min(void) { return CHAR_MIN; }
long owned_task_long_min(void) { return LONG_MIN; }
unsigned long owned_task_ulong_max(void) { return ULONG_MAX; }
size_t owned_task_size_max(void) { return SIZE_MAX; }
intptr_t owned_task_ssize_min(void) { return INTPTR_MIN; }
ptrdiff_t owned_task_ptrdiff_min(void) { return PTRDIFF_MIN; }
intptr_t owned_task_intptr_min(void) { return INTPTR_MIN; }
uintptr_t owned_task_uintptr_max(void) { return UINTPTR_MAX; }
wchar_t owned_task_wchar_max(void) { return WCHAR_MAX; }

static int visit_count;
void owned_task_visit(bool enabled, double value)
{ if (enabled && value == 2.5) ++visit_count; }
int owned_task_visits(void) { return visit_count; }
bool owned_task_debug(void) { return xrtMemDebugEnabled() || xrtMemDebugEnable(true); }
void owned_task_fail(int64_t prefix) { xrtMemDebugFailAfter((uint64_t)prefix); }
void owned_task_clear(void) { xrtMemDebugFailClear(); xrtClearError(); }
