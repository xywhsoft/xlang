#include "foreign_xge3_camera_values.h"
#include <xrt_decl.h>
#include <assert.h>
#include <stdio.h>
static xmemdebugsnapshot before;
static void equal(const char *message)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.AllocCount==after.AllocCount && before.FreeCount==after.FreeCount &&
        before.ReallocCount==after.ReallocCount);
    puts(message);
}
void fixtureCameraHotBegin(void){xrtMemDebugSnapshot(&before);}
void fixtureCameraHotEnd(void){equal("Language 3D values: zero XRT heap operations");}
void fixtureCameraNativeLoop(void)
{
    fixtureCameraHotBegin();
    for(unsigned i=0;i<1000;++i) {
        xge3d_transform_t transform=XGE3D_TRANSFORM_IDENTITY;
        transform.rotation.w=2;transform.scale=(xge3d_vec3_t){2,3,4};transform.position.x=5;
        xge3d_mat4_t matrix;xge3d_camera_t perspective,orthographic;
        assert(xge3dTransformMatrix(&transform,&matrix)==XGE_OK);
        assert(xge3dCameraPerspective(&perspective,1,1.5f,.1f,100)==XGE_OK);
        assert(xge3dCameraOrthographic(&orthographic,8,4,0,100)==XGE_OK);
        assert(matrix.m[0]==2 && matrix.m[5]==3 && matrix.m[10]==4 && matrix.m[12]==5);
        assert(perspective.projection.m[11]==-1 && orthographic.projection.m[0]==.25f);
    }
    equal("Native 3D values: zero XRT heap operations");
}
