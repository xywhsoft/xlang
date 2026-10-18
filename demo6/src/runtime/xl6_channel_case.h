#ifndef XL6_CHANNEL_CASE_H
#define XL6_CHANNEL_CASE_H
#include "xl6_channel.h"
#include "xl6_channel_case_abi.h"
#include "xl6_native_handle.h"
XL6_TEXT_API xrtownershipref xl6ChannelCaseOwnershipV1(const XL6_ChannelCaseOwnerV1*);
XL6_TEXT_API xrtownershipref xl6ChannelSelectionOwnershipV1(const XL6_ChannelSelectionOwnerV1*);
XL6_TEXT_API const xrtownershipadapterv1* xl6ChannelCaseOwnershipAdapterV1(xrtownershipref);
XL6_TEXT_API const xrtownershipquiescencev1* xl6ChannelCaseQuiescenceV1Get(void);
XL6_TEXT_API const xrtownershipquiescencev1* xl6ChannelSelectionQuiescenceV1Get(void);
XL6_TEXT_API const XL6_NativeHandlePolicyV1* xl6ChannelCaseHandlePolicyV1Get(void);
XL6_TEXT_API const XL6_NativeHandlePolicyV1* xl6ChannelSelectionHandlePolicyV1Get(void);
#endif
