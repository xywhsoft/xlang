#include "xl6_udp_message_internal.h"
#include "xl6_net_bytes_internal.h"
#include <string.h>
typedef XL6_UdpMessageV1 Message;
void xl6UdpMessageDestroyV1(void* raw)
{
    Message* m=raw;if(!m)return;
    if(m->native.Kind==XNET_UDP_OWNED_BYTES)xrtNetBytesDestroy(m->native.Payload.Bytes.Bytes);
    else if(m->native.Kind==XNET_UDP_OWNED_PACKET)xrtNetUdpPacketDestroy(m->native.Payload.Packet.Packet);
    else if(m->native.Kind==XNET_UDP_OWNED_VECTOR)
        for(size_t i=0;i<m->native.Payload.Vector.Count;++i)xrtNetBytesDestroy(m->native.Payload.Vector.Items[i].Bytes);
    xrtFree(m);
}
Message* xl6UdpMessageDiscardInternalV1(Message* m)
{
    xl6_error_scope cleanup={0};if(!xl6ErrorScopeBeginCleanup(&cleanup,true))abort();
    xl6UdpMessageDestroyV1(m);xl6_error_result result=xl6ErrorScopeEnd(&cleanup);
    xrtSetErrorTake(result.error);return NULL;
}
Message* xl6UdpMessageAllocateInternalV1(size_t count)
{
    if(count>XNET_UDP_VECTOR_MAX||count>(SIZE_MAX-sizeof(Message))/sizeof(xnetbytesref)){xrtSetErrorKind(XERR_RANGE);return NULL;}
    return xrtCalloc(1,sizeof(Message)+count*sizeof(xnetbytesref));
}
bool xl6UdpMessageSizeValidInternalV1(size_t size)
{if(size<=XNET_UDP_PAYLOAD_MAX)return true;xrtSetErrorKind(XERR_RANGE);return false;}
Message* xl6UdpMessageCopyInternalV1(xbytesview view,int64_t size)
{
    if(!xl6BufferSelect(&view,size)||!xl6UdpMessageSizeValidInternalV1(view.Size))return NULL;
    Message* m=xl6UdpMessageAllocateInternalV1(0);if(!m)return NULL;m->native.Kind=XNET_UDP_OWNED_BYTES;
    m->native.Payload.Bytes=(xnetbytesref){xrtNetBytesCopy(view.Data,view.Size),0,view.Size};m->length=view.Size;
    return m->native.Payload.Bytes.Bytes?m:xl6UdpMessageDiscardInternalV1(m);
}
void* xl6UdpMessageTextV1(xstrview input,int64_t size)
{Message* m=xl6UdpMessageCopyInternalV1((xbytesview){(const uint8_t*)input.Data,input.Size},size);if(!m)xl6UdpFailedV1();return m;}
void* xl6UdpMessageBytesV1(const xvalue* input,int64_t size)
{xbytesview view={0};Message* m=xl6BufferBytesInput(input,size,&view)?xl6UdpMessageCopyInternalV1(view,-1):NULL;if(!m)xl6UdpFailedV1();return m;}
static void* fromRef(void* input,int64_t size,int64_t offset,bool packet)
{
    Message* m=NULL;xbytesview view={0};
    if(!input){xrtSetErrorKind(XERR_ARGUMENT);goto done;}
    view=packet?(xbytesview){xrtNetUdpPacketData(input),xrtNetUdpPacketSize(input)}:xrtNetBytesView(input);
    if(!xl6UdpSliceV1(&view,size,offset)||!xl6UdpMessageSizeValidInternalV1(view.Size))goto done;
    m=xl6UdpMessageAllocateInternalV1(0);if(!m)goto done;m->length=view.Size;
    if(packet){m->native.Kind=XNET_UDP_OWNED_PACKET;m->native.Payload.Packet=(xnetudppacketref){xrtNetUdpPacketRef(input),(size_t)offset,view.Size};
        if(!m->native.Payload.Packet.Packet)m=xl6UdpMessageDiscardInternalV1(m);}
    else{m->native.Kind=XNET_UDP_OWNED_BYTES;m->native.Payload.Bytes=(xnetbytesref){xrtNetBytesRef(input),(size_t)offset,view.Size};
        if(!m->native.Payload.Bytes.Bytes)m=xl6UdpMessageDiscardInternalV1(m);}
done:if(!m)xl6UdpFailedV1();return m;
}
void* xl6UdpMessageRefBytesV1(void* p,int64_t s,int64_t o){return fromRef(p,s,o,false);}
void* xl6UdpMessageRefPacketV1(void* p,int64_t s,int64_t o){return fromRef(p,s,o,true);}
static Message* clone(const Message* source)
{
    if(!source){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    size_t count=source->native.Kind==XNET_UDP_OWNED_VECTOR?source->native.Payload.Vector.Count:0;
    Message* r=xl6UdpMessageAllocateInternalV1(count);if(!r)return NULL;*r=*source;
    r->native.Remote=source->native.Remote?&r->remote:NULL;r->native.Control=source->native.Control?&r->control:NULL;
    if(r->native.Kind==XNET_UDP_OWNED_BYTES){r->native.Payload.Bytes.Bytes=xrtNetBytesRef(source->native.Payload.Bytes.Bytes);
        if(!r->native.Payload.Bytes.Bytes)return xl6UdpMessageDiscardInternalV1(r);}
    else if(r->native.Kind==XNET_UDP_OWNED_PACKET){r->native.Payload.Packet.Packet=xrtNetUdpPacketRef(source->native.Payload.Packet.Packet);
        if(!r->native.Payload.Packet.Packet)return xl6UdpMessageDiscardInternalV1(r);}
    else if(r->native.Kind==XNET_UDP_OWNED_VECTOR){r->native.Payload.Vector.Items=(xnetbytesref*)(r+1);r->native.Payload.Vector.Count=0;
        for(size_t i=0;i<count;++i){xnetbytesref ref=source->native.Payload.Vector.Items[i];ref.Bytes=xrtNetBytesRef(ref.Bytes);
            if(!ref.Bytes)return xl6UdpMessageDiscardInternalV1(r);
            ((xnetbytesref*)(r+1))[i]=ref;r->native.Payload.Vector.Count=i+1;}}
    else{xrtSetErrorKind(XERR_TYPE);return xl6UdpMessageDiscardInternalV1(r);}return r;
}
static void* configure(const void* source,const void* value,bool control,bool clear)
{
    Message* r=NULL;if(!value&&!clear){xrtSetErrorKind(XERR_ARGUMENT);goto done;}r=clone(source);if(!r)goto done;
    if(control){if(value)r->control=*(const xnetdgramcontrol*)value;r->native.Control=clear?NULL:&r->control;}
    else{if(value)r->remote=*(const xnetaddr*)value;r->native.Remote=clear?NULL:&r->remote;}
done:if(!r)xl6UdpFailedV1();return r;
}
void* xl6UdpMessageTargetV1(const void* m,const void* a){return configure(m,a,false,false);}
void* xl6UdpMessageNoTargetV1(const void* m){return configure(m,NULL,false,true);}
void* xl6UdpMessageControlV1(const void* m,const void* c){return configure(m,c,true,false);}
void* xl6UdpMessageNoControlV1(const void* m){return configure(m,NULL,true,true);}
uint64_t xl6UdpMessageLengthV1(const void* m){return m?((const Message*)m)->length:0;}
bool xl6UdpMessageHasTargetV1(const void* m){return m&&((const Message*)m)->native.Remote;}
int64_t xl6UdpMessageControlFlagsV1(const void* m){return m&&((const Message*)m)->native.Control?((const Message*)m)->control.Flags:0;}
static bool trace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;const xvaluehandleops* ops=NULL;void* user=NULL;
    if(!visit||!xrtValueGetHandle(value,&raw,&ops,&user)||!raw||user||ops!=xl6UdpMessageHandlePolicyV1Get()->Ops)return false;
    const Message* m=raw;
    if(m->native.Kind==XNET_UDP_OWNED_BYTES)return visit(xrtNetBytesOwnership(m->native.Payload.Bytes.Bytes),context);
    if(m->native.Kind==XNET_UDP_OWNED_PACKET)return visit(xrtNetUdpPacketOwnership(m->native.Payload.Packet.Packet),context);
    if(m->native.Kind!=XNET_UDP_OWNED_VECTOR||m->native.Payload.Vector.Count>XNET_UDP_VECTOR_MAX||m->native.Payload.Vector.Items!=(const xnetbytesref*)(m+1))return false;
    for(size_t i=0;i<m->native.Payload.Vector.Count;++i)if(!visit(xrtNetBytesOwnership(m->native.Payload.Vector.Items[i].Bytes),context))return false;
    return true;
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6UdpMessageHandlePolicyV1Get,xl6UdpMessageDestroyV1,trace)
