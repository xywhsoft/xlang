#ifndef XL6_XGE3_PASS_H
#define XL6_XGE3_PASS_H
#include "xge3_checked_graphics.h"
static inline bool xl6Xge3PassDestroy(void* raw)
{
 bool consumed=false;int status=xgePassReleaseOwnerChecked((xge_pass)raw,&consumed);
 return xl6Xge3GraphicsConsumed(status,consumed,"xgePassReleaseOwnerChecked");
}
static inline bool xl6Xge3PassRequestClose(void* raw)
{
 xge_pass pass=(xge_pass)raw;
 if((pass->bActive && !xgePassIsCurrent(pass)) ||
    (!pass->bActive && pass->pTarget->_iOwnerReferences==1 && pass->pTarget->_iActivePasses)){
  xl6ForeignStatusFailureV1(XGE_ERROR_BUSY,"xgePassReleaseOwnerChecked");return false;
 }
 return true;
}
static inline const XL6_ForeignViewDescriptorV1* xl6Xge3PassPolicyGet(void)
{
 static const XL6_ForeignViewDescriptorV1 policy={sizeof(policy),XL6_FOREIGN_VIEW_V1_TAG,
  "xge.pass",0,1,NULL,NULL,xl6Xge3PassDestroy,0,1,NULL,NULL,NULL,NULL,
  XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE,xl6Xge3PassDestroy,xl6Xge3PassRequestClose};
 return &policy;
}
#define xl6Xge3PassPolicy (*xl6Xge3PassPolicyGet())
#endif
