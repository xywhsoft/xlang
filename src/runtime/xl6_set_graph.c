#include "xl6_set_graph.h"
#include "xl6_set_graph_access.h"
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
    (void)sequence;
    XL6_SetGraphViewV1 source={0};
    int status=xl6SetGraphBeginV1(ops,raw,user,identity,&source);
    if (status!=1) return status;
    xvalue* result=data ? xrtValueArray() : xl6SetGraphEmptyV1(&source);
    xtypedsetiter iterator={0}; bool started=false,ready=false;
    if (!result || !xrtTypedSetIterBegin(source.storage,&iterator)) goto done;
    started=true; ready=true;
    for (;;) {
        const void* slot=xrtTypedSetIterNext(&iterator);
        if (xrtGetError()) { ready=false; break; }
        if (!slot) break;
        if (!data && source.layout.value.transfer!=3) {
            bool added=false;
            ready=xl6SetAddV1(result,&source.layout,slot,&added) && added;
            if (!ready) break;
            continue;
        }
        bool owned=source.layout.value.transfer!=3;
        const xvalue* item=owned ? xl6SetGraphProjectSlotV1(&source,slot) : *(xvalue* const*)slot;
        if (!owned && !item) item=xrtValueNull();
        xvalue* child=item ? xrtValueGraphNextV1(next,item) : NULL;
        bool added=false;
        ready=child && (data ? xrtValueArrayAppendTake(result,&child) :
            (xl6SetErasedKeyV1(result,child,XL6_SET_ADD_V1,&added) && added));
        if (!ready) { discard(child); if (owned) discard((xvalue*)item); break; }
        xrtValueRelease(child); if (owned) xrtValueRelease((xvalue*)item);
    }
done:
    if (started) xrtTypedSetIterEnd(&iterator);
    if (ready) { *output=result; result=NULL; }
    else discard(result);
    xl6SetGraphEndV1(&source);
    return ready ? 1 : -1;
}
const XL6_ValueGraphProviderV1* xl6SetGraphProviderV1(void)
{ static const XL6_ValueGraphProviderV1 provider={copy}; return &provider; }
