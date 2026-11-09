#ifndef XL6_XGE3_INPUT_ACTIONS_H
#define XL6_XGE3_INPUT_ACTIONS_H
#include <xge3_runtime.h>
/* The native allocator owns the outer object; do not free it a second time. */
static inline bool xl6Xge3ActionsDestroy(void* raw)
{ return xgeActionsFreeChecked((xge_actions*)raw)==XGE_OK; }
static inline const XL6_ForeignViewDescriptorV1* xl6Xge3ActionsPolicyGet(void)
{
 static const XL6_ForeignViewDescriptorV1 policy={sizeof(policy),XL6_FOREIGN_VIEW_V1_TAG,
  "xge.actions",0,1,NULL,NULL,xl6Xge3ActionsDestroy,0,1,NULL,NULL,NULL,NULL,0,xl6Xge3ActionsDestroy,NULL};
 return &policy;
}
#define xl6Xge3ActionsPolicy (*xl6Xge3ActionsPolicyGet())
#endif
