#include "xl6_native_class_borrow.h"
#include "xl6_net_datagram_batch_internal.h"
#include "src/api/xl_foreign_error.h"
static void failed(void){xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});}
void xl6NetDatagramPacketDestroyV1(void* raw)
{
    XL6_NetDatagramPacketV1* packet=raw;if(!packet)return;
    xvalue* buffer=packet->buffer;packet->buffer=NULL;xrtValueRelease(buffer);xrtFree(packet);
}
static void* takeBuffer(xvalue* buffer)
{
    XL6_NetDatagramPacketV1* packet=buffer?xrtCalloc(1,sizeof(*packet)):NULL;
    if(packet){packet->buffer=buffer;packet->status=XNET_RESULT_ERROR;}
    else {xerror* first=xrtTakeError();xrtValueRelease(buffer);xrtClearError();xrtSetErrorTake(first);failed();}
    return packet;
}
void* xl6NetDatagramPacketCreateV1(int64_t capacity)
{
    void* buffer=xl6BufferCreateReserveV1(capacity);
    return takeBuffer(buffer?xl6NativeHandleTakeV1(&buffer,xl6BufferHandlePolicyV1Get()):NULL);
}
void* xl6NetDatagramPacketCreateBufferV1(XL6_NativeClassFieldContextV1 context,const xvalue* object)
{
    void* raw=NULL;xvalue* handle=xl6NativeClassFieldRetainV1(context,object,xl6BufferHandlePolicyV1Get(),&raw);
    if(handle&&!xl6BufferChecked(raw)){xerror* first=xrtTakeError();xrtValueRelease(handle);xrtClearError();xrtSetErrorTake(first);handle=NULL;}
    return takeBuffer(handle);
}
static void* copyBytes(xbytesview bytes,int64_t size)
{
    if(!xl6BufferSelect(&bytes,size)){failed();return NULL;}
    void* raw=xl6BufferCreateV1();if(!raw){failed();return NULL;}
    if(!xrtBufferAppend(raw,bytes)){xl6BufferDestroyV1(raw);failed();return NULL;}
    return takeBuffer(xl6NativeHandleTakeV1(&raw,xl6BufferHandlePolicyV1Get()));
}
void* xl6NetDatagramPacketCreateTextV1(xstrview text,int64_t size)
{return copyBytes((xbytesview){(const uint8_t*)text.Data,text.Size},size);}
void* xl6NetDatagramPacketCreateBytesV1(const xvalue* value,int64_t size)
{xbytesview bytes;if(!xl6BufferBytesInput(value,size,&bytes)){failed();return NULL;}return copyBytes(bytes,-1);}
xvalue* xl6NetDatagramPacketBufferHandleV1(const void* raw)
{
    const XL6_NetDatagramPacketV1* packet=raw;
    if(!xl6NetDatagramPacketBufferV1(packet)){failed();return NULL;}
    xvalue* result=xrtValueRetain(packet->buffer);if(!result)failed();return result;
}
void xl6NetDatagramPacketTargetSetV1(void* raw,const void* target)
{
    XL6_NetDatagramPacketV1* packet=raw;const xnetaddr* address=target;
    if(!packet||!address||(address->Family!=4&&address->Family!=6)){xrtSetErrorKind(XERR_ARGUMENT);failed();return;}
    packet->target=*address;packet->hasTarget=true;
}
void xl6NetDatagramPacketTargetClearV1(void* raw)
{if(raw){XL6_NetDatagramPacketV1* packet=raw;packet->target=(xnetaddr){0};packet->hasTarget=false;}}
bool xl6NetDatagramPacketHasTargetV1(const void* raw)
{return raw&&((const XL6_NetDatagramPacketV1*)raw)->hasTarget;}
void* xl6NetDatagramPacketTargetV1(const void* raw)
{
    const XL6_NetDatagramPacketV1* packet=raw;
    if(!packet||!packet->hasTarget){xrtSetErrorKind(XERR_STATE);failed();return NULL;}
    void* result=xl6NetAddressCopyV1(&packet->target);if(!result)failed();return result;
}
bool xl6NetDatagramPacketValidV1(const void* raw)
{return raw&&((const XL6_NetDatagramPacketV1*)raw)->info.valid;}
int64_t xl6NetDatagramPacketStatusV1(const void* raw)
{
    const XL6_NetDatagramPacketV1* packet=raw;
    if(!packet||!packet->hasResult){xrtSetErrorKind(XERR_STATE);failed();return XNET_RESULT_ERROR;}return packet->status;
}
void xl6NetDatagramPacketClearV1(void* raw)
{
    XL6_NetDatagramPacketV1* packet=raw;xbuffer* buffer=xl6NetDatagramPacketBufferV1(packet);
    if(!buffer){failed();return;}buffer->Size=0;packet->info=(XL6_NetDatagramInfoV1){0};packet->status=XNET_RESULT_ERROR;packet->hasResult=false;
}
static bool trace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    if(!visit||!xrtValueGetHandle(value,&raw,&ops,&user)||!raw||user||ops!=xl6NetDatagramPacketHandlePolicyV1Get()->Ops)return false;
    const XL6_NetDatagramPacketV1* packet=raw;
    void* buffer=NULL;return packet->buffer&&xrtValueGetHandle(packet->buffer,&buffer,&ops,&user)&&buffer&&!user&&
        ops==xl6BufferHandlePolicyV1Get()->Ops&&visit(xrtValueOwnership(packet->buffer),context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6NetDatagramPacketHandlePolicyV1Get,xl6NetDatagramPacketDestroyV1,trace)
