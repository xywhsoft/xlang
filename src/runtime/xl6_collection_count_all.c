#include "xl6_collection_count_all.h"
#include "xl6_collection_count.h"
#include "xl6_list.h"
#include "xl6_set.h"
#include "xl6_dict.h"
#include "src/api/xl_foreign_error.h"
static bool count(const xvalue* source,int64_t* output)
{
    int status=xl6ListErasedProbeV1(source);
    if (status<0) return false;
    if (status) return xl6ListErasedCountV1(source,output);
    status=xl6SetErasedProbeV1(source);
    if (status<0) return false;
    if (status) return xl6SetErasedCountV1(source,false,output);
    status=xl6DictErasedProbeV1(source);
    if (status<0) return false;
    if (status) return xl6DictErasedCountV1(source,false,output);
    *output=xl6CollectionCountV1(source); return !xrtGetError();
}
static bool enter(xl6_error_scope* scope)
{
    if (xl6ErrorScopeBegin(scope)) return true;
    xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return false;
}
static bool finish(xl6_error_scope* scope,bool success)
{
    if (!success || xrtGetError() || scope->failed) xl6ErrorScopeCaptureFailure(scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(scope);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return outcome.ok;
}
int64_t xl6CollectionCountAllV1(const xvalue* source)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return 0;
    int64_t size=0; bool success=count(source,&size);
    return finish(&scope,success) ? size : 0;
}
bool xl6CollectionCountAllIfPresentV1(const xvalue* source,int64_t* output)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return false;
    int64_t size=0; bool present=source && xrtValueType(source)!=XVALUE_NULL;
    bool success=output && (!present || count(source,&size));
    if (!output) xrtSetErrorKind(XERR_ARGUMENT);
    if (!finish(&scope,success)) return false;
    if (present) *output=size;
    return present;
}
xvalue* xl6CollectionCountAllOrNullV1(const xvalue* source)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return NULL;
    int64_t size=0; xvalue* result=NULL;
    if (!source || xrtValueType(source)==XVALUE_NULL) result=xrtValueRetain(xrtValueNull());
    else if (count(source,&size)) result=xrtValueInt(size);
    if (!result || xrtGetError() || scope.failed) {
        xl6ErrorScopeCaptureFailure(&scope); xrtValueRelease(result); result=NULL;
    }
    if (!finish(&scope,result!=NULL)) { xrtValueRelease(result); return NULL; }
    return result;
}
