#include "xl6_text_value.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
void xl6NetAddressListDestroyV1(void* raw){xrtNetAddrListDestroy(raw);}
int64_t xl6NetAddressListCountV1(const void* raw){return (int64_t)xrtNetAddrListCount(raw);}
static const xnetaddr* at(const void* raw,int64_t index)
{
    const xnetaddr* result=NULL;
    if(index<0)xrtSetErrorKind(XERR_RANGE);
    else result=xrtNetAddrListGet(raw,(size_t)index);
    if(!result)xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
    return result;
}
void* xl6NetAddressListAtV1(const void* raw,int64_t index)
{const xnetaddr* value=at(raw,index);return value?xl6NetAddressCopyV1(value):NULL;}
int64_t xl6NetAddressListFamilyV1(const void* raw,int64_t index)
{const xnetaddr* value=at(raw,index);return value?value->Family:0;}
int64_t xl6NetAddressListPortV1(const void* raw,int64_t index)
{const xnetaddr* value=at(raw,index);return value?value->Port:0;}
xstrview xl6NetAddressListTextV1(const void* raw,int64_t index)
{const xnetaddr* value=at(raw,index);return value?xl6NetAddressTextV1((void*)value):(xstrview){0};}
void* xl6NetAddressListCloneV1(void* raw){return xrtNetAddrListRef(raw);}
void* xl6NetAddressListWithPortV1(const void* raw,int64_t port)
{
    if(port<0||port>UINT16_MAX){xrtSetErrorKind(XERR_RANGE);return NULL;}
    return xrtNetAddrListWithPort((xnetaddrlist*)raw,(uint16)port);
}
static bool trace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    return visit&&xrtValueGetHandle(value,&raw,&ops,&user)&&raw&&!user&&
        ops==xl6NetAddressListHandlePolicyV1Get()->Ops&&visit(xrtNetAddrListOwnership(raw),context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6NetAddressListHandlePolicyV1Get,xl6NetAddressListDestroyV1,trace)
