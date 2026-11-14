#ifndef XL6_XUI3_CODE_TOKEN_BUFFER_H
#define XL6_XUI3_CODE_TOKEN_BUFFER_H
#include <xge3_runtime.h>
/* Independent numeric cache. No retained Document, callback, native child or
 * language reference. The generic owner defers destruction during calls. */
static inline bool xl6Xui3CodeTokenBufferDestroy(void* raw)
{xuiCodeTokenBufferDestroy((xui_code_token_buffer)raw);return true;}
static inline const XL6_ForeignViewDescriptorV1* xl6Xui3CodeTokenBufferPolicyGet(void)
{
 static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,"xui.codeTokenBuffer",
  0,1,NULL,NULL,xl6Xui3CodeTokenBufferDestroy,0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};
 return &value;
}
#define xl6Xui3CodeTokenBufferPolicy (*xl6Xui3CodeTokenBufferPolicyGet())
#endif
