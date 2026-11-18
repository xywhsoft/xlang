#ifndef XL6_XGE3_SHAPE_SNAPSHOT_H
#define XL6_XGE3_SHAPE_SNAPSHOT_H
#include <xge3_checked_graphics.h>

#define XL6_XGE_SHAPE_SNAPSHOT_POLICY(Name,Type,Family,Release) \
static inline bool Name##Destroy(void* raw) \
{ \
    int status = Release((Type*)raw); \
    return xl6Xge3GraphicsConsumed(status,status == XGE_OK,#Release); \
} \
static inline const XL6_ForeignViewDescriptorV1* Name##Get(void) \
{ \
    static const XL6_ForeignViewDescriptorV1 policy={sizeof(policy),XL6_FOREIGN_VIEW_V1_TAG, \
        Family,0,1,NULL,NULL,Name##Destroy,0,1,NULL,NULL,NULL,NULL, \
        XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE,Name##Destroy,NULL}; \
    return &policy; \
}
XL6_XGE_SHAPE_SNAPSHOT_POLICY(xl6Xge3ShapePathSnapshotPolicy,xge_shape_path_snapshot,
    "xge.shape.path-snapshot",xgeShapePathSnapshotReleaseChecked)
XL6_XGE_SHAPE_SNAPSHOT_POLICY(xl6Xge3ShapeGradientSnapshotPolicy,xge_shape_gradient_snapshot,
    "xge.shape.gradient-snapshot",xgeShapeGradientSnapshotReleaseChecked)
XL6_XGE_SHAPE_SNAPSHOT_POLICY(xl6Xge3ShapeDashSnapshotPolicy,xge_shape_dash_snapshot,
    "xge.shape.dash-snapshot",xgeShapeDashSnapshotReleaseChecked)
#undef XL6_XGE_SHAPE_SNAPSHOT_POLICY

#define xl6Xge3ShapePathSnapshotPolicy (*xl6Xge3ShapePathSnapshotPolicyGet())
#define xl6Xge3ShapeGradientSnapshotPolicy (*xl6Xge3ShapeGradientSnapshotPolicyGet())
#define xl6Xge3ShapeDashSnapshotPolicy (*xl6Xge3ShapeDashSnapshotPolicyGet())
#endif
