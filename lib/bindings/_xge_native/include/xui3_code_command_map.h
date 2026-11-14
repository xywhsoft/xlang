#ifndef XL6_XUI3_CODE_COMMAND_MAP_H
#define XL6_XUI3_CODE_COMMAND_MAP_H
#include <xge3_runtime.h>
/* Independent numeric bindings; no retained document, callback or language
 * edge. Generic owner activity defers native destruction during a call. */
static inline bool xl6Xui3CodeCommandMapDestroy(void* raw)
{xuiCodeCommandMapDestroy((xui_code_command_map)raw);return true;}
static inline const XL6_ForeignViewDescriptorV1* xl6Xui3CodeCommandMapPolicyGet(void)
{
 static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,"xui.codeCommandMap",
  0,1,NULL,NULL,xl6Xui3CodeCommandMapDestroy,0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};
 return &value;
}
#define xl6Xui3CodeCommandMapPolicy (*xl6Xui3CodeCommandMapPolicyGet())
#endif
