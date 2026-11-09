#ifndef XL6_XGE3_RENDER_TARGET_H
#define XL6_XGE3_RENDER_TARGET_H
#include "xge3_checked_graphics.h"
static inline bool xl6Xge3TargetDestroy(void* raw)
{
 bool consumed=false;int status=xgeRenderTargetReleaseOwnerChecked((xge_render_target)raw,&consumed);
 return xl6Xge3GraphicsConsumed(status,consumed,"xgeRenderTargetReleaseOwnerChecked");
}
static inline bool xl6Xge3TargetRequestClose(void* raw)
{
 xge_render_target target=(xge_render_target)raw;
 if(target->_iOwnerReferences==1 && (target->_iActivePasses || target->tTexture.iRefCount>1)){
  xl6ForeignStatusFailureV1(XGE_ERROR_BUSY,"xgeRenderTargetReleaseOwnerChecked");return false;
 }
 return true;
}
static inline const XL6_ForeignViewDescriptorV1* xl6Xge3TargetPolicyGet(void)
{
 static const XL6_ForeignViewDescriptorV1 policy={sizeof(policy),XL6_FOREIGN_VIEW_V1_TAG,
  "xge.renderTarget",0,1,NULL,NULL,xl6Xge3TargetDestroy,0,1,NULL,NULL,NULL,NULL,
  XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE,xl6Xge3TargetDestroy,xl6Xge3TargetRequestClose};
 return &policy;
}
#define xl6Xge3TargetPolicy (*xl6Xge3TargetPolicyGet())
#endif
