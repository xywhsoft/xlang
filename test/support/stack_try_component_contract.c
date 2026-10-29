#include "src/compile/xl6_artifact_runtime.h"
#include "src/runtime/xl6_stack_erased_try.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    assert(xl6ArtifactRuntimeSymbolOwner("xl6StackErasedTryCallV1")==XL6_ARTIFACT_STACK_ERASED_TRY);
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_STACK_ERASED_TRY);
    XL6_ArtifactRuntimePlan plan={0};
    assert(xl6ArtifactRuntimeClosePlan(&roots,&plan));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_STACK_ERASED_TRY));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_STACK_ERASED));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SEQUENCE));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_FOREIGN_ERROR));
    size_t sources=0,call=0;
    for(size_t i=0;i<plan.sourceCount;++i)
        sources+=!strcmp(plan.sources[i],"src/runtime/xl6_stack_erased_try.c");
    for(size_t i=0;i<plan.nativeModuleCount;++i)
        call+=!strcmp(plan.nativeModules[i],"XRUNTIME_MODULE_RUNTIME_CALL");
    assert(sources==1 && call==1);
    xrtcallresult result=XRT_CALL_RESULT_INIT;
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
    assert(!xl6StackErasedTryCallV1(xrtValueNull(),0,false,&result));
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(!outcome.ok && !xrtCallResultCount(&result));
    assert(outcome.error && xrtErrorKind(outcome.error)==XERR_TYPE);
    xrtErrorFree(outcome.error); xrtCallResultUnit(&result);
    puts("Stack try provider closure and current-source linkage verified");
    return 0;
}
