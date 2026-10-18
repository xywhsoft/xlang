#ifndef XL6_CHANNEL_CASE_ABI_H
#define XL6_CHANNEL_CASE_ABI_H
#include "xl6_channel_wait_abi.h"

/* Resident immutable objects. These descriptors own values, not addresses of
 * language variables. A case can be reused/concurrently selected; no receive
 * result is ever written into the case. Native callers protect actual owners
 * and keep the input pointer array stable for the synchronous call. */
typedef struct XL6_ChannelCaseOwnerV1 XL6_ChannelCaseOwnerV1;
typedef struct XL6_ChannelSelectionOwnerV1 XL6_ChannelSelectionOwnerV1;
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_CHANNEL_CASE_API __declspec(dllexport)
#else
#define XL6_CHANNEL_CASE_API
#endif
XL6_CHANNEL_CASE_API XL6_ChannelCaseOwnerV1* xl6ChannelSendCaseCreateV1(
    const xvalue*,uint64_t,uint64_t,const xrttype*,XL6_ChannelSlotV1);
XL6_CHANNEL_CASE_API XL6_ChannelCaseOwnerV1* xl6ChannelReceiveCaseCreateV1(
    const xvalue*,uint64_t,uint64_t,const xrttype*,size_t);
XL6_CHANNEL_CASE_API void xl6ChannelCaseDestroyV1(XL6_ChannelCaseOwnerV1*);
XL6_CHANNEL_CASE_API XL6_ChannelSelectionOwnerV1* xl6ChannelCasesSelectV1(
    XL6_ChannelCaseOwnerV1* const*,size_t,XL6_ChannelWaitOptionsV2);
XL6_CHANNEL_CASE_API XL6_ChannelSelectionOwnerV1* xl6ChannelCasesSelectCancelV1(
    XL6_ChannelCaseOwnerV1* const*,size_t,XL6_ChannelWaitOptionsV2,xcancel*);
XL6_CHANNEL_CASE_API bool xl6ChannelSelectionInfoV1(
    const XL6_ChannelSelectionOwnerV1*,XL6_ChannelSelectionV1*);
/* Exact selected RECEIVING case only, successful status required (including
 * successful nullable null). Copies into an empty private typed output slot;
 * false leaves it unchanged, independently of diagnostic allocation. This is
 * not an implicit conversion, a type-erased result, or a public byref API. */
XL6_CHANNEL_CASE_API bool xl6ChannelSelectionValueV1(const XL6_ChannelSelectionOwnerV1*,
    const XL6_ChannelCaseOwnerV1*,uint64_t,const xrttype*,XL6_ChannelSlotV1);
XL6_CHANNEL_CASE_API void xl6ChannelSelectionDestroyV1(XL6_ChannelSelectionOwnerV1*);
#undef XL6_CHANNEL_CASE_API
#endif
