#ifndef XL6_XGE3_RENDER_RESOURCES_H
#define XL6_XGE3_RENDER_RESOURCES_H
#include <xge3_runtime.h>

/* Cold record ownership only. Every business operation is declared against
 * its real native symbol; no draw/update forwarding functions are provided.
 * SpriteBatch's native Texture reference owns the adopted Texture record. */
#define XL6_XGE_RECORD_POLICY(Name,Family,Type,NativeFree) \
static inline bool Name##Destroy(void* raw) \
{ NativeFree((Type*)raw);xrtFree(raw);return true; } \
static inline const XL6_ForeignViewDescriptorV1* Name##Get(void) \
{ \
 static const XL6_ForeignViewDescriptorV1 policy={sizeof(policy),XL6_FOREIGN_VIEW_V1_TAG, \
  Family,0,1,NULL,NULL,Name##Destroy,0,1,NULL,NULL,NULL,NULL,0,Name##Destroy,NULL}; \
 return &policy; \
}

/* Each family instantiates this in its own header so importing one resource
 * does not define cleanup functions that reference three unrelated families. */
#endif
