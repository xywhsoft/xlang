#include "src/runtime/xl6_text_value.h"
#include "src/api/xl_foreign_error.h"
#include "release/include/xrt_decl.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern bool xl6_list_recipe_probe(void);
/* Interpose only generated-C Begin calls, not resident runtime internals.
 * This models entry refusal WITHOUT a diagnostic and must never abort or
 * manufacture success. Real scopes still enter/end in stack order. */
static size_t scopeFailPoint=SIZE_MAX,scopeCalls;
static bool scopeFailHit;
bool xl6_list_recipe_scope_begin(xl6_error_scope* scope)
{
    if (scopeCalls++==scopeFailPoint) { scopeFailHit=true; return false; }
    return xl6ErrorScopeBegin(scope);
}
int main(int argc,char** argv)
{
    assert(argc==2 && (!strcmp(argv[1],"vectors") || !strcmp(argv[1],"oom") || !strcmp(argv[1],"scopes")));
    bool fault=!strcmp(argv[1],"oom");
    bool scopes=!strcmp(argv[1],"scopes");
    for (size_t point=0;point<4097;++point) {
        xl6ProductMemoryBegin(SIZE_MAX);
        xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
        scopeFailPoint=scopes ? point : SIZE_MAX; scopeCalls=0; scopeFailHit=false;
        if (fault) assert(xrtMemDebugFailAfter(point));
        bool ok=xl6_list_recipe_probe(); bool hit=scopes ? scopeFailHit : fault && xrtMemDebugFailTriggered();
        if (!ok) xl6ErrorScopeCaptureFailure(&scope);
        xrtMemDebugFailClear(); xl6_error_result result=xl6ErrorScopeEnd(&scope);
        assert(hit ? !ok && !result.ok : ok && result.ok);
        if (hit && fault) assert(result.error && xrtErrorKind(result.error)==XERR_MEMORY);
        if (hit && scopes) assert(!result.error);
        xrtErrorFree(result.error); assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
        printf("List target %s point=%zu hit=%d balanced\n",scopes ? "scope" : "allocation",point,hit);
        if (!hit) { printf("List targets closed-batch runs=%zu first-no-hit=%zu\n",point+1,point); return 0; }
    }
    assert(!"List target allocation prefix not closed"); return 1;
}
