#include "xl6_text_value.h"
#include "xl6_channel_case_array.h"
#include "xl6_channel_future.h"
#include "xl6_channel_future_core.h"
#include "src/api/xl_foreign_error.h"

void* xl6ChannelFutureBuilderCreateLanguageV1(XL6_ChannelCaseArrayContextV1 context,const xvalue* input)
{
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope))return NULL;
    XL6_ChannelCasesSnapshotV1 cases={0};
    const XL6_ChannelFutureRuntimeV1* api=xl6ChannelFutureRuntimeV1Get();
    XL6_ChannelFutureFrameV1* frame=NULL;
    bool ok=xl6ChannelCasesSnapshotCaptureV1(&cases,context,input);
    if(ok){frame=api->Prepare(cases.values,cases.count);ok=frame&&xl6ChannelFutureCorePublishBuilderV1(frame);}
    if(!ok || xrtGetError() || scope.failed)xl6ErrorScopeCaptureFailure(&scope);
    xl6ChannelCasesSnapshotDiscardV1(&cases);
    if(xrtGetError() || scope.failed)xl6ErrorScopeCaptureFailure(&scope);
    if(scope.failed){api->Discard(frame);frame=NULL;
        if(xrtGetError() || scope.failed)xl6ErrorScopeCaptureFailure(&scope);}
    xl6_error_result result=xl6ErrorScopeEnd(&scope);
    if(!result.ok){xl6ErrorScopePropagateFailure(result);return NULL;}
    xrtErrorFree(result.error);return frame;
}
xfuture* xl6ChannelFutureBuilderStartCancelLanguageV1(XL6_RuntimeServicesV1 services,XL6_ChannelResultFieldContextV1 context,
    void* frame,const xvalue* owner,xcancel* parent)
{
    if(!services.executor || !context.fieldName || !context.fieldSize ||
        context.policy!=xl6ChannelSelectionHandlePolicyV1Get()){
        xrtSetErrorKind(XERR_ARGUMENT);
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return NULL;
    }
    xfuture* result=xl6ChannelFutureBuilderStartV1(services.executor,frame,owner,
        context.fieldName,context.fieldSize,parent);
    if(!result)xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
    return result;
}
xfuture* xl6ChannelFutureBuilderStartLanguageV1(XL6_RuntimeServicesV1 services,XL6_ChannelResultFieldContextV1 context,
    void* frame,const xvalue* owner)
{return xl6ChannelFutureBuilderStartCancelLanguageV1(services,context,frame,owner,NULL);}
