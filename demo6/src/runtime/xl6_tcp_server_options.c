#include "xl6_tcp_server_options_internal.h"
#include <string.h>
static bool valid(const xnetserverconfig* c)
{
    if(!c||!c->AcceptQueueLimit||(c->Mode!=XNET_SERVER_SHARED&&c->Mode!=XNET_SERVER_REUSE_PORT)||
        (c->AdditionalCount&&!c->Additional)){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(!xrtNetListenConfigValid(&c->Listen))return false;
    uint16 port=c->SharedPort?c->Listen.Address.Port:0;
    for(size_t i=0;i<c->AdditionalCount;++i){
        if(!xrtNetListenConfigValid(&c->Additional[i]))return false;
        uint16 next=c->SharedPort?c->Additional[i].Address.Port:0;
        if(port&&next&&port!=next){xrtSetErrorKind(XERR_ARGUMENT);return false;}
        if(next)port=next;
    }
    return true;
}
static XL6_TcpServerOptionsV1* allocate(const xnetserverconfig* c,size_t additional)
{
    if(additional>(SIZE_MAX-sizeof(XL6_TcpServerOptionsV1))/sizeof(xnetlistenconfig)){
        xrtSetErrorKind(XERR_RANGE);return NULL;}
    XL6_TcpServerOptionsV1* r=xrtMalloc(sizeof(*r)+additional*sizeof(xnetlistenconfig));if(!r)return NULL;
    r->config=*c;r->config.AdditionalCount=additional;r->config.Additional=additional?r->additional:NULL;
    size_t copied=additional<c->AdditionalCount?additional:c->AdditionalCount;
    if(copied)memcpy(r->additional,c->Additional,copied*sizeof(*r->additional));
    return r;
}
static void* finish(XL6_TcpServerOptionsV1* r)
{
    if(r&&valid(&r->config))return r;
    xrtFree(r);xl6TcpOptionsFailedV1();return NULL;
}
static void* publish(const xnetserverconfig* c){return finish(allocate(c,c->AdditionalCount));}
void* xl6TcpServerOptionsCreateV1(void)
{xnetserverconfig c;xrtNetServerConfigInit(&c);return publish(&c);}
void* xl6TcpServerOptionsCreateListenV1(const void* listen)
{
    if(!listen){xrtSetErrorKind(XERR_ARGUMENT);xl6TcpOptionsFailedV1();return NULL;}
    xnetserverconfig c;xrtNetServerConfigInit(&c);c.Listen=*(const xnetlistenconfig*)listen;return publish(&c);
}
void xl6TcpServerOptionsDestroyV1(void* raw){xrtFree(raw);}
uint64_t xl6TcpServerOptionsEndpointCountV1(const void* raw)
{return ((const XL6_TcpServerOptionsV1*)raw)->config.AdditionalCount+UINT64_C(1);}
int64_t xl6TcpServerOptionsModeV1(const void* raw){return ((const XL6_TcpServerOptionsV1*)raw)->config.Mode;}
bool xl6TcpServerOptionsSharedPortV1(const void* raw){return ((const XL6_TcpServerOptionsV1*)raw)->config.SharedPort;}
int64_t xl6TcpServerOptionsAcceptQueueLimitV1(const void* raw){return ((const XL6_TcpServerOptionsV1*)raw)->config.AcceptQueueLimit;}
static bool validIndex(const xnetserverconfig* c,int64_t i)
{return xl6TcpOptionsRangeV1(i>=0&&(uint64_t)i<=c->AdditionalCount);}
void* xl6TcpServerOptionsEndpointV1(const void* raw,int64_t i)
{
    const xnetserverconfig* c=xl6TcpServerOptionsNativeV1(raw);if(!validIndex(c,i))return NULL;
    return xl6TcpListenOptionsCopyV1(i?&c->Additional[i-1]:&c->Listen);
}
void* xl6TcpServerOptionsWithEndpointV1(const void* raw,int64_t i,const void* listen)
{
    const xnetserverconfig* c=xl6TcpServerOptionsNativeV1(raw);if(!validIndex(c,i))return NULL;
    XL6_TcpServerOptionsV1* r=allocate(c,c->AdditionalCount);if(r){if(i)r->additional[i-1]=*(const xnetlistenconfig*)listen;else r->config.Listen=*(const xnetlistenconfig*)listen;}
    return finish(r);
}
void* xl6TcpServerOptionsWithAddedEndpointV1(const void* raw,const void* listen)
{
    const xnetserverconfig* c=xl6TcpServerOptionsNativeV1(raw);
    if(!xl6TcpOptionsRangeV1(c->AdditionalCount<SIZE_MAX))return NULL;
    XL6_TcpServerOptionsV1* r=allocate(c,c->AdditionalCount+1);if(r)r->additional[c->AdditionalCount]=*(const xnetlistenconfig*)listen;
    return finish(r);
}
void* xl6TcpServerOptionsWithoutEndpointV1(const void* raw,int64_t i)
{
    const xnetserverconfig* c=xl6TcpServerOptionsNativeV1(raw);if(!validIndex(c,i)||!xl6TcpOptionsRangeV1(c->AdditionalCount>0))return NULL;
    XL6_TcpServerOptionsV1* r=allocate(c,c->AdditionalCount-1);
    if(r){
        if(!i)r->config.Listen=c->Additional[0];
        for(size_t j=0;j<r->config.AdditionalCount;++j){size_t old=j+1;if(old>=(uint64_t)i)++old;r->additional[j]=c->Additional[old-1];}
    }
    return finish(r);
}
void* xl6TcpServerOptionsWithModeV1(const void* raw,int64_t mode)
{
    if(!xl6TcpOptionsRangeV1(mode==XNET_SERVER_SHARED||mode==XNET_SERVER_REUSE_PORT))return NULL;
    xnetserverconfig c=*xl6TcpServerOptionsNativeV1(raw);c.Mode=(xnetservermode)mode;return publish(&c);
}
void* xl6TcpServerOptionsWithSharedPortV1(const void* raw,bool shared)
{xnetserverconfig c=*xl6TcpServerOptionsNativeV1(raw);c.SharedPort=shared;return publish(&c);}
void* xl6TcpServerOptionsWithAcceptQueueLimitV1(const void* raw,int64_t count)
{
    if(!xl6TcpOptionsRangeV1(count>0&&(uint64_t)count<=UINT32_MAX))return NULL;
    xnetserverconfig c=*xl6TcpServerOptionsNativeV1(raw);c.AcceptQueueLimit=(uint32)count;return publish(&c);
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TcpServerOptionsHandlePolicyV1Get,xl6TcpServerOptionsDestroyV1)
