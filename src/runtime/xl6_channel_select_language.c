#include "xl6_text_value.h"
#include "xl6_channel_case_array.h"
static XL6_ChannelSelectionOwnerV1* invoke(XL6_ChannelCaseOwnerV1* const* cases,
    size_t count,XL6_ChannelWaitOptionsV2 options,xcancel* unused)
{ (void)unused;return xl6ChannelCasesSelectV1(cases,count,options); }
void* xl6ChannelCasesWaitLanguageV1(XL6_NativeClassArrayContextV1 context,const xvalue* cases)
{ return xl6ChannelCasesLanguageInvokeV1(context,cases,(XL6_ChannelWaitOptionsV2){sizeof(XL6_ChannelWaitOptionsV2),XL6_CHANNEL_WAIT_V1,0,0},invoke,NULL); }
void* xl6ChannelCasesTryLanguageV1(XL6_NativeClassArrayContextV1 context,const xvalue* cases)
{ return xl6ChannelCasesLanguageInvokeV1(context,cases,(XL6_ChannelWaitOptionsV2){sizeof(XL6_ChannelWaitOptionsV2),XL6_CHANNEL_TRY_V1,0,0},invoke,NULL); }
void* xl6ChannelCasesForLanguageV1(XL6_NativeClassArrayContextV1 context,const xvalue* cases,int64_t milliseconds)
{ return xl6ChannelCasesLanguageInvokeV1(context,cases,(XL6_ChannelWaitOptionsV2){sizeof(XL6_ChannelWaitOptionsV2),XL6_CHANNEL_FOR_V1,milliseconds,0},invoke,NULL); }
void* xl6ChannelCasesUntilLanguageV1(XL6_NativeClassArrayContextV1 context,const xvalue* cases,uint64_t deadline)
{ return xl6ChannelCasesLanguageInvokeV1(context,cases,(XL6_ChannelWaitOptionsV2){sizeof(XL6_ChannelWaitOptionsV2),XL6_CHANNEL_UNTIL_V2,0,deadline},invoke,NULL); }
