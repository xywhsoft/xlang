#ifndef XL6_CHANNEL_LANGUAGE_ABI_H
#define XL6_CHANNEL_LANGUAGE_ABI_H
#include "xl6_sequence_abi.h"
#include "xl6_native_class_abi.h"
#include "xl6_channel_case_abi.h"
#include "xl6_runtime_services.h"
/* Compiler-sealed physical schema. No module-local copy/drop callback, source
 * variable address, or descriptor storage is retained by these entries. */
XL6_TEXT_API void* xl6ChannelReceiveCaseLanguageV1(XL6_SequenceLayoutV1,const xvalue*);
XL6_TEXT_API void* xl6ChannelSendCaseLanguageV1(XL6_SequenceLayoutV1,const xvalue*,const void*);
/* Checked void + private, initially empty, exact typed output. On ANY failure
 * the output remains empty, including cleanup/diagnostic allocation failure. */
XL6_TEXT_API void xl6ChannelSelectionValueLanguageV1(XL6_SequenceLayoutV1,
    const void*,const void*,const xvalue*,void*);
XL6_TEXT_API int64_t xl6ChannelSelectionIndexLanguageV1(const void*);
XL6_TEXT_API int64_t xl6ChannelSelectionStatusLanguageV1(const void*);
struct XL6_NativeHandlePolicyV1;
XL6_TEXT_API const struct XL6_NativeHandlePolicyV1* xl6ChannelCaseHandlePolicyV1Get(void);
XL6_TEXT_API const struct XL6_NativeHandlePolicyV1* xl6ChannelSelectionHandlePolicyV1Get(void);
XL6_TEXT_API void* xl6ChannelCasesWaitLanguageV1(XL6_NativeClassArrayContextV1,const xvalue*);
XL6_TEXT_API void* xl6ChannelCasesTryLanguageV1(XL6_NativeClassArrayContextV1,const xvalue*);
XL6_TEXT_API void* xl6ChannelCasesForLanguageV1(XL6_NativeClassArrayContextV1,const xvalue*,int64_t);
XL6_TEXT_API void* xl6ChannelCasesUntilLanguageV1(XL6_NativeClassArrayContextV1,const xvalue*,uint64_t);
XL6_TEXT_API void* xl6ChannelCasesWaitCancelLanguageV1(XL6_NativeClassArrayContextV1,const xvalue*,xcancel*);
XL6_TEXT_API void* xl6ChannelCasesForCancelLanguageV1(XL6_NativeClassArrayContextV1,const xvalue*,int64_t,xcancel*);
XL6_TEXT_API void* xl6ChannelCasesUntilCancelLanguageV1(XL6_NativeClassArrayContextV1,const xvalue*,uint64_t,xcancel*);
struct XL6_ContinuationExecutor;
XL6_TEXT_API void* xl6ChannelFutureBuilderCreateLanguageV1(XL6_NativeClassArrayContextV1,const xvalue*);
XL6_TEXT_API void xl6ChannelFutureBuilderDestroyV1(void*);
XL6_TEXT_API xvalue* xl6ChannelFutureBuilderResultV1(void*);
XL6_TEXT_API const struct XL6_NativeHandlePolicyV1* xl6ChannelFutureBuilderHandlePolicyV1Get(void);
XL6_TEXT_API xfuture* xl6ChannelFutureBuilderStartLanguageV1(XL6_RuntimeServicesV1,XL6_NativeClassFieldContextV1,void*,const xvalue*);
XL6_TEXT_API xfuture* xl6ChannelFutureBuilderStartCancelLanguageV1(XL6_RuntimeServicesV1,XL6_NativeClassFieldContextV1,void*,const xvalue*,xcancel*);
struct XL6_ChannelReceiveResultV1;
struct XL6_FutureValueProfileV1;
XL6_TEXT_API void* xl6ChannelDirectSendCreateLanguageV1(XL6_SequenceLayoutV1,const xvalue*,const void*);
XL6_TEXT_API void* xl6ChannelDirectReceiveCreateLanguageV1(XL6_SequenceLayoutV1,const xvalue*);
XL6_TEXT_API xvalue* xl6ChannelReceiveFutureResultLanguageV1(void*);
XL6_TEXT_API void xl6ChannelSendFutureBuilderDestroyV1(void*);
XL6_TEXT_API void xl6ChannelReceiveFutureBuilderDestroyV1(void*);
XL6_TEXT_API const struct XL6_NativeHandlePolicyV1* xl6ChannelSendFutureBuilderHandlePolicyV1Get(void);
XL6_TEXT_API const struct XL6_NativeHandlePolicyV1* xl6ChannelReceiveFutureBuilderHandlePolicyV1Get(void);
XL6_TEXT_API xfuture* xl6ChannelDirectSendStartLanguageV1(XL6_RuntimeServicesV1,void*);
XL6_TEXT_API xfuture* xl6ChannelDirectSendStartCancelLanguageV1(XL6_RuntimeServicesV1,void*,xcancel*);
XL6_TEXT_API xfuture* xl6ChannelDirectReceiveStartLanguageV1(XL6_RuntimeServicesV1,XL6_NativeClassFieldContextV1,void*,const xvalue*);
XL6_TEXT_API xfuture* xl6ChannelDirectReceiveStartCancelLanguageV1(XL6_RuntimeServicesV1,XL6_NativeClassFieldContextV1,void*,const xvalue*,xcancel*);
XL6_TEXT_API void xl6ChannelReceiveResultDestroyV1(struct XL6_ChannelReceiveResultV1*);
XL6_TEXT_API const struct XL6_NativeHandlePolicyV1* xl6ChannelReceiveResultHandlePolicyV1Get(void);
XL6_TEXT_API int64_t xl6ChannelReceiveResultStatusLanguageV1(const void*);
XL6_TEXT_API void xl6ChannelReceiveResultValueLanguageV1(struct XL6_FutureValueProfileV1,const void*,void*);
#endif
