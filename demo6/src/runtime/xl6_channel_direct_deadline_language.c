#include "xl6_text_value.h"
#include "xl6_channel_deadline_language.h"
#include "xl6_channel_direct_deadline.h"
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
void xl6ChannelDeadlineSendBuilderDestroyV1(void* frame){xl6ChannelFutureBuilderDestroyV1(frame);}
void xl6ChannelDeadlineReceiveBuilderDestroyV1(void* frame){xl6ChannelFutureBuilderDestroyV1(frame);}
static bool trace_builder(const xvalue* value,const XL6_NativeHandlePolicyV1* policy,xrtownershipvisitor visit,void* context)
{
    void* frame=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    return visit&&xrtValueGetHandle(value,&frame,&ops,&user)&&frame&&!user&&ops==policy->Ops&&
        visit(xl6ChannelDirectDeadlineRuntimeV1Get()->Ownership(frame),context);
}
static bool trace_send(const xvalue* value,xrtownershipvisitor visit,void* context)
{return trace_builder(value,xl6ChannelDeadlineSendBuilderHandlePolicyV1Get(),visit,context);}
static bool trace_receive(const xvalue* value,xrtownershipvisitor visit,void* context)
{return trace_builder(value,xl6ChannelDeadlineReceiveBuilderHandlePolicyV1Get(),visit,context);}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6ChannelDeadlineSendBuilderHandlePolicyV1Get,xl6ChannelDeadlineSendBuilderDestroyV1,trace_send)
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6ChannelDeadlineReceiveBuilderHandlePolicyV1Get,xl6ChannelDeadlineReceiveBuilderDestroyV1,trace_receive)
void* xl6ChannelDeadlineSendForLanguageV1(XL6_SequenceLayoutV1 layout,const xvalue* channel,const void* value,int64_t ms)
{return publish(xl6ChannelDirectDeadlineRuntimeV1Get()->PrepareSendFor(layout,channel,value,ms));}
void* xl6ChannelDeadlineSendUntilLanguageV1(XL6_SequenceLayoutV1 layout,const xvalue* channel,const void* value,uint64_t due)
{return publish(xl6ChannelDirectDeadlineRuntimeV1Get()->PrepareSendUntil(layout,channel,value,due));}
void* xl6ChannelDeadlineReceiveForLanguageV1(XL6_SequenceLayoutV1 layout,const xvalue* channel,int64_t ms)
{return publish(xl6ChannelDirectDeadlineRuntimeV1Get()->PrepareReceiveFor(layout,channel,ms));}
void* xl6ChannelDeadlineReceiveUntilLanguageV1(XL6_SequenceLayoutV1 layout,const xvalue* channel,uint64_t due)
{return publish(xl6ChannelDirectDeadlineRuntimeV1Get()->PrepareReceiveUntil(layout,channel,due));}
xvalue* xl6ChannelDeadlineReceiveResultLanguageV1(void* frame)
{
    const xvalue* value=xl6ChannelDirectDeadlineRuntimeV1Get()->ResultHandle(frame);
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    if(!value||!xrtValueGetHandle(value,&raw,&ops,&user)||!raw||user||ops!=xl6ChannelReceiveResultHandlePolicyV1Get()->Ops){
        xrtSetErrorKind(XERR_TYPE);failed();return NULL;}
    xvalue* result=xrtValueRetain(value);if(!result)failed();return result;
}
static xfuture* start(XL6_RuntimeServicesV1 services,void* frame,const xvalue* owner,
    const char* field,size_t size,xcancel* parent,bool sending)
{
    if(!services.executor||!services.deadline){xrtSetErrorKind(XERR_ARGUMENT);failed();return NULL;}
    /* The lexical builder keeps its credit. The queue gets its own before
     * publication, even when notification and completion happen inside Start. */
    if(!xl6ChannelFutureCoreRetainV1(frame)){xrtSetErrorKind(XERR_STATE);failed();return NULL;}
    const XL6_ChannelDirectDeadlineRuntimeV1* api=xl6ChannelDirectDeadlineRuntimeV1Get();
    xfuture* result=sending?api->StartSend(services.executor,services.deadline,frame,parent):
        api->StartReceive(services.executor,services.deadline,frame,owner,field,size,parent);
    if(!result){xl6ChannelFutureCoreDataDropV1(frame);failed();}return result;
}
xfuture* xl6ChannelDeadlineSendStartCancelLanguageV1(XL6_RuntimeServicesV1 services,void* frame,xcancel* parent)
{return start(services,frame,NULL,NULL,0,parent,true);}
xfuture* xl6ChannelDeadlineSendStartLanguageV1(XL6_RuntimeServicesV1 services,void* frame)
{return xl6ChannelDeadlineSendStartCancelLanguageV1(services,frame,NULL);}
xfuture* xl6ChannelDeadlineReceiveStartCancelLanguageV1(XL6_RuntimeServicesV1 services,
    XL6_NativeClassFieldContextV1 context,void* frame,const xvalue* owner,xcancel* parent)
{
    if(!context.fieldName||!context.fieldSize||context.policy!=xl6ChannelReceiveResultHandlePolicyV1Get()){
        xrtSetErrorKind(XERR_ARGUMENT);failed();return NULL;}
    return start(services,frame,owner,context.fieldName,context.fieldSize,parent,false);
}
xfuture* xl6ChannelDeadlineReceiveStartLanguageV1(XL6_RuntimeServicesV1 services,
    XL6_NativeClassFieldContextV1 context,void* frame,const xvalue* owner)
{return xl6ChannelDeadlineReceiveStartCancelLanguageV1(services,context,frame,owner,NULL);}
