#include "src/runtime/xl6_list_graph.h"
#include "src/runtime/xl6_list_graph_access.h"
#include "src/api/xl_foreign_error.h"

static void discard(xvalue* value)
{
    if (!value) return;
    xerror* incoming=xrtErrorRef(xrtGetError()); xl6_error_scope cleanup={0};
    if (xl6ErrorScopeBeginCleanup(&cleanup,true)) {
        xrtErrorFree(incoming); xrtValueRelease(value);
        xl6_error_result outcome=xl6ErrorScopeEnd(&cleanup);
        if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    } else {
        xrtClearError();
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorRef(incoming)});
        xrtClearError(); xrtValueRelease(value); xrtClearError(); xrtSetErrorTake(incoming);
    }
}
static int copy(const xvaluehandleops* ops,ptr raw,ptr user,uint64_t identity,
    xvaluegraphnextv1* next,xvalue** output,bool data,bool sequence)
{
    XL6_ListGraphViewV1 source={0};
    int status=xl6ListGraphBeginV1(ops,raw,user,identity,&source);
    if (status!=1) return status;
    xvalue* result=data ? (sequence ? xrtValueArray() : xrtValueIntMap()) : xl6ListGraphEmptyV1(&source);
    xtypedlistiter iterator={0}; bool started=false,ready=false;
    if (!result || !xrtTypedListIterBegin(source.storage,&iterator)) goto done;
    started=true; ready=true;
    for (;;) {
        int64_t key=0; const void* slot=xrtTypedListIterNext(&iterator,&key);
        if (xrtGetError()) { ready=false; break; }
        if (!slot) break;
        /* Non-reference native copies use the actual typed slot directly:
         * one text allocation, no temporary boxing or decode/copy cycle.
         * Future leaf semantics retain the actual consumer control block. */
        if (!data && source.layout.value.transfer!=3) {
            ready=xl6ListSetV1(result,&source.layout,key,slot);
            if (!ready) break;
            continue;
        }
        bool owned=source.layout.value.transfer!=3;
        const xvalue* item=owned ? xl6ListGraphProjectSlotV1(&source,slot) : *(xvalue* const*)slot;
        if (!owned && !item) item=xrtValueNull();
        xvalue* child=item ? xrtValueGraphNextV1(next,item) : NULL;
        ready=child && (data ? (sequence ? xrtValueArrayAppendTake(result,&child) :
            xrtValueIntMapSetTake(result,key,&child)) : xl6ListErasedSetV1(result,key,child));
        if (!ready) { discard(child); if (owned) discard((xvalue*)item); break; }
        xrtValueRelease(child); if (owned) xrtValueRelease((xvalue*)item);
    }
done:
    if (started) xrtTypedListIterEnd(&iterator);
    if (ready) { *output=result; result=NULL; }
    else discard(result);
    xl6ListGraphEndV1(&source);
    return ready ? 1 : -1;
}
const XL6_ValueGraphProviderV1* xl6ListGraphProviderV1(void)
{ static const XL6_ValueGraphProviderV1 provider={copy}; return &provider; }
