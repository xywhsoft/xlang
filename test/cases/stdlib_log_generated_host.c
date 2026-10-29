#include "src/api/xl_foreign_error.h"
#include "src/runtime/xl6_log.h"
#include <stdio.h>
#include <assert.h>
/* The generated TU is supplied with -include, as in the shared source gate. */
int main(void)
{
    int ok=xlir_module_init();
#if defined(XLIR_LOG_USER_ENTRY)
    if (ok) {
        xvalue* args=xrtValueArray(); int code=1;
        if (!args) ok=0;
        else { ok=xl_script_main_checked(args,&code) && code==0; xrtValueRelease(args); }
    }
#endif
    xl6_error_scope cleanup={0};
    if (!xl6ErrorScopeBeginCleanup(&cleanup,!ok)) return 1;
    xlir_module_fini();
    xl6_error_result outcome=xl6ErrorScopeEnd(&cleanup);
    if (!outcome.ok) {
        fprintf(stderr,"log module failed: %s\n",xrtErrorMessage(outcome.error));
        xrtErrorFree(outcome.error); return 1;
    }
    assert(xrtLogDefault()==NULL);
    return 0;
}
