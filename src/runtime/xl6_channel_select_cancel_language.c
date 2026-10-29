#include "xl6_text_value.h"
#include "xl6_channel_case_array.h"
#include "src/api/xl_foreign_error.h"
static XL6_ChannelSelectionOwnerV1* invoke(XL6_ChannelCaseOwnerV1* const* cases,
    size_t count,XL6_ChannelWaitOptionsV2 options,xcancel* token)
{ return xl6ChannelCasesSelectCancelV1(cases,count,options,token); }
static void* selectCases(XL6_NativeClassArrayContextV1 context,const xvalue* cases,
    XL6_ChannelWaitOptionsV2 options,xcancel* token)
{
    if(!token){xrtSetErrorKind(XERR_ARGUMENT);xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return NULL;}
    return xl6ChannelCasesLanguageInvokeV1(context,cases,options,invoke,token);
}
void* xl6ChannelCasesWaitCancelLanguageV1(XL6_NativeClassArrayContextV1 context,const xvalue* cases,xcancel* token)
{ return selectCases(context,cases,(XL6_ChannelWaitOptionsV2){sizeof(XL6_ChannelWaitOptionsV2),XL6_CHANNEL_WAIT_V1,0,0},token); }
void* xl6ChannelCasesForCancelLanguageV1(XL6_NativeClassArrayContextV1 context,const xvalue* cases,int64_t milliseconds,xcancel* token)
{ return selectCases(context,cases,(XL6_ChannelWaitOptionsV2){sizeof(XL6_ChannelWaitOptionsV2),XL6_CHANNEL_FOR_V1,milliseconds,0},token); }
void* xl6ChannelCasesUntilCancelLanguageV1(XL6_NativeClassArrayContextV1 context,const xvalue* cases,uint64_t deadline,xcancel* token)
{ return selectCases(context,cases,(XL6_ChannelWaitOptionsV2){sizeof(XL6_ChannelWaitOptionsV2),XL6_CHANNEL_UNTIL_V2,0,deadline},token); }
