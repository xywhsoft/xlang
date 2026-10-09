#include "src/runtime/xl6_array_graph.h"
#include "src/runtime/xl6_sequence_projection.h"
#include "src/runtime/xl6_array_reference_value.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>

typedef struct GraphContext {
    bool data;
    xvaluegraphobjectcopyv1 object;
    ptr user;
    const xvaluegraphsequencev1* sequences;
    size_t sequenceCount;
    const XL6_ValueGraphPlanV1* native;
} GraphContext;
static const XL6_ValueGraphProviderV1* sequenceProvider(const GraphContext* graph,uint64_t identity)
{
    for (size_t i=0;graph->data && graph->native && i<graph->native->sequenceCount;++i)
        if (graph->native->sequences[i].identity==identity) return graph->native->sequences[i].provider;
    return NULL;
}

static int copyObject(uint64_t identity, xvaluegraphnextv1* next,
    xvalue** output, ptr context)
{
    const GraphContext* graph=context;
    if (sequenceProvider(graph,identity)) { xrtSetErrorKind(XERR_TYPE); return -1; }
    return graph->object ? graph->object(identity,next,output,graph->user) : 0;
}

static void releaseKeepingFailure(xvalue* value)
{
    if (!value) return;
    xerror* failure=xrtErrorRef(xrtGetError());
    xl6_error_scope cleanup={0};
    if (!xl6ErrorScopeBeginCleanup(&cleanup,true)) {
        xrtClearError(); xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorRef(failure)});
        xrtClearError(); xrtValueRelease(value); xrtClearError(); xrtSetErrorTake(failure); return;
    }
    xrtErrorFree(failure);
    xrtValueRelease(value);
    xl6_error_result outcome=xl6ErrorScopeEnd(&cleanup);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
}

static int copyArray(const xvaluehandleops* ops, ptr raw, ptr user, uint64_t identity,
    xvaluegraphnextv1* next, xvalue** output, ptr context)
{
    bool data=((const GraphContext*)context)->data;
    XL6_Sequence* source=NULL; XL6_SequenceLayoutV1 layout;
    const xvalueconverter* converter=NULL;
    int status=xl6SequenceGraphBeginV1(ops,raw,user,identity,&source,&layout,&converter);
    if (status!=1) return status;
    XL6_SequenceAdmissionV3 admission={0};
    xvalue* result=NULL; XL6_Sequence* target=NULL;
    if (data) result=xrtValueArray();
    else if (xl6SequenceAdmissionCopyV3(source,&admission)) {
        result=xl6SequenceCreateAdmittedV1(XL6_SEQUENCE_ARRAY_V1,layout.identity,&admission);
        if (result) {
            target=xl6SequenceBeginRoleV1(result,XL6_SEQUENCE_ARRAY_V1,layout.identity,
                layout.elementIdentity,layout.transfer,layout.size,layout.type);
            if (!target || !xl6SequenceBindProjectionV1(target,converter)) goto done;
        }
    }
    if (!result) goto done;
    xtypedarray* storage=xl6SequenceStorageV1(source);
    size_t count=xrtTypedArrayCount(storage);
    bool ready=data ? xrtValueReserve(result,count) : xrtTypedArrayReserve(xl6SequenceStorageV1(target),count);
    for (size_t at=0;ready && at<count;++at) {
        /* Reference slots already carry the exact logical identity. Read the
         * borrowed edge under the owner's operation pin, then let Next own
         * recursion; retaining an active source ancestor here would be
         * rejected by XRT's callback guards before its cycle memo can run. */
        bool ownedItem=layout.transfer!=3;
        xvalue* item=ownedItem ? xl6SequenceProjectionTakeV1(source,xrtTypedArrayGetValue(storage,at,converter)) :
            *(xvalue* const*)xrtTypedArrayConstGet(storage,at);
        if (!ownedItem && !item) item=xrtValueNull();
        xvalue* copy=item ? xrtValueGraphNextV1(next,item) : NULL;
        if (copy) {
            ready=data ? xrtValueArrayAppendTake(result,&copy) :
                xl6SequenceAdmitValueV1(target,copy) &&
                xrtTypedArrayPushValue(xl6SequenceStorageV1(target),copy,converter);
        } else ready=false;
        if (!ready) {
            /* A language destructor has an independent failure bit. Latch
             * the operation's failure before dropping either owned shell. */
            releaseKeepingFailure(copy); if (ownedItem) releaseKeepingFailure(item);
        } else { xrtValueRelease(copy); if (ownedItem) xrtValueRelease(item); }
    }
    if (ready) { *output=result; result=NULL; }
done:
    if (target) xl6SequenceEndV1(target);
    xl6SequenceAdmissionDropV3(&admission);
    releaseKeepingFailure(result);
    xl6SequenceEndV1(source);
    return *output ? 1 : -1;
}

