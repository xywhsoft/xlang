#ifndef FOREIGN_XGE3_SPATIAL_VALUES_H
#define FOREIGN_XGE3_SPATIAL_VALUES_H
#include <xge3_runtime.h>
#ifdef __cplusplus
extern "C" {
#endif
xge3d_vec3_t fixtureSpatialPosition(xge3d_dvec3_t origin, xge3d_dvec3_t global);
xge3d_ray_t fixtureSpatialRay(xge3d_camera_t camera);
void fixtureSpatialNative(void);
void fixtureSpatialHotBegin(void);
void fixtureSpatialHotEnd(void);
#ifdef __cplusplus
}
#endif
#endif
