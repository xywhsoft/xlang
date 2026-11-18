#ifndef XL6_XGE3D_RENDERER_H
#define XL6_XGE3D_RENDERER_H
#include <xge3_checked_graphics.h>
#if XGE3D_ENABLE_BILLBOARD
#include <xge3_render_resources.h>
XL6_XGE_NATIVE_POLICY(xl6Xge3dBillboardBatchPolicy,"xge3d.billboardBatch",xge3d_billboard_batch,xge3dBillboardBatchFree)
#define xl6Xge3dBillboardBatchPolicy (*xl6Xge3dBillboardBatchPolicyGet())
#endif

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
static inline int xl6Xge3dReleaseGeometry(xge3d_mesh_geometry* geometry,bool* consumed)
{
    int status = xge3dMeshGeometryReleaseChecked(geometry);
    *consumed = status == XGE_OK;
    return status;
}
XL6_XGE3D_CHECKED_OWNER(xl6Xge3dMeshGeometryPolicy,xge3d_mesh_geometry,"xge3d.mesh.geometry",xl6Xge3dReleaseGeometry)
XL6_XGE3D_CHECKED_OWNER(xl6Xge3dMaterialPolicy,xge3d_material,"xge3d.material",xge3dMaterialReleaseOwnerChecked)
XL6_XGE3D_CHECKED_OWNER(xl6Xge3dShaderPolicy,xge3d_shader,"xge3d.shader",xge3dShaderReleaseOwnerChecked)
XL6_XGE3D_CHECKED_OWNER(xl6Xge3dEnvironmentPolicy,xge3d_environment,"xge3d.environment",xge3dEnvironmentReleaseOwnerChecked)
#if XGE3D_ENABLE_MODEL
XL6_XGE3D_CHECKED_OWNER(xl6Xge3dModelPolicy,xge3d_model,"xge3d.model",xge3dModelReleaseOwnerChecked)
XL6_XGE3D_CHECKED_OWNER(xl6Xge3dModelRetirementPolicy,xge3d_model_retirement,"xge3d.model.retirement",xge3dModelRetireReleaseOwnerChecked)
XL6_XGE3D_CHECKED_OWNER(xl6Xge3dInstanceBuilderPolicy,xge3d_instance_builder,"xge3d.model.instance-builder",xge3dModelInstanceReleaseOwnerChecked)
#define xl6Xge3dModelPolicy (*xl6Xge3dModelPolicyGet())
#define xl6Xge3dModelRetirementPolicy (*xl6Xge3dModelRetirementPolicyGet())
#define xl6Xge3dInstanceBuilderPolicy (*xl6Xge3dInstanceBuilderPolicyGet())
#endif
#if XGE3D_ENABLE_ASYNC
XL6_XGE3D_CHECKED_OWNER(xl6Xge3dModelLoaderPolicy,xge3d_loader,"xge3d.model.loader",xge3dLoaderReleaseOwnerChecked)
#define xl6Xge3dModelLoaderPolicy (*xl6Xge3dModelLoaderPolicyGet())
#endif
#if XGE3D_ENABLE_CACHE
XL6_XGE3D_CHECKED_OWNER(xl6Xge3dModelCachePolicy,xge3d_cache,"xge3d.model.cache",xge3dCacheReleaseOwnerChecked)
#define xl6Xge3dModelCachePolicy (*xl6Xge3dModelCachePolicyGet())
#endif
#if XGE3D_ENABLE_ANIMATION
XL6_XGE3D_CHECKED_OWNER(xl6Xge3dClipPolicy,xge3d_clip,"xge3d.animation.clip",xge3dClipReleaseOwnerChecked)
XL6_XGE3D_CHECKED_OWNER(xl6Xge3dAnimatorPolicy,xge3d_animator,"xge3d.animation.animator",xge3dAnimatorReleaseOwnerChecked)
#define xl6Xge3dClipPolicy (*xl6Xge3dClipPolicyGet())
#define xl6Xge3dAnimatorPolicy (*xl6Xge3dAnimatorPolicyGet())
#endif
#if XGE3D_ENABLE_TERRAIN
XL6_XGE3D_CHECKED_OWNER(xl6Xge3dTerrainPolicy,xge3d_terrain,"xge3d.terrain",xge3dTerrainReleaseOwnerChecked)
#define xl6Xge3dTerrainPolicy (*xl6Xge3dTerrainPolicyGet())
#endif
#if XGE3D_ENABLE_COLLISION
XL6_XGE3D_CHECKED_OWNER(xl6Xge3dQueryResultsPolicy,xge3d_query_results,"xge3d.query-results",xge3dQueryResultsRelease)
#define xl6Xge3dQueryResultsPolicy (*xl6Xge3dQueryResultsPolicyGet())
#endif
#undef XL6_XGE3D_CHECKED_OWNER
#define xl6Xge3dRendererPolicy (*xl6Xge3dRendererPolicyGet())
#define xl6Xge3dTexturePolicy (*xl6Xge3dTexturePolicyGet())
#define xl6Xge3dTargetPolicy (*xl6Xge3dTargetPolicyGet())
#define xl6Xge3dMeshPolicy (*xl6Xge3dMeshPolicyGet())
#define xl6Xge3dMeshGeometryPolicy (*xl6Xge3dMeshGeometryPolicyGet())
#define xl6Xge3dMaterialPolicy (*xl6Xge3dMaterialPolicyGet())
#define xl6Xge3dShaderPolicy (*xl6Xge3dShaderPolicyGet())
#define xl6Xge3dEnvironmentPolicy (*xl6Xge3dEnvironmentPolicyGet())
#endif
