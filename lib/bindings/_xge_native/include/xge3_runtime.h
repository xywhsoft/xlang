#ifndef XL6_XGE3_RUNTIME_H
#define XL6_XGE3_RUNTIME_H
#include <xrt_decl.h>
#include <xge_build_config.h>
#ifndef XGE_XRT_HOST
#define XGE_XRT_HOST 1
#endif
#if defined(_WIN32)
#ifndef XGE_DLL
#define XGE_DLL 1
#endif
#ifndef XUI_DLL
#define XUI_DLL 1
#endif
#endif
#include <xge.h>
#include <xui.h>
#include <xge_host_xrt.h>
#include <__xl_runtime/xl6_foreign_view.h>

#ifdef __cplusplus
extern "C" {
#endif
/* Cold checked initialization. Repeat calls validate the identical runtime;
 * the native engine never acquires or embeds a second XRT implementation. */
int xl6Xge3RuntimeBindV1(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3ContextPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3WidgetPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3TableCellOutputPolicyGet(void);
const XL6_OwnershipDomainV1* xuiTableViewCellOutputOwnershipDomainV1Get(void);
const XL6_OwnershipDomainV1* xl6Xui3OwnershipDomainV1Get(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3ProxyPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3FontPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3SurfacePolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3PathPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xge3FontPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xge3TexturePolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xge3ImagePolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xge3MaterialPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xge3ShapePolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xge3ShapeScenePolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xge3PathMeasurePolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xge3AudioClipPolicyGet(void);
#ifdef __cplusplus
}
#endif
/* Getter expressions also work when a C provider lives in another PE image;
 * importing a data symbol through a function-only registry is not portable. */
#define xl6Xui3ContextPolicy (*xl6Xui3ContextPolicyGet())
#define xl6Xui3WidgetPolicy (*xl6Xui3WidgetPolicyGet())
#define xl6Xui3TableCellOutputPolicy (*xl6Xui3TableCellOutputPolicyGet())
#define xuiTableViewCellOutputOwnershipDomainV1 (*xuiTableViewCellOutputOwnershipDomainV1Get())
#define xl6Xui3OwnershipDomainV1 (*xl6Xui3OwnershipDomainV1Get())
#define xl6Xui3ProxyPolicy (*xl6Xui3ProxyPolicyGet())
#define xl6Xui3FontPolicy (*xl6Xui3FontPolicyGet())
#define xl6Xui3SurfacePolicy (*xl6Xui3SurfacePolicyGet())
#define xl6Xui3PathPolicy (*xl6Xui3PathPolicyGet())
#define xl6Xge3FontPolicy (*xl6Xge3FontPolicyGet())
#define xl6Xge3TexturePolicy (*xl6Xge3TexturePolicyGet())
#define xl6Xge3ImagePolicy (*xl6Xge3ImagePolicyGet())
#define xl6Xge3MaterialPolicy (*xl6Xge3MaterialPolicyGet())
#define xl6Xge3ShapePolicy (*xl6Xge3ShapePolicyGet())
#define xl6Xge3ShapeScenePolicy (*xl6Xge3ShapeScenePolicyGet())
#define xl6Xge3PathMeasurePolicy (*xl6Xge3PathMeasurePolicyGet())
#define xl6Xge3AudioClipPolicy (*xl6Xge3AudioClipPolicyGet())
#endif
