#ifndef FOREIGN_XGE3D_SCENE_CONTROLS_H
#define FOREIGN_XGE3D_SCENE_CONTROLS_H
#ifndef XRT_MODULE_ALL
#define XRT_MODULE_ALL
#endif
#include <xge3d_scene.h>
#ifdef __cplusplus
extern "C" {
#endif
void fixture3dControlsTriangle(xge3d_scene*,xge3d_node_t);
void fixture3dControlsAppearance(xge3d_scene*,xge3d_node_t,int);
void fixture3dControlsBudget(int);
int fixture3dControlsTriggered(void);
void fixture3dControlsWarmBegin(void);
void fixture3dControlsWarmEnd(void);
void fixture3dControlsNativeWarm(xge3d_scene*,xge3d_node_t);
int fixture3dControlsNativeMain(void);
#ifdef __cplusplus
}
#endif
#endif
