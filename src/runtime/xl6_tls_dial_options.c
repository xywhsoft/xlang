#include "xl6_tls_dial_options_internal.h"
#include <string.h>
void xl6TlsDialDefaultsV1(xtlsdialconfig* value)
{
    *value=(xtlsdialconfig){0};xrtNetDialConfigInit(&value->Transport);
    xl6TlsStreamOptionsDefaultsV1(&value->Stream);value->ServerNameFromHost=true;
}
static void* copy(const xtlsdialconfig* native,xstrview server,xstrview verify)
{
    if((!server.Data&&server.Size)||(!verify.Data&&verify.Size)||
        server.Size>SIZE_MAX-sizeof(XL6_TlsDialOptionsV1)||verify.Size>SIZE_MAX-sizeof(XL6_TlsDialOptionsV1)-server.Size){
        xrtSetErrorKind(XERR_RANGE);xl6TcpOptionsFailedV1();return NULL;}
    XL6_TlsDialOptionsV1* result=xrtMalloc(sizeof(*result)+server.Size+verify.Size);
    if(!result){xl6TcpOptionsFailedV1();return NULL;}
    result->native=*native;result->serverName=(xstrview){result->names,server.Size};
    result->verifyName=(xstrview){result->names+server.Size,verify.Size};
    if(server.Size)memcpy((char*)result->serverName.Data,server.Data,server.Size);
    if(verify.Size)memcpy((char*)result->verifyName.Data,verify.Data,verify.Size);
    return result;
}
void* xl6TlsDialOptionsCreateV1(void)
{xtlsdialconfig native;xl6TlsDialDefaultsV1(&native);return copy(&native,(xstrview){0},(xstrview){0});}
void xl6TlsDialOptionsDestroyV1(void* raw){xrtFree(raw);}
void* xl6TlsDialOptionsTransportV1(const void* raw)
{return xl6TcpDialOptionsCopyV1(&((const XL6_TlsDialOptionsV1*)raw)->native.Transport);}
void* xl6TlsDialOptionsStreamV1(const void* raw)
{return xl6TlsStreamOptionsCopyV1(&((const XL6_TlsDialOptionsV1*)raw)->native.Stream);}
void* xl6TlsDialOptionsWithTransportV1(const void* raw,const void* transport)
{
    if(!transport||!xrtNetDialConfigValid(transport)){xl6TcpOptionsFailedV1();return NULL;}
    const XL6_TlsDialOptionsV1* self=raw;xtlsdialconfig value=self->native;value.Transport=*(const xnetdialconfig*)transport;
    return copy(&value,self->serverName,self->verifyName);
}
void* xl6TlsDialOptionsWithStreamV1(const void* raw,const void* stream)
{
    const xtlsstreamconfig* input=stream;
    if(!input||!input->AsyncBytesLimit||!input->AsyncCountLimit||!input->AsyncBatch){
        xrtSetErrorKind(XERR_RANGE);xl6TcpOptionsFailedV1();return NULL;}
    const XL6_TlsDialOptionsV1* self=raw;xtlsdialconfig value=self->native;value.Stream=*input;
    return copy(&value,self->serverName,self->verifyName);
}
int64_t xl6TlsDialOptionsTimeoutV1(const void* raw)
{uint64_t value=((const XL6_TlsDialOptionsV1*)raw)->native.Timeout;return value?xl6TcpOptionsMillisV1(value):-1;}
void* xl6TlsDialOptionsWithTimeoutV1(const void* raw,int64_t millis)
{
    const XL6_TlsDialOptionsV1* self=raw;xtlsdialconfig value=self->native;
    if(!xl6TcpOptionsTimeoutV1(millis,&value.Timeout))return NULL;
    return copy(&value,self->serverName,self->verifyName);
}
bool xl6TlsDialOptionsServerNameFromHostV1(const void* raw)
{return ((const XL6_TlsDialOptionsV1*)raw)->native.ServerNameFromHost;}
void* xl6TlsDialOptionsWithServerNameFromHostV1(const void* raw,bool enabled)
{
    const XL6_TlsDialOptionsV1* self=raw;xtlsdialconfig value=self->native;value.ServerNameFromHost=enabled;
    return copy(&value,self->serverName,self->verifyName);
}
static xstrview nameCopy(xstrview name)
{
    char* text=xrtStrDupView(name);if(!text){xl6TcpOptionsFailedV1();return (xstrview){0};}
    return (xstrview){text,name.Size};
}
xstrview xl6TlsDialOptionsServerNameV1(const void* raw)
{return nameCopy(((const XL6_TlsDialOptionsV1*)raw)->serverName);}
xstrview xl6TlsDialOptionsVerifyNameV1(const void* raw)
{return nameCopy(((const XL6_TlsDialOptionsV1*)raw)->verifyName);}
static bool selectName(xstrview* name,int64_t size)
{
    if(!xl6TextSelect(name,size))return false;
    if(name->Size&&memchr(name->Data,0,name->Size)){xrtSetErrorKind(XERR_VALUE);return false;}
    return true;
}
void* xl6TlsDialOptionsWithServerNameV1(const void* raw,xstrview name,int64_t size)
{
    if(!selectName(&name,size)){xl6TcpOptionsFailedV1();return NULL;}
    const XL6_TlsDialOptionsV1* self=raw;return copy(&self->native,name,self->verifyName);
}
void* xl6TlsDialOptionsWithVerifyNameV1(const void* raw,xstrview name,int64_t size)
{
    if(!selectName(&name,size)){xl6TcpOptionsFailedV1();return NULL;}
    const XL6_TlsDialOptionsV1* self=raw;return copy(&self->native,self->serverName,name);
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TlsDialOptionsHandlePolicyV1Get,xl6TlsDialOptionsDestroyV1)
