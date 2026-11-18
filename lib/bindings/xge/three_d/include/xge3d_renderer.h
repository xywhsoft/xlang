#ifndef XL6_XGE3D_RENDERER_H
#define XL6_XGE3D_RENDERER_H
#include <xge3_checked_graphics.h>

/* Native resources retain their graphics lifetime until final release.
 * The language policy forwards native consumption and checked status. */
#define XL6_XGE3D_CHECKED_OWNER(Name,Type,Family,Release) \
static inline bool Name##Destroy(void* raw) \
{ \
 bool consumed=false;int status=Release((Type*)raw,&consumed); \
 return xl6Xge3GraphicsConsumed(status,consumed,#Release); \
} \
static inline const XL6_ForeignViewDescriptorV1* Name##Get(void) \
{ \
 static const XL6_ForeignViewDescriptorV1 policy={sizeof(policy),XL6_FOREIGN_VIEW_V1_TAG, \
  Family,0,1,NULL,NULL,Name##Destroy,0,1,NULL,NULL,NULL,NULL, \
  XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE,Name##Destroy,NULL}; \
 return &policy; \
}
XL6_XGE3D_CHECKED_OWNER(xl6Xge3dRendererPolicy,xge3d_renderer,"xge3d.renderer",xge3dRendererReleaseOwnerChecked)
XL6_XGE3D_CHECKED_OWNER(xl6Xge3dTexturePolicy,xge3d_texture,"xge3d.texture",xge3dTextureReleaseOwnerChecked)
XL6_XGE3D_CHECKED_OWNER(xl6Xge3dTargetPolicy,xge3d_target,"xge3d.target",xge3dTargetReleaseOwnerChecked)
XL6_XGE3D_CHECKED_OWNER(xl6Xge3dMeshPolicy,xge3d_mesh,"xge3d.mesh",xge3dMeshReleaseOwnerChecked)
XL6_XGE3D_CHECKED_OWNER(xl6Xge3dMaterialPolicy,xge3d_material,"xge3d.material",xge3dMaterialReleaseOwnerChecked)
XL6_XGE3D_CHECKED_OWNER(xl6Xge3dShaderPolicy,xge3d_shader,"xge3d.shader",xge3dShaderReleaseOwnerChecked)
#undef XL6_XGE3D_CHECKED_OWNER
#define xl6Xge3dRendererPolicy (*xl6Xge3dRendererPolicyGet())
#define xl6Xge3dTexturePolicy (*xl6Xge3dTexturePolicyGet())
#define xl6Xge3dTargetPolicy (*xl6Xge3dTargetPolicyGet())
#define xl6Xge3dMeshPolicy (*xl6Xge3dMeshPolicyGet())
#define xl6Xge3dMaterialPolicy (*xl6Xge3dMaterialPolicyGet())
#define xl6Xge3dShaderPolicy (*xl6Xge3dShaderPolicyGet())
#endif
