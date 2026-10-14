#include "src/runtime/xl6_dynamic_product.h"
#include "src/api/xl_foreign_error.h"

static void capture(xl6_error_scope* scope)
{ if(xrtGetError())xl6ErrorScopeCaptureFailure(scope); }
static void arguments(xl6_error_scope* scope,size_t count,xvalue** values)
{
    if(!values)return;
    for(size_t i=0;i<count;++i) { xrtValueRelease(values[i]); capture(scope); }
}
/* Replace each owned result individually so every destructor's first error
 * is captured before the next destructor can change the native diagnostic.
 * The final vector unit only owns canonical null sentinels and capacity. */
static void results(xl6_error_scope* scope,xrtcallresult* result)
{
    size_t count=xrtCallResultCount(result);
    for(size_t i=0;i<count;++i) {
        xvalue* nil=xrtValueRetain(xrtValueNull());
        if(!nil || !xrtCallResultSetTake(result,i,&nil))xl6ErrorScopeCaptureFailure(scope);
        capture(scope); xrtValueRelease(nil); capture(scope);
    }
    xrtCallResultUnit(result); capture(scope);
}
static bool invokeProduct(const xvalue* callable,XL6_DynamicOwnedDispatchV1 dispatch,size_t argumentCount,xvalue** positional,
    size_t keywordCount,const xstrview* names,xvalue** keywords,size_t expected,xvalue*** output,bool exact)
{
    xl6_error_scope scope={0};
    if(!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
        xl6_error_scope cleanup={0};
        if(xl6ErrorScopeBeginCleanup(&cleanup,true)) {
            arguments(&cleanup,argumentCount,positional); arguments(&cleanup,keywordCount,keywords);
            xl6ErrorScopePropagateFailure(xl6ErrorScopeEnd(&cleanup));
        } else {
            xerror* first=xrtTakeError();
            if(positional)for(size_t i=0;i<argumentCount;++i)xrtValueRelease(positional[i]);
            if(keywords)for(size_t i=0;i<keywordCount;++i)xrtValueRelease(keywords[i]);
            xrtClearError(); xl6ErrorScopePropagateFailure((xl6_error_result){false,first});
        }
        return false;
    }
    xrtcallresult result=XRT_CALL_RESULT_INIT;
    xvalue* local[XRT_CALL_RESULT_INLINE_COUNT]={0};
    xvalue** prepared=local; size_t retained=0; bool success=false,published=false;
    if(!callable || expected<(exact?2u:1u) || expected>SIZE_MAX/sizeof(*prepared) || !output ||
       argumentCount>SIZE_MAX-keywordCount || (argumentCount && !positional) ||
       (keywordCount && (!keywords || !names))) { xrtSetErrorKind(XERR_ARGUMENT); goto done; }
    for(size_t i=0;i<expected;++i) {
        if(!output[i] || *output[i]) { xrtSetErrorKind(XERR_ARGUMENT); goto done; }
        for(size_t j=0;j<i;++j)if(output[i]==output[j]) { xrtSetErrorKind(XERR_ARGUMENT); goto done; }
    }
    if(expected>XRT_CALL_RESULT_INLINE_COUNT) {
        prepared=xrtCalloc(expected,sizeof(*prepared));
        if(!prepared) { xrtSetErrorKind(XERR_MEMORY); goto done; }
    }
    xrtcallframe frame={.ArgumentCount=argumentCount,.Arguments=positional,
        .KeywordCount=keywordCount,.KeywordNames=names,.KeywordValues=keywords};
    bool invoked;
    if(dispatch) {
        invoked=dispatch((xvalue*)callable,argumentCount,positional,keywordCount,names,keywords,&result);
        argumentCount=keywordCount=0; /* Dispatcher consumed all values. */
    } else invoked=xrtValueInvoke(callable,&frame,&result);
    if(!invoked) {
        if(!scope.failed) {
            xerror* error=xrtTakeError(); const xerror* cause=xrtErrorCause(error);
            xerror* primary=cause ? xrtErrorRef(cause) : NULL;
            if(primary) { xrtErrorFree(error); error=primary; }
            xrtSetErrorTake(error);
        }
        goto done;
    }
    if(xrtGetError() || scope.failed)goto done;
    if(exact ? xrtCallResultCount(&result)!=expected : xrtCallResultCount(&result)<expected) { xrtSetErrorKind(XERR_TYPE); goto done; }
    for(size_t i=0;i<expected;++i) {
        prepared[i]=xrtValueRetain(xrtCallResultGet(&result,i));
        if(prepared[i])++retained;
        if(!prepared[i] || xrtGetError() || scope.failed)goto done;
    }
    success=true;
done:
    if(!success || xrtGetError())xl6ErrorScopeCaptureFailure(&scope);
    arguments(&scope,argumentCount,positional); arguments(&scope,keywordCount,keywords);
    results(&scope,&result);
    if(scope.failed)for(size_t i=0;i<retained;++i) { xrtValueRelease(prepared[i]); prepared[i]=NULL; capture(&scope); }
    else { for(size_t i=0;i<expected;++i)*output[i]=prepared[i]; published=true; }
    if(prepared!=local)xrtFree(prepared);
    capture(&scope);
    if(scope.failed && published)for(size_t i=0;i<expected;++i) {
        xrtValueRelease(*output[i]); *output[i]=NULL; capture(&scope);
    }
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok)xl6ErrorScopePropagateFailure(outcome);
    return success && outcome.ok;
}
bool xl6DynamicFunctionProductTakeV1(const xvalue* callable,size_t argc,xvalue** argv,size_t kwc,
    const xstrview* names,xvalue** keywords,size_t expected,xvalue*** output)
{ return invokeProduct(callable,NULL,argc,argv,kwc,names,keywords,expected,output,true); }
bool xl6DynamicDispatchProductTakeV1(xvalue* receiver,XL6_DynamicOwnedDispatchV1 dispatch,size_t argc,xvalue** argv,size_t kwc,
    const xstrview* names,xvalue** keywords,size_t expected,xvalue*** output)
{
    /* Missing dispatch is a physical misuse, never an implicit callable path. */
    return invokeProduct(dispatch?receiver:NULL,dispatch,argc,argv,kwc,names,keywords,expected,output,true);
}
xvalue* xl6DynamicDispatchFirstTakeV1(xvalue* receiver,XL6_DynamicOwnedDispatchV1 dispatch,size_t argc,xvalue** argv,size_t kwc,
    const xstrview* names,xvalue** keywords)
{
    xvalue* result=NULL; xvalue** output[]={&result};
    return invokeProduct(dispatch?receiver:NULL,dispatch,argc,argv,kwc,names,keywords,1,output,false)?result:NULL;
}
