/* Exact closed native generic class admission, shared by semantic analysis
 * and XLIR. Roles fix logical/physical signatures; names grant no arbitrary
 * generic C ABI. One owner type parameter is sealed after specialization. */
XL6_NATIVE_GENERIC_BRIDGE("xl6PromiseCreateV1", "xl6PromiseDestroyV1", CREATE)
XL6_NATIVE_GENERIC_BRIDGE("xl6PromiseCreateParentV1", "xl6PromiseDestroyV1", CREATE_PARENT)
XL6_NATIVE_GENERIC_BRIDGE("xl6PromiseDestroyV1", "xl6PromiseDestroyV1", DESTROY)
XL6_NATIVE_GENERIC_BRIDGE("xl6PromiseFutureV1", "xl6PromiseDestroyV1", FUTURE)
XL6_NATIVE_GENERIC_BRIDGE("xl6PromiseResolveV1", "xl6PromiseDestroyV1", RESOLVE)
XL6_NATIVE_GENERIC_BRIDGE("xl6PromiseRejectV1", "xl6PromiseDestroyV1", REJECT)
XL6_NATIVE_GENERIC_BRIDGE("xl6PromiseCancelV1", "xl6PromiseDestroyV1", CANCEL)
XL6_NATIVE_GENERIC_BRIDGE("xl6PromiseCloseV1", "xl6PromiseDestroyV1", CLOSE)
XL6_NATIVE_GENERIC_BRIDGE("xl6PromiseDoneV1", "xl6PromiseDestroyV1", DONE)
XL6_NATIVE_GENERIC_BRIDGE("xl6PromiseRequestedV1", "xl6PromiseDestroyV1", REQUESTED)
XL6_NATIVE_GENERIC_BRIDGE("xl6ChannelReceiveCaseLanguageV1", "xl6ChannelCaseDestroyV1", CHANNEL_CASE_RECEIVE)
XL6_NATIVE_GENERIC_BRIDGE("xl6ChannelSendCaseLanguageV1", "xl6ChannelCaseDestroyV1", CHANNEL_CASE_SEND)
XL6_NATIVE_GENERIC_BRIDGE("xl6ChannelSelectionValueLanguageV1", "xl6ChannelSelectionDestroyV1", CHANNEL_SELECTION_VALUE)