static bool planValid(const GraphContext* graph)
{
    const XL6_ValueGraphPlanV1* plan=graph->native;
    if (!plan) return true;
    if (plan->size!=sizeof(*plan) || plan->providerCount>SIZE_MAX/sizeof(*plan->providers) ||
        plan->sequenceCount>SIZE_MAX/sizeof(*plan->sequences) ||
        (plan->providerCount && !plan->providers) || (plan->sequenceCount && !plan->sequences) ||
        graph->sequenceCount>SIZE_MAX/sizeof(*graph->sequences) || (graph->sequenceCount && !graph->sequences)) return false;
    for (size_t i=0;i<plan->providerCount;++i) {
        const XL6_ValueGraphProviderV1* provider=plan->providers[i];
        if (!provider || !provider->Copy) return false;
        for (size_t j=0;j<i;++j) if (plan->providers[j]==provider) return false;
    }
    for (size_t i=0;i<plan->sequenceCount;++i) {
        const XL6_ValueGraphSequenceV1* sequence=&plan->sequences[i]; bool found=false;
        if (!sequence->identity || !sequence->provider) return false;
        for (size_t j=0;j<plan->providerCount;++j) found=found || plan->providers[j]==sequence->provider;
        if (!found) return false;
        for (size_t j=0;j<i;++j) if (plan->sequences[j].identity==sequence->identity) return false;
        for (size_t j=0;j<graph->sequenceCount;++j) if (graph->sequences[j].TypeId==sequence->identity) return false;
    }
    return true;
}
static int invoke(const XL6_ValueGraphProviderV1* provider,const xvaluehandleops* ops,ptr raw,ptr user,
    uint64_t identity,xvaluegraphnextv1* next,xvalue** output,GraphContext* graph,bool sequence)
{
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorRef(xrtGetError())}); return -1;
    }
    int status=provider ? provider->Copy(ops,raw,user,identity,next,output,graph->data,sequence) :
        copyArray(ops,raw,user,identity,next,output,graph);
    bool valid=(status==0 && !*output) || (status==1 && *output);
    if (!valid && status!=-1 && !xrtGetError() && !scope.failed) xrtSetErrorKind(XERR_STATE);
    if (valid && sequence && (status!=1 || xrtValueType(*output)!=XVALUE_ARRAY)) {
        xrtSetErrorKind(XERR_TYPE); valid=false;
    }
    if (!valid || xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) { xl6ErrorScopePropagateFailure(outcome); return -1; }
    return status;
}
static int copyHandle(const xvaluehandleops* ops,ptr raw,ptr user,uint64_t identity,
    xvaluegraphnextv1* next,xvalue** output,ptr context)
{
    GraphContext* graph=context;
    const XL6_ValueGraphProviderV1* selected=sequenceProvider(graph,identity);
    if (selected) return invoke(selected,ops,raw,user,identity,next,output,graph,true);
    int status=invoke(NULL,ops,raw,user,identity,next,output,graph,false);
    for (size_t i=0;status==0 && graph->native && i<graph->native->providerCount;++i)
        status=invoke(graph->native->providers[i],ops,raw,user,identity,next,output,graph,false);
    return status;
}
static xvalue* copyGraph(const xvalue* source, GraphContext* graph)
{
    if (!planValid(graph)) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    if (source && sequenceProvider(graph,xrtValueTypeId(source)) && xrtValueType(source)!=XVALUE_HANDLE) {
        xrtSetErrorKind(XERR_TYPE); return NULL;
    }
    xerror* incoming=xrtTakeError();
    xl6_error_scope operation={0};
    if (!xl6ErrorScopeBegin(&operation)) {
        xrtErrorFree(incoming);
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorRef(xrtGetError())}); return NULL;
    }
    const xvaluegraphcopyv1 config={.Size=sizeof(config),
        .Flags=graph->data ? XVALUE_GRAPH_COPY_DATA_V1 : 0,
        .CopyHandle=copyHandle,.UserData=graph,.CopyObject=graph->object || graph->native ? copyObject : NULL,
        .Sequences=graph->sequences,.SequenceCount=graph->sequenceCount};
    xvalue* result=xrtValueGraphCopyV1(source,&config);
    if (!result || xrtGetError()) xl6ErrorScopeCaptureFailure(&operation);
    if (operation.failed) { releaseKeepingFailure(result); result=NULL; }
    xl6_error_result outcome=xl6ErrorScopeEnd(&operation);
    if (!outcome.ok) { xrtErrorFree(incoming); xl6ErrorScopePropagateFailure(outcome); }
    else xrtSetErrorTake(incoming);
    return result;
}
xvalue* xl6ValueDeepCloneV1(const xvalue* source)
{ GraphContext graph={.data=false}; return copyGraph(source,&graph); }
xvalue* xl6ValueDataSnapshotV1(const xvalue* source)
{ GraphContext graph={.data=true}; return copyGraph(source,&graph); }
xvalue* xl6ValueDataProjectV1(const xvalue* source, xvaluegraphobjectcopyv1 object, ptr user,
    const xvaluegraphsequencev1* sequences, size_t sequenceCount)
{
    GraphContext graph={.data=true,.object=object,.user=user,
        .sequences=sequences,.sequenceCount=sequenceCount};
    return copyGraph(source,&graph);
}
xvalue* xl6ValueDeepCloneNativeV1(const xvalue* source,const XL6_ValueGraphPlanV1* native)
{ GraphContext graph={.data=false,.native=native}; return copyGraph(source,&graph); }
xvalue* xl6ValueDataSnapshotNativeV1(const xvalue* source,const XL6_ValueGraphPlanV1* native)
{ GraphContext graph={.data=true,.native=native}; return copyGraph(source,&graph); }
xvalue* xl6ValueDataProjectNativeV1(const xvalue* source,xvaluegraphobjectcopyv1 object,ptr user,
    const xvaluegraphsequencev1* sequences,size_t count,const XL6_ValueGraphPlanV1* native)
{
    GraphContext graph={.data=true,.object=object,.user=user,.sequences=sequences,.sequenceCount=count,.native=native};
    return copyGraph(source,&graph);
}
