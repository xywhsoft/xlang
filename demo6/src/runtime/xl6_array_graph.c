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
} GraphContext;

static int copyObject(uint64_t identity, xvaluegraphnextv1* next,
    xvalue** output, ptr context)
{
    const GraphContext* graph=context;
    return graph->object(identity,next,output,graph->user);
}

static void releaseKeepingFailure(xvalue* value)
{
    if (!value) return;
    xerror* failure=xrtTakeError();
    xl6_error_scope cleanup={0};
    if (!xl6ErrorScopeBegin(&cleanup)) abort();
    xrtSetErrorTake(failure); xl6ErrorScopeCaptureFailure(&cleanup);
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

static xvalue* copyGraph(const xvalue* source, GraphContext* graph)
{
    xerror* incoming=xrtTakeError();
    xl6_error_scope operation={0};
    if (!xl6ErrorScopeBegin(&operation)) abort();
    const xvaluegraphcopyv1 config={.Size=sizeof(config),
        .Flags=graph->data ? XVALUE_GRAPH_COPY_DATA_V1 : 0,
        .CopyHandle=copyArray,.UserData=graph,.CopyObject=graph->object ? copyObject : NULL,
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
