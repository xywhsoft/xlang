#pragma once
#include <xge3d_scene.h>
#ifdef __cplusplus
extern "C" {
#endif
void fixture3dSceneBudget(int);
void fixture3dSceneMark(void);
void fixture3dSceneBalanced(void);
void fixture3dSceneHeapBegin(void);
void fixture3dSceneHeapEnd(void);
void fixture3dSceneColdBegin(void);
void fixture3dSceneColdEnd(int);
int fixture3dSceneCall(xge3d_scene*,void(*)(void*),void*);
size_t fixture3dSceneNativeCount(xge3d_scene*);
#ifdef __cplusplus
}
#endif
