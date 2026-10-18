#ifndef XL6_CHANNEL_LANGUAGE_ABI_H
#define XL6_CHANNEL_LANGUAGE_ABI_H
#include "xl6_sequence_abi.h"
#include "xl6_channel_case_abi.h"
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
typedef struct XL6_ChannelCaseArrayContextV1 {
    XL6_SequenceLayoutV1 layout;
    const char* fieldName;
    size_t fieldSize;
    const struct XL6_NativeHandlePolicyV1* policy;
} XL6_ChannelCaseArrayContextV1;
XL6_TEXT_API void* xl6ChannelCasesWaitLanguageV1(XL6_ChannelCaseArrayContextV1,const xvalue*);
XL6_TEXT_API void* xl6ChannelCasesTryLanguageV1(XL6_ChannelCaseArrayContextV1,const xvalue*);
XL6_TEXT_API void* xl6ChannelCasesForLanguageV1(XL6_ChannelCaseArrayContextV1,const xvalue*,int64_t);
XL6_TEXT_API void* xl6ChannelCasesUntilLanguageV1(XL6_ChannelCaseArrayContextV1,const xvalue*,uint64_t);
XL6_TEXT_API void* xl6ChannelCasesWaitCancelLanguageV1(XL6_ChannelCaseArrayContextV1,const xvalue*,xcancel*);
XL6_TEXT_API void* xl6ChannelCasesForCancelLanguageV1(XL6_ChannelCaseArrayContextV1,const xvalue*,int64_t,xcancel*);
XL6_TEXT_API void* xl6ChannelCasesUntilCancelLanguageV1(XL6_ChannelCaseArrayContextV1,const xvalue*,uint64_t,xcancel*);
#endif
