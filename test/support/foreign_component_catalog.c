#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void require(unsigned root,unsigned dependency,bool expected)
{
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(root);
    XL6_ArtifactRuntimePlan plan={0};assert(xl6ArtifactRuntimeClosePlan(&roots,&plan));
    assert((xl6ArtifactRuntimeSetHas(&plan.components,dependency)!=0)==expected);
    assert(plan.sourceCount && plan.sourceCount<=XL6_ARTIFACT_RUNTIME_COUNT && !plan.hostAbi);
    for(size_t i=0;i<plan.sourceCount;++i)
        for(size_t j=0;j<i;++j)assert(strcmp(plan.sources[i],plan.sources[j]));
}
int main(void)
{
    const struct {const char* name;unsigned owner;} symbols[]={
        {"xl6ForeignContextReadyV1",XL6_ARTIFACT_FOREIGN_CONTEXT},
        {"xl6ForeignCallbackEnterV1",XL6_ARTIFACT_FOREIGN_CALLBACK_SCOPE},
        {"xl6ForeignViewCallBeginV1",XL6_ARTIFACT_FOREIGN_VIEW},
        {"xl6ForeignViewBorrowRootV1",XL6_ARTIFACT_FOREIGN_VIEW},
        {"xl6ForeignViewBoxV1",XL6_ARTIFACT_FOREIGN_VIEW_VALUE},
        {"xl6OwnershipDomainForeignViewValueV1Get",XL6_ARTIFACT_FOREIGN_VIEW_VALUE},
        {"xl6ForeignViewNativePinV1",XL6_ARTIFACT_FOREIGN_VIEW},
        {"xl6NativeCallbackRuntimeV1Get",XL6_ARTIFACT_NATIVE_CALLBACK},
        {"xl6OwnershipDomainNativeCallbackV1Get",XL6_ARTIFACT_NATIVE_CALLBACK},
        {"xl6OwnershipDomainForeignViewV1Get",XL6_ARTIFACT_FOREIGN_VIEW},
        {"xl6NativeGraphCaptureProfileV1Get",XL6_ARTIFACT_NATIVE_CAPTURE_GRAPH},
        {"xl6ForeignViewCaptureProfileV1Get",XL6_ARTIFACT_FOREIGN_VIEW_CAPTURE},
        {"xl6CaptureRuntimeV3Get",XL6_ARTIFACT_CAPTURE_V3},
        {"xl6ActivationCaptureRuntimeV3Get",XL6_ARTIFACT_CAPTURE_V3},
        {"xl6ClosureRuntimeV4Get",XL6_ARTIFACT_CLOSURE_CAPTURE_V4},
        {"xl6CellCreateGraphV1",XL6_ARTIFACT_CELL},
        {"xl6CellCreateNativeGraphV3",XL6_ARTIFACT_CELL_NATIVE_GRAPH},
        {"xl6NativeGraphFuturePayloadRuntimeV1Get",XL6_ARTIFACT_NATIVE_GRAPH_FUTURE_PAYLOAD},
        {"xl6OwnershipDomainNativeGraphFuturePayloadV1Get",XL6_ARTIFACT_OWNERSHIP_DOMAIN_NATIVE_GRAPH_FUTURE_PAYLOAD}};
    for(size_t i=0;i<sizeof(symbols)/sizeof(symbols[0]);++i){
        assert(xl6ArtifactRuntimeSymbolOwner(symbols[i].name)==symbols[i].owner);
        require(symbols[i].owner,symbols[i].owner,true);
    }
    require(XL6_ARTIFACT_CELL,XL6_ARTIFACT_FOREIGN_ERROR,true);
    require(XL6_ARTIFACT_NATIVE_CALLBACK,XL6_ARTIFACT_FOREIGN_VIEW,true);
    require(XL6_ARTIFACT_NATIVE_CALLBACK,XL6_ARTIFACT_CLOSURE,true);
    require(XL6_ARTIFACT_FOREIGN_VIEW_VALUE,XL6_ARTIFACT_FOREIGN_VIEW,true);
    require(XL6_ARTIFACT_CLOSURE,XL6_ARTIFACT_FOREIGN_CALLBACK_SCOPE,true);
    require(XL6_ARTIFACT_CODE_FINALIZER,XL6_ARTIFACT_FOREIGN_CALLBACK_SCOPE,true);
    require(XL6_ARTIFACT_FOREIGN_CALLBACK_SCOPE,XL6_ARTIFACT_FOREIGN_CONTEXT,true);
    require(XL6_ARTIFACT_FOREIGN_VIEW_CAPTURE,XL6_ARTIFACT_FOREIGN_VIEW,true);
    require(XL6_ARTIFACT_CAPTURE_V3,XL6_ARTIFACT_NATIVE_CAPTURE_GRAPH,true);
    require(XL6_ARTIFACT_CLOSURE_CAPTURE_V4,XL6_ARTIFACT_CAPTURE_V3,true);
    require(XL6_ARTIFACT_CELL_NATIVE_GRAPH,XL6_ARTIFACT_NATIVE_CAPTURE_GRAPH,true);
    require(XL6_ARTIFACT_NATIVE_GRAPH_FUTURE_PAYLOAD,XL6_ARTIFACT_FOREIGN_CALLBACK_SCOPE,true);
    require(XL6_ARTIFACT_OWNERSHIP_DOMAIN_NATIVE_GRAPH_FUTURE_PAYLOAD,XL6_ARTIFACT_NATIVE_GRAPH_FUTURE_PAYLOAD,true);
    /* A generic graph storage capability does not pull in a UI view registry. */
    const unsigned generic[]={XL6_ARTIFACT_CELL,XL6_ARTIFACT_CLOSURE,XL6_ARTIFACT_CODE_FINALIZER,
        XL6_ARTIFACT_NATIVE_CAPTURE_GRAPH,XL6_ARTIFACT_CAPTURE_V3,XL6_ARTIFACT_CLOSURE_CAPTURE_V4,
        XL6_ARTIFACT_CELL_NATIVE_GRAPH,XL6_ARTIFACT_NATIVE_GRAPH_FUTURE_PAYLOAD};
    for(size_t i=0;i<sizeof(generic)/sizeof(generic[0]);++i)require(generic[i],XL6_ARTIFACT_FOREIGN_VIEW,false);
    puts("native component catalog: exact symbol owners, complete callback dependencies, unique sources and generic/view separation passed");return 0;
}
