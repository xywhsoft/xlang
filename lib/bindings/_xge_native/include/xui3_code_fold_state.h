#ifndef XL6_XUI3_CODE_FOLD_STATE_H
#define XL6_XUI3_CODE_FOLD_STATE_H
#include <xui3_code_cleanup.h>
/* Independent numeric leaf: no retained document, native child, callback or
 * language edge. The existing language root defers destruction until its
 * active foreign calls return; no simulated native activity scope is needed. */
static inline bool xl6Xui3CodeFoldStateDestroy(void* raw)
{
 int status=xuiCodeFoldStateDestroyChecked((xui_code_fold_state)raw);
 return xl6Xui3CodeCleanupResult(status,"xuiCodeFoldStateDestroyChecked");
}
static inline const XL6_ForeignViewDescriptorV1* xl6Xui3CodeFoldStatePolicyGet(void)
{
 static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,"xui.codeFoldState",
  0,1,NULL,NULL,xl6Xui3CodeFoldStateDestroy,0,1,NULL,NULL,NULL,NULL,XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE,NULL,NULL};
 return &value;
}
#define xl6Xui3CodeFoldStatePolicy (*xl6Xui3CodeFoldStatePolicyGet())
#endif
