#include "xl6_dict_graph.h"
#include "xl6_dict_graph_access.h"
#include "src/api/xl_foreign_error.h"

static bool finish(xl6_error_scope* scope,bool ok)
{
    if (!ok || xrtGetError() || scope->failed) xl6ErrorScopeCaptureFailure(scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(scope);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return outcome.ok;
}
static int copy(const xvaluehandleops* ops,ptr raw,ptr user,uint64_t identity,
    xvaluegraphnextv1* next,xvalue** output,bool data,bool sequence)
{
    XL6_DictGraphViewV1 source={0};
    int status=xl6DictGraphBeginV1(ops,raw,user,identity,&source);
    if (status!=1) return status;
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
        xl6DictGraphEndV1(&source); return -1;
    }
    xvalue* result=NULL; xtypeddictiter iterator={0}; bool started=false,ready=false;
    if (!next || !output || *output || sequence) { xrtSetErrorKind(XERR_ARGUMENT); goto done; }
    result=data ? xrtValueObject() : xl6DictGraphEmptyV1(&source);
    if (!result || !xrtTypedDictIterBegin(source.storage,&iterator)) goto done;
    started=true;
    for (;;) {
        xstrview key={0}; const void* slot=xrtTypedDictIterNext(&iterator,&key);
        if (xrtGetError() || scope.failed) break;
        if (!slot) { ready=true; break; }
        /* Native POD/string/Future slots copy directly. Reference edges and
         * data projections recurse through the SAME supplied memo. */
        if (!data && source.layout.value.transfer!=3) {
            if (!xl6DictSetV1(result,&source.layout,key,slot)) break;
            continue;
        }
        bool owned=source.layout.value.transfer!=3;
        const xvalue* item=owned ? xl6DictGraphProjectSlotV1(&source,slot) : *(xvalue* const*)slot;
        if (!owned && !item) item=xrtValueNull();
        xvalue* child=item ? xrtValueGraphNextV1(next,item) : NULL;
        bool stored=child && (data ? xrtValueObjectSetTake(result,key,&child) : xl6DictErasedSetV1(result,key,child));
        if (!stored || xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
        xrtValueRelease(child); if (owned) xrtValueRelease((xvalue*)item);
        if (!stored || xrtGetError() || scope.failed) break;
    }
done:
    if (!ready || xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
    if (started) xrtTypedDictIterEnd(&iterator);
    xl6DictGraphEndV1(&source);
    if (xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
    if (scope.failed) { xrtValueRelease(result); result=NULL; }
    if (!finish(&scope,ready)) { xrtValueRelease(result); return -1; }
    *output=result; return 1;
}
const XL6_ValueGraphProviderV1* xl6DictGraphProviderV1(void)
{ static const XL6_ValueGraphProviderV1 provider={copy}; return &provider; }
