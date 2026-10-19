#include "xl6_text_value.h"
#include "xl6_channel_deadline_language.h"
#include "xl6_channel_select_deadline.h"
#include "xl6_channel_future_core.h"
#include "xl6_channel_case_array.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
static void failed(void){xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});}
void xl6ChannelDeadlineSelectBuilderDestroyV1(void* frame){xl6ChannelFutureBuilderDestroyV1(frame);}
static bool trace_builder(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* frame=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    return visit&&xrtValueGetHandle(value,&frame,&ops,&user)&&frame&&!user&&
        ops==xl6ChannelDeadlineSelectBuilderHandlePolicyV1Get()->Ops&&visit(xl6ChannelFutureCoreOwnershipV1(frame),context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6ChannelDeadlineSelectBuilderHandlePolicyV1Get,xl6ChannelDeadlineSelectBuilderDestroyV1,trace_builder)
static void* prepare(XL6_ChannelCaseArrayContextV1 context,const xvalue* input,bool relative,int64_t ms,uint64_t until)
{
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope))return NULL;
    XL6_ChannelCasesSnapshotV1 cases={0};XL6_ChannelFutureFrameV1* frame=NULL;xdeadline due=until;bool ok=false;
    /* Budget includes projection/case copies; never restart it after capture. */
    if(relative){if(!xl6DeadlineFromMillisecondsV1(ms,&due))goto done;}
    else if(due==XRT_DEADLINE_NEVER){xrtSetErrorKind(XERR_ARGUMENT);goto done;}
    ok=xl6ChannelCasesSnapshotCaptureV1(&cases,context,input);
    if(ok){frame=xl6ChannelSelectDeadlineRuntimeV1Get()->PrepareUntil(cases.values,cases.count,due);
        ok=frame&&xl6ChannelFutureCorePublishBuilderV1(frame);}
done:
    if(!ok||xrtGetError()||scope.failed)xl6ErrorScopeCaptureFailure(&scope);
    xl6ChannelCasesSnapshotDiscardV1(&cases);
    if(xrtGetError()||scope.failed)xl6ErrorScopeCaptureFailure(&scope);
    if(scope.failed){xl6ChannelFutureCoreDiscardV1(frame);frame=NULL;
        if(xrtGetError()||scope.failed)xl6ErrorScopeCaptureFailure(&scope);}
    xl6_error_result result=xl6ErrorScopeEnd(&scope);
    if(!result.ok){xl6ErrorScopePropagateFailure(result);return NULL;}xrtErrorFree(result.error);return frame;
}
void* xl6ChannelDeadlineSelectForLanguageV1(XL6_ChannelCaseArrayContextV1 context,const xvalue* input,int64_t ms)
{return prepare(context,input,true,ms,0);}
void* xl6ChannelDeadlineSelectUntilLanguageV1(XL6_ChannelCaseArrayContextV1 context,const xvalue* input,uint64_t due)
{return prepare(context,input,false,0,due);}
xvalue* xl6ChannelDeadlineSelectResultLanguageV1(void* frame)
{
    const xvalue* value=xl6ChannelSelectDeadlineRuntimeV1Get()->ResultHandle(frame);
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    if(!value||!xrtValueGetHandle(value,&raw,&ops,&user)||!raw||user||ops!=xl6ChannelSelectionHandlePolicyV1Get()->Ops){
        xrtSetErrorKind(XERR_TYPE);failed();return NULL;}
    xvalue* result=xrtValueRetain(value);if(!result)failed();return result;
}
xfuture* xl6ChannelDeadlineSelectStartCancelLanguageV1(XL6_RuntimeServicesV1 services,
    XL6_ChannelResultFieldContextV1 context,void* frame,const xvalue* owner,xcancel* parent)
{
    if(!services.executor||!services.deadline||!context.fieldName||!context.fieldSize||
        context.policy!=xl6ChannelSelectionHandlePolicyV1Get()){
        xrtSetErrorKind(XERR_ARGUMENT);failed();return NULL;}
    if(!xl6ChannelFutureCoreRetainV1(frame)){xrtSetErrorKind(XERR_STATE);failed();return NULL;}
    xfuture* result=xl6ChannelSelectDeadlineRuntimeV1Get()->Start(services.executor,services.deadline,frame,
        owner,context.fieldName,context.fieldSize,parent);
    if(!result){xl6ChannelFutureCoreDataDropV1(frame);failed();}return result;
}
xfuture* xl6ChannelDeadlineSelectStartLanguageV1(XL6_RuntimeServicesV1 services,
    XL6_ChannelResultFieldContextV1 context,void* frame,const xvalue* owner)
{return xl6ChannelDeadlineSelectStartCancelLanguageV1(services,context,frame,owner,NULL);}
