#include "src/runtime/xl6_channel_wait_abi.h"
#include "src/runtime/xl6_array_admission.h"
#include <assert.h>
#include <stdint.h>
#define ID UINT64_C(4001)
#define ELEMENT UINT64_C(4002)
int main(void)
{
    xvalue* seed=xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,123,ELEMENT,1,sizeof(int64_t),
        xrtTypeInt64(),XL6_VALUE_ADMIT_I64,false,ELEMENT);
    xvalue* channel=xl6ChannelCreateTakeV1(ID,seed,1); assert(channel);
    int64_t output=91; XL6_ChannelSlotV1 slot={&output,sizeof(output)};
    XL6_ChannelWaitOptionsV2 options={sizeof(options),XL6_CHANNEL_TRY_V1,0,0};
#if TEST_KIND==1 || TEST_KIND==2
    XL6_ChannelStatusV1 status=XL6_CHANNEL_OK_V1;
#if TEST_KIND==1
    assert(xl6ChannelReceiveWaitV2(channel,ID,ELEMENT,xrtTypeInt64(),slot,options,&status));
#else
    xcancel* token=xrtCancelCreate(); assert(token);
    assert(xl6ChannelReceiveCancelV2(channel,ID,ELEMENT,xrtTypeInt64(),slot,options,token,&status));
    xrtCancelDestroy(token);
#endif
    assert(status==XL6_CHANNEL_EMPTY_V1 && output==91);
#else
    XL6_ChannelCaseV1 item={channel,ID,ELEMENT,xrtTypeInt64(),slot,XL6_CHANNEL_CASE_RECEIVE_V1};
    XL6_ChannelSelectionV1 result;
#if TEST_KIND==3
    assert(xl6ChannelSelectV1(&item,1,options,&result));
#else
    xcancel* token=xrtCancelCreate(); assert(token);
    assert(xl6ChannelSelectCancelV1(&item,1,options,token,&result)); xrtCancelDestroy(token);
#endif
    assert(result.index==SIZE_MAX && result.status==XL6_CHANNEL_TIMEOUT_V1 && output==91);
#endif
    xrtValueRelease(channel); assert(!xrtGetError()); return 0;
}
