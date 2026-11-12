#include "foreign_xge3_spatial_values.h"
#include <xrt_decl.h>
#include <assert.h>
#include <stdio.h>
static xmemdebugsnapshot before;
void fixtureSpatialHotBegin(void) { assert(xrtMemDebugEnabled()); xrtMemDebugSnapshot(&before); }
static void equal(const char* message)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(after.AllocCount==before.AllocCount && after.FreeCount==before.FreeCount && after.ReallocCount==before.ReallocCount);
    puts(message);
}
void fixtureSpatialHotEnd(void) { equal("Language spatial values: zero XRT heap operations"); }
void fixtureSpatialNative(void)
{
    xge3d_camera_t camera;
    assert(xge3dCameraPerspective(&camera,1,1.5f,.1f,100)==XGE_OK);
    fixtureSpatialHotBegin();
    for (unsigned i=0;i<1000;++i) {
        xge3d_vec3_t point; xge3d_ray_t ray;
        assert(xge3dRelativePosition((xge3d_dvec3_t){1e12,-1e12,2e12},
            (xge3d_dvec3_t){1e12+.25,-1e12-.5,2e12+4},&point)==XGE_OK);
        assert(point.x==.25f && point.y==-.5f && point.z==4);
        assert(xge3dCameraScreenRay(&camera,400,300,800,600,&ray)==XGE_OK);
        assert(ray.direction.x==0 && ray.direction.y==0 && ray.direction.z==-1);
    }
    equal("Native spatial values: zero XRT heap operations");
}
