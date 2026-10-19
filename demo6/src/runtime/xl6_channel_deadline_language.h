#ifndef XL6_CHANNEL_DEADLINE_LANGUAGE_H
#define XL6_CHANNEL_DEADLINE_LANGUAGE_H
#include "xl6_channel_language_abi.h"
XL6_TEXT_API void* xl6ChannelDeadlineSendForLanguageV1(XL6_SequenceLayoutV1,const xvalue*,const void*,int64_t);
XL6_TEXT_API void* xl6ChannelDeadlineSendUntilLanguageV1(XL6_SequenceLayoutV1,const xvalue*,const void*,uint64_t);
XL6_TEXT_API void* xl6ChannelDeadlineReceiveForLanguageV1(XL6_SequenceLayoutV1,const xvalue*,int64_t);
XL6_TEXT_API void* xl6ChannelDeadlineReceiveUntilLanguageV1(XL6_SequenceLayoutV1,const xvalue*,uint64_t);
XL6_TEXT_API void xl6ChannelDeadlineSendBuilderDestroyV1(void*);
XL6_TEXT_API void xl6ChannelDeadlineReceiveBuilderDestroyV1(void*);
XL6_TEXT_API const struct XL6_NativeHandlePolicyV1* xl6ChannelDeadlineSendBuilderHandlePolicyV1Get(void);
XL6_TEXT_API const struct XL6_NativeHandlePolicyV1* xl6ChannelDeadlineReceiveBuilderHandlePolicyV1Get(void);
XL6_TEXT_API xvalue* xl6ChannelDeadlineReceiveResultLanguageV1(void*);
XL6_TEXT_API xfuture* xl6ChannelDeadlineSendStartLanguageV1(XL6_RuntimeServicesV1,void*);
XL6_TEXT_API xfuture* xl6ChannelDeadlineSendStartCancelLanguageV1(XL6_RuntimeServicesV1,void*,xcancel*);
XL6_TEXT_API xfuture* xl6ChannelDeadlineReceiveStartLanguageV1(XL6_RuntimeServicesV1,XL6_ChannelResultFieldContextV1,void*,const xvalue*);
XL6_TEXT_API xfuture* xl6ChannelDeadlineReceiveStartCancelLanguageV1(XL6_RuntimeServicesV1,XL6_ChannelResultFieldContextV1,void*,const xvalue*,xcancel*);
XL6_TEXT_API void* xl6ChannelDeadlineSelectForLanguageV1(XL6_ChannelCaseArrayContextV1,const xvalue*,int64_t);
XL6_TEXT_API void* xl6ChannelDeadlineSelectUntilLanguageV1(XL6_ChannelCaseArrayContextV1,const xvalue*,uint64_t);
XL6_TEXT_API void xl6ChannelDeadlineSelectBuilderDestroyV1(void*);
XL6_TEXT_API const struct XL6_NativeHandlePolicyV1* xl6ChannelDeadlineSelectBuilderHandlePolicyV1Get(void);
XL6_TEXT_API xvalue* xl6ChannelDeadlineSelectResultLanguageV1(void*);
XL6_TEXT_API xfuture* xl6ChannelDeadlineSelectStartLanguageV1(XL6_RuntimeServicesV1,XL6_ChannelResultFieldContextV1,void*,const xvalue*);
XL6_TEXT_API xfuture* xl6ChannelDeadlineSelectStartCancelLanguageV1(XL6_RuntimeServicesV1,XL6_ChannelResultFieldContextV1,void*,const xvalue*,xcancel*);
#endif
