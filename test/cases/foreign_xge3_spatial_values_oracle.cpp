#include "foreign_xge3_spatial_values.h"
#include <cassert>
extern "C" xge3d_vec3_t fixtureSpatialPosition(xge3d_dvec3_t origin, xge3d_dvec3_t global)
{
    xge3d_vec3_t result;
    assert(xge3dRelativePosition(origin,global,&result)==XGE_OK);
    return result;
}
extern "C" xge3d_ray_t fixtureSpatialRay(xge3d_camera_t camera)
{
    xge3d_ray_t result;
    assert(xge3dCameraScreenRay(&camera,400,300,800,600,&result)==XGE_OK);
    return result;
}
