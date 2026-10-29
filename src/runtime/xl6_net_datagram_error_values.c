#include "xl6_net_datagram_error_internal.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
#include <string.h>
static void failed(xerrkind kind)
{xrtSetErrorKind(kind);xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});}
void* xl6NetDatagramErrorInfoCreateV1(void)
{
    void* result=xrtCalloc(1,sizeof(XL6_NetDatagramErrorInfoV1));
    if(!result)xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
    return result;
}
void xl6NetDatagramErrorInfoDestroyV1(void* raw){xrtFree(raw);}
void xl6NetDatagramErrorInfoClearV1(void* raw)
{if(raw)memset(raw,0,sizeof(XL6_NetDatagramErrorInfoV1));}
bool xl6NetDatagramErrorInfoValidV1(const void* raw)
{return raw&&((const XL6_NetDatagramErrorInfoV1*)raw)->valid;}
int64_t xl6NetDatagramErrorInfoFlagsV1(const void* raw)
{return xl6NetDatagramErrorInfoValidV1(raw)?((const XL6_NetDatagramErrorInfoV1*)raw)->error.Flags:0;}
static const xnetdgramerror* snapshot(const void* raw,uint32_t flag)
{
    const XL6_NetDatagramErrorInfoV1* value=raw;
    if(!value){failed(XERR_ARGUMENT);return NULL;}
    if(!value->valid||(flag&&!(value->error.Flags&flag))){failed(XERR_STATE);return NULL;}
    return &value->error;
}
#define FIELD(Name,Field) \
int64_t xl6NetDatagramErrorInfo##Name##V1(const void* raw) \
{const xnetdgramerror* value=snapshot(raw,0);return value?value->Field:0;}
FIELD(Origin,Origin)
FIELD(KindCode,Kind)
FIELD(SystemCode,SystemCode)
FIELD(Type,Type)
FIELD(Code,Code)
FIELD(Info,Info)
FIELD(Data,Data)
#undef FIELD
uint64_t xl6NetDatagramErrorInfoPathMtuV1(const void* raw)
{const xnetdgramerror* value=snapshot(raw,XNET_DGRAM_ERROR_PATH_MTU);return value?value->PathMtu:0;}
static void* address(const void* raw,bool offender)
{
    const xnetdgramerror* value=snapshot(raw,offender?XNET_DGRAM_ERROR_OFFENDER:XNET_DGRAM_ERROR_REMOTE);
    if(!value)return NULL;
    void* result=xl6NetAddressCopyV1(offender?&value->Offender:&value->Remote);
    if(!result)xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
    return result;
}
void* xl6NetDatagramErrorInfoRemoteV1(const void* raw){return address(raw,false);}
void* xl6NetDatagramErrorInfoOffenderV1(const void* raw){return address(raw,true);}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6NetDatagramErrorInfoHandlePolicyV1Get,xl6NetDatagramErrorInfoDestroyV1)
