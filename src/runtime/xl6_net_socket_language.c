#include "xl6_text_value.h"
#include "xl6_net_socket_owner.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
#include <limits.h>
static void failed(void){xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});}
static bool enter(void* raw,bool control,XL6_NetSocketScopeV1* scope)
{
    bool ok=control?xl6NetSocketOwnerControlV1(raw,scope):xl6NetSocketOwnerEnterV1(raw,scope);
    if(!ok)failed();
    return ok;
}
void* xl6NetSocketEmptyV1(void)
{void* result=xl6NetSocketOwnerEmptyV1();if(!result)failed();return result;}
void* xl6NetSocketCreateV1(int64_t kind,int64_t family,bool nonblocking)
{
    if((kind!=XNET_SOCKET_STREAM&&kind!=XNET_SOCKET_DGRAM)||(family!=XNET_FAMILY_IPV4&&family!=XNET_FAMILY_IPV6)){
        xrtSetErrorKind(XERR_VALUE);failed();return NULL;}
    void* result=xl6NetSocketOwnerOpenV1((int)family,(int)kind,nonblocking);if(!result)failed();return result;
}
void xl6NetSocketDestroyV1(void* raw){xl6NetSocketOwnerReleaseV1(raw);}
void xl6NetSocketCloseV1(void* raw){if(!xl6NetSocketOwnerCloseV1(raw))failed();}
bool xl6NetSocketClosedV1(const void* raw){return xl6NetSocketOwnerClosedV1(raw);}
int64_t xl6NetSocketFamilyV1(void* raw)
{XL6_NetSocketScopeV1 scope={0};if(!enter(raw,false,&scope))return 0;
 int64_t result=xrtNetSocketFamily(scope.native);xl6NetSocketOwnerLeaveV1(&scope);return result;}
int64_t xl6NetSocketKindV1(void* raw)
{XL6_NetSocketScopeV1 scope={0};if(!enter(raw,false,&scope))return 0;
 int64_t result=xrtNetSocketType(scope.native);xl6NetSocketOwnerLeaveV1(&scope);return result;}
uint64_t xl6NetSocketAvailableV1(void* raw)
{XL6_NetSocketScopeV1 scope={0};if(!enter(raw,false,&scope))return 0;
 size_t count=0;if(!xrtNetSocketAvailable(scope.native,&count))failed();xl6NetSocketOwnerLeaveV1(&scope);return count;}
static bool option(int64_t value)
{if(value>=XNET_OPTION_NONBLOCK&&value<=XNET_OPTION_ERROR)return true;xrtSetErrorKind(XERR_VALUE);failed();return false;}
void xl6NetSocketSetV1(void* raw,int64_t key,int64_t value)
{XL6_NetSocketScopeV1 scope={0};if(!option(key)||!enter(raw,true,&scope))return;
 if(!xrtNetSocketSet(scope.native,(xnetoption)key,value))failed();
 xl6NetSocketOwnerLeaveV1(&scope);}
int64_t xl6NetSocketGetV1(void* raw,int64_t key)
{XL6_NetSocketScopeV1 scope={0};if(!option(key)||!enter(raw,false,&scope))return 0;
 int64 result=0;if(!xrtNetSocketGet(scope.native,(xnetoption)key,&result))failed();xl6NetSocketOwnerLeaveV1(&scope);return result;}
void xl6NetSocketBindV1(void* raw,const void* address)
{XL6_NetSocketScopeV1 scope={0};if(!enter(raw,true,&scope))return;
 if(!xrtNetSocketBind(scope.native,address))failed();
 xl6NetSocketOwnerLeaveV1(&scope);}
void xl6NetSocketListenV1(void* raw,int64_t backlog)
{
    if(backlog<=0||backlog>INT_MAX){xrtSetErrorKind(XERR_RANGE);failed();return;}
    XL6_NetSocketScopeV1 scope={0};if(!enter(raw,true,&scope))return;
    if(!xrtNetSocketListen(scope.native,(int)backlog))failed();
    xl6NetSocketOwnerLeaveV1(&scope);
}
int64_t xl6NetSocketConnectV1(void* raw,const void* address)
{XL6_NetSocketScopeV1 scope={0};if(!enter(raw,true,&scope))return XNET_RESULT_ERROR;
 xnetresult result=xrtNetSocketConnect(scope.native,address);if(result==XNET_RESULT_ERROR)failed();
 xl6NetSocketOwnerLeaveV1(&scope);return result;}
int64_t xl6NetSocketFinishConnectV1(void* raw)
{XL6_NetSocketScopeV1 scope={0};if(!enter(raw,true,&scope))return XNET_RESULT_ERROR;
 xnetresult result=xrtNetSocketFinishConnect(scope.native);if(result==XNET_RESULT_ERROR)failed();
 xl6NetSocketOwnerLeaveV1(&scope);return result;}
int64_t xl6NetSocketAcceptIntoV1(void* raw,void* destination)
{int result=xl6NetSocketOwnerAcceptV1(raw,destination);if(result==XNET_RESULT_ERROR)failed();return result;}
void xl6NetSocketShutdownV1(void* raw,int64_t direction)
{
    if(direction<XNET_SHUTDOWN_READ||direction>XNET_SHUTDOWN_BOTH){xrtSetErrorKind(XERR_VALUE);failed();return;}
    XL6_NetSocketScopeV1 scope={0};if(!enter(raw,false,&scope))return;
    if(!xrtNetSocketShutdown(scope.native,(xnetshutdown)direction))failed();
    xl6NetSocketOwnerLeaveV1(&scope);
}
static void* address(void* raw,bool remote)
{
    XL6_NetSocketScopeV1 scope={0};if(!enter(raw,false,&scope))return NULL;xnetaddr value;
    bool ok=remote?xrtNetSocketRemote(scope.native,&value):xrtNetSocketLocal(scope.native,&value);
    if(!ok)failed();
    xl6NetSocketOwnerLeaveV1(&scope);
    void* result=ok?xl6NetAddressCopyV1(&value):NULL;if(ok&&!result)failed();return result;
}
void* xl6NetSocketLocalV1(void* raw){return address(raw,false);}
void* xl6NetSocketRemoteV1(void* raw){return address(raw,true);}
static bool handleTrace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* owner=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    return visit&&xrtValueGetHandle(value,&owner,&ops,&user)&&owner&&!user&&
        ops==xl6NetSocketHandlePolicyV1Get()->Ops&&visit(xl6NetSocketOwnershipV1(owner),context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6NetSocketHandlePolicyV1Get,xl6NetSocketDestroyV1,handleTrace)
