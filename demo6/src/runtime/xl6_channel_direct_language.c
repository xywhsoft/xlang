#include "xl6_text_value.h"
#include "xl6_channel_direct_future.h"
#include "xl6_channel_future_core.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>

static void failed(void)
{xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});}
static void* publish(void* frame)
{
    if(frame&&!xl6ChannelFutureCorePublishBuilderV1(frame)){
        xl6_error_scope scope={0};if(!xl6ErrorScopeBeginCleanup(&scope,true))abort();
        xl6ChannelFutureCoreDiscardV1(frame);frame=NULL;
        xl6_error_result result=xl6ErrorScopeEnd(&scope);xl6ErrorScopePropagateFailure(result);
    }
    if(!frame)failed();return frame;
}
void xl6ChannelSendFutureBuilderDestroyV1(void* frame) {xl6ChannelFutureBuilderDestroyV1(frame);}
void xl6ChannelReceiveFutureBuilderDestroyV1(void* frame) {xl6ChannelFutureBuilderDestroyV1(frame);}
static bool trace_builder(const xvalue* value,const XL6_NativeHandlePolicyV1* policy,
    xrtownershipvisitor visit,void* context)
{
    void* frame=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    return visit&&xrtValueGetHandle(value,&frame,&ops,&user)&&frame&&!user&&ops==policy->Ops&&
        visit(xl6ChannelDirectFutureRuntimeV1Get()->Ownership(frame),context);
}
static bool trace_send(const xvalue* value,xrtownershipvisitor visit,void* context)
{return trace_builder(value,xl6ChannelSendFutureBuilderHandlePolicyV1Get(),visit,context);}
static bool trace_receive(const xvalue* value,xrtownershipvisitor visit,void* context)
{return trace_builder(value,xl6ChannelReceiveFutureBuilderHandlePolicyV1Get(),visit,context);}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6ChannelSendFutureBuilderHandlePolicyV1Get,xl6ChannelSendFutureBuilderDestroyV1,trace_send)
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6ChannelReceiveFutureBuilderHandlePolicyV1Get,xl6ChannelReceiveFutureBuilderDestroyV1,trace_receive)
void* xl6ChannelDirectSendCreateLanguageV1(XL6_SequenceLayoutV1 layout,const xvalue* channel,const void* value)
{
    void* frame=xl6ChannelDirectFutureRuntimeV1Get()->PrepareSend(layout,channel,value);
    return publish(frame);
}
void* xl6ChannelDirectReceiveCreateLanguageV1(XL6_SequenceLayoutV1 layout,const xvalue* channel)
{
    void* frame=xl6ChannelDirectFutureRuntimeV1Get()->PrepareReceive(layout,channel);
    return publish(frame);
}
xvalue* xl6ChannelReceiveFutureResultLanguageV1(void* frame)
{
    const xvalue* value=xl6ChannelDirectFutureRuntimeV1Get()->ResultHandle(frame);
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    if(!value||!xrtValueGetHandle(value,&raw,&ops,&user)||!raw||user||
        ops!=xl6ChannelReceiveResultHandlePolicyV1Get()->Ops){
        xrtSetErrorKind(XERR_TYPE);failed();return NULL;
    }
    xvalue* result=xrtValueRetain(value);if(!result)failed();return result;
}
xfuture* xl6ChannelDirectSendStartCancelLanguageV1(XL6_RuntimeServicesV1 services,void* frame,xcancel* parent)
{
    /* NULL carrier requires the scalar/send profile. Retained Start gives the
     * queue its own credit before publication, preserving lexical destruction. */
    xfuture* result=xl6ChannelFutureBuilderStartV1(services.executor,frame,NULL,NULL,0,parent);
    if(!result)failed();
    return result;
}
xfuture* xl6ChannelDirectSendStartLanguageV1(XL6_RuntimeServicesV1 services,void* frame)
{return xl6ChannelDirectSendStartCancelLanguageV1(services,frame,NULL);}
xfuture* xl6ChannelDirectReceiveStartCancelLanguageV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,
    void* frame,const xvalue* owner,xcancel* parent)
{
    if(!services.executor || !context.fieldName || !context.fieldSize ||
        context.policy!=xl6ChannelReceiveResultHandlePolicyV1Get()) {
        xrtSetErrorKind(XERR_ARGUMENT);failed();return NULL;
    }
    xfuture* result=xl6ChannelFutureBuilderStartV1(services.executor,frame,owner,
        context.fieldName,context.fieldSize,parent);
    if(!result)failed();
    return result;
}
xfuture* xl6ChannelDirectReceiveStartLanguageV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,void* frame,const xvalue* owner)
{return xl6ChannelDirectReceiveStartCancelLanguageV1(services,context,frame,owner,NULL);}
int64_t xl6ChannelReceiveResultStatusLanguageV1(const void* source)
{
    int64_t status=-1;if(!xl6ChannelReceiveResultInfoV1(source,&status))failed();return status;
}
void xl6ChannelReceiveResultValueLanguageV1(XL6_FutureValueProfileV1 profile,const void* source,void* output)
{if(!xl6ChannelReceiveResultReadV1(profile,source,output))failed();}
