#ifndef FIXTURE_XGE3_SCENE_OOM_H
#define FIXTURE_XGE3_SCENE_OOM_H
#include "foreign_xge3_scene.h"
static inline void fixtureSceneFailAfter(int value){(void)xrtMemDebugFailAfter(value);}
static inline void fixtureSceneFailClear(void){xrtMemDebugFailClear();xrtClearError();}
#endif
