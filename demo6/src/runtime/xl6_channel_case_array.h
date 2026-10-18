#ifndef XL6_CHANNEL_CASE_ARRAY_H
#define XL6_CHANNEL_CASE_ARRAY_H
#include "xl6_channel_language_abi.h"
/* Only audited resident select providers pass this synchronous stack callback.
 * No input array slot, output variable, callback or descriptor is retained. */
typedef XL6_ChannelSelectionOwnerV1* (*XL6_ChannelCasesInvokeV1)(
    XL6_ChannelCaseOwnerV1* const*,size_t,XL6_ChannelWaitOptionsV2,xcancel*);
XL6_TEXT_API void* xl6ChannelCasesLanguageInvokeV1(XL6_ChannelCaseArrayContextV1,
    const xvalue*,XL6_ChannelWaitOptionsV2,XL6_ChannelCasesInvokeV1,xcancel*);
#endif
