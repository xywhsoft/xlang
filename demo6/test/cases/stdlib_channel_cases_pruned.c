#include "src/runtime/xl6_channel_case_abi.h"
#include "src/runtime/xl6_array_admission.h"
#include <assert.h>
#define ID UINT64_C(4001)
#define ELEMENT UINT64_C(4002)
int main(void)
{
    xvalue* schema=xl6SequencePrimitiveCreateV1(1,123,ELEMENT,1,sizeof(int64_t),xrtTypeInt64(),XL6_VALUE_ADMIT_I64,false,ELEMENT);
    xvalue* q=xl6ChannelCreateTakeV1(ID,schema,1); assert(q);
    XL6_ChannelCaseOwnerV1* item=xl6ChannelReceiveCaseCreateV1(q,ID,ELEMENT,xrtTypeInt64(),sizeof(int64_t)); assert(item);
#if TEST_KIND!=1
    XL6_ChannelWaitOptionsV2 options={sizeof(options),XL6_CHANNEL_TRY_V1,0,0};
    XL6_ChannelSelectionOwnerV1* result;
#if TEST_KIND==3
    xcancel* token=xrtCancelCreate(); assert(token && xrtCancelRequest(token));
    result=xl6ChannelCasesSelectCancelV1(&item,1,options,token); xrtCancelDestroy(token);
#else
    result=xl6ChannelCasesSelectV1(&item,1,options);
#endif
    XL6_ChannelSelectionV1 info={0}; assert(result && xl6ChannelSelectionInfoV1(result,&info));
    assert(info.index==SIZE_MAX && info.status==(TEST_KIND==3 ? XL6_CHANNEL_CANCELLED_V1 : XL6_CHANNEL_TIMEOUT_V1));
    xl6ChannelSelectionDestroyV1(result);
#endif
    xl6ChannelCaseDestroyV1(item); xrtValueRelease(q); assert(!xrtGetError()); return 0;
}
