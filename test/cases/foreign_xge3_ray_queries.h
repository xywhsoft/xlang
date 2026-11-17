#ifndef FOREIGN_XGE3_RAY_QUERIES_H
#define FOREIGN_XGE3_RAY_QUERIES_H
#include <xge3_runtime.h>
#ifdef __cplusplus
extern "C" {
#endif
int fixtureRayInput(int which,xge3d_ray_t* ray,xge3d_aabb_t* box,
    xge3d_sphere_t* sphere,xge3d_vec3_t triangle[3],float* maximum);
void fixtureRayCompare(int which,int box_status,float box_distance,
    int sphere_status,float sphere_distance,int triangle_status,xge3d_hit_t hit);
void fixtureRayNativeOracle(void);
void fixtureRayHeapBegin(void);
void fixtureRayHeapEnd(int native);
void fixtureRayNativeHot(void);
#ifdef __cplusplus
}
#endif
#endif
