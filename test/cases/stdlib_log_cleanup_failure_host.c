#include "src/api/xl_foreign_error.h"
#include "src/runtime/xl6_log.h"
#include <assert.h>
#include <string.h>
/* Supplied with the generated TU by -include. */
int main(void)
{
    assert(xlir_module_init());
    xl6_error_scope cleanup={0};
    assert(xl6ErrorScopeBeginCleanup(&cleanup,false));
    xlir_module_fini();
    xl6_error_result result=xl6ErrorScopeEnd(&cleanup);
    assert(xrtLogDefault()==NULL);
    assert(!result.ok && result.error);
    assert(!strcmp(xrtErrorMessage(result.error),"final flush failure"));
    assert(!strcmp(xrtErrorData(result.error),"ValueError"));
    xrtErrorFree(result.error);
    return 0;
}
