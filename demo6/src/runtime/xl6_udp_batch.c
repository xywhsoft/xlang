#include "xl6_udp_internal.h"
void xl6UdpBatchDestroyV1(void* raw){xrtNetUdpBatchDestroy(raw);}
uint64_t xl6UdpBatchCountV1(const void* raw){return xrtNetUdpBatchCount(raw);}
void* xl6UdpBatchTakeV1(void* raw,int64_t index)
{
    if(index<0||(uint64_t)index>=xrtNetUdpBatchCount(raw)){xrtSetErrorKind(XERR_RANGE);xl6UdpFailedV1();return NULL;}
    /* Native atomic Take transfers the actual slot once. NULL means that
     * this valid slot has already been taken; no unsafe borrowed Get/Ref. */
    return xrtNetUdpBatchTake(raw,(size_t)index);
}
static bool trace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    return visit&&xrtValueGetHandle(value,&raw,&ops,&user)&&raw&&!user&&ops==xl6UdpBatchHandlePolicyV1Get()->Ops&&
        visit(xrtNetUdpBatchOwnership(raw),context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6UdpBatchHandlePolicyV1Get,xl6UdpBatchDestroyV1,trace)
