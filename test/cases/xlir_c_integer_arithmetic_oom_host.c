/* Real generated error edge, with only diagnostic construction replaced.
 * The failed bool must prevent a global write even when no Error is allocated. */
#include "public.h"
#include <__xl_runtime/xl6_error_text.h>
#include <__xl_runtime/xl_foreign_error.h>
#include <assert.h>
#include <limits.h>
#include <stdio.h>
typedef struct xvalue xvalue;
extern int xl_script_module_init(xvalue *);
extern int xl_script_module_unit_checked(void);
static unsigned failedAllocations;
xerror* __wrap_xl6ErrorCreateV1(const char* kind,xstrview text,int32_t line,int32_t column)
{ (void)kind;(void)text;(void)line;(void)column;++failedAllocations;return NULL; }
int main(void)
{
    assert(xl_script_module_init(NULL));
    assert(checked_native_oom_read()==7 && !failedAllocations);
    xl6_error_scope scope;assert(xl6ErrorScopeBegin(&scope));
    checked_native_oom_add(INT_MAX);
    xl6_error_result failure=xl6ErrorScopeEnd(&scope);
    assert(!failure.ok && !failure.error && failedAllocations==1);
    assert(checked_native_oom_read()==7);
    checked_native_oom_add(1);
    assert(checked_native_oom_read()==8 && failedAllocations==1);
    assert(xl_script_module_unit_checked());
    puts("Generated native integer OOM error edge rejects write and recovers");
    return 0;
}
