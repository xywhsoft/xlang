#include "public.h"
#include <__xl_runtime/xl_foreign_error.h>
#include <assert.h>
#include <limits.h>
#include <stdio.h>
typedef struct xvalue xvalue;
extern int xl_script_module_init(xvalue *);
extern int xl_script_main_checked(xvalue *,int *);
extern int xl_script_module_unit_checked(void);
extern void xl_script_print_uncaught(void);
_Static_assert(_Generic(&checked_native_add,int (*)(int,int):1,default:0),"native integer export ABI");
int main(void)
{
    int code=-1;
    assert(xl_script_module_init(NULL));
    assert(checked_native_add(20,22)==42);
    xl6_error_scope scope;assert(xl6ErrorScopeBegin(&scope));
    (void)checked_native_add(INT_MAX,1);
    xl6_error_result failed=xl6ErrorScopeEnd(&scope);
    assert(!failed.ok && failed.error);
    xrtErrorFree(failed.error);
    assert(xl6ErrorScopeBegin(&scope));
    if(!xl_script_main_checked(NULL,&code)) {xl_script_print_uncaught();return 90;}
    xl6_error_result handled=xl6ErrorScopeEnd(&scope);
    assert(handled.ok && !handled.error);
    assert(code==0 && xl_script_module_unit_checked());
    puts("C caller: checked native integer export ABI and overflow passed");
    return 0;
}
