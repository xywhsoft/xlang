#include "src/runtime/xl6_foreign_resource.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { NO_FAULT, ERROR_SCOPE, ERROR_SCOPE_NO_DIAGNOSTIC, MUTATION_SCOPE };
static unsigned fault, drops;

static bool testErrorScopeBegin(xl6_error_scope* scope)
{
    if(fault==ERROR_SCOPE || fault==ERROR_SCOPE_NO_DIAGNOSTIC){
        if(fault==ERROR_SCOPE)xrtSetErrorKind(XERR_MEMORY);
        return false;
    }
    return xl6ErrorScopeBegin(scope);
}
static bool testMutationBegin(xrtownershipscope* scope)
{
    if(fault==MUTATION_SCOPE)return false;
    return xrtOwnershipMutationBegin(scope);
}
static _Noreturn void testAbort(void)
{
    /* Observe the fatal boundary without a crash dialog/core dump. The child
     * must never resume the runtime after this invariant failure. */
    _Exit(fault && drops==0 ? 77 : 78);
}

/* Inject at this consumer boundary only; the actual XRT and error-scope
 * implementation remain linked unchanged, with no production test hooks. */
#define xl6ErrorScopeBegin testErrorScopeBegin
#define xrtOwnershipMutationBegin testMutationBegin
#define abort testAbort
#include "src/runtime/xl6_foreign_resource.c"
#undef abort
#undef xrtOwnershipMutationBegin
#undef xl6ErrorScopeBegin

static bool dispose(void* context,const xrtcallframe* frame,xrtcallresult* result)
{
    (void)context;(void)result;void* raw=NULL;
    assert(xrtValueGetPointer(frame->Arguments[0],&raw));
    free(raw);++drops;return true;
}
int main(int argc,char** argv)
{
    assert(argc==2);
    unsigned selected=NO_FAULT;
    if(!strcmp(argv[1],"error"))selected=ERROR_SCOPE;
    else if(!strcmp(argv[1],"no-diagnostic"))selected=ERROR_SCOPE_NO_DIAGNOSTIC;
    else if(!strcmp(argv[1],"mutation"))selected=MUTATION_SCOPE;
    else assert(!strcmp(argv[1],"normal"));

    xrtparamdesc parameter={.Name={"raw",3},.Type=xrtTypePointer()};
    xrtfunctionsig signature={.Name={"dispose",7},.ParamCount=1,.Params=&parameter};
    xrtcallable* callable=xrtCallableCreate(&signature,dispose,NULL,NULL);assert(callable);
    xvalue* releaser=xrtValueCallableTake(&callable);assert(releaser);
    void* raw=malloc(1);assert(raw);
    xvalue* token=xl6ForeignResourceCreateV1(raw,releaser);assert(token);
    fault=selected;
    xrtValueRelease(token);
    /* This return would expose the original silently abandoned duty. */
    if(selected!=NO_FAULT)return 1;
    assert(drops==1 && !xrtGetError());
    xrtValueRelease(releaser);
    puts("foreign resource context: PASS");
    return 0;
}
