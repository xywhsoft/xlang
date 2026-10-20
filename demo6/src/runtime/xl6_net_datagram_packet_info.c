#include "xl6_net_datagram_batch_internal.h"
#include "src/api/xl_foreign_error.h"
void* xl6NetDatagramPacketInfoV1(const void* raw)
{
    const XL6_NetDatagramPacketV1* packet=raw;XL6_NetDatagramInfoV1* result=NULL;
    if(!packet||!packet->info.valid)xrtSetErrorKind(XERR_STATE);
    else {result=xrtMalloc(sizeof(*result));if(result)*result=packet->info;}
    if(!result)xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
    return result;
}
