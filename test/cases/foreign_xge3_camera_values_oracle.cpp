#include "test/cases/foreign_xge3_camera_values.h"
#include <cassert>
extern "C" xge3d_transform_t fixtureCameraIdentity(void)
{ return XGE3D_TRANSFORM_IDENTITY; }
extern "C" xge3d_mat4_t fixtureCameraMatrix(xge3d_transform_t value)
{
    xge3d_mat4_t result;
    assert(xge3dTransformMatrix(&value,&result)==XGE_OK);
    return result;
}
