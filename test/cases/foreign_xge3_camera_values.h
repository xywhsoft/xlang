#ifndef FOREIGN_XGE3_CAMERA_VALUES_H
#define FOREIGN_XGE3_CAMERA_VALUES_H
#include <xge3_runtime.h>
#ifdef __cplusplus
extern "C" {
#endif
xge3d_transform_t fixtureCameraIdentity(void);
xge3d_mat4_t fixtureCameraMatrix(xge3d_transform_t value);
void fixtureCameraNativeLoop(void);
void fixtureCameraHotBegin(void);
void fixtureCameraHotEnd(void);
#ifdef __cplusplus
}
#endif
#endif
