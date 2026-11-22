#pragma once
#include "xge3d_renderer.h"

void fixtureAnimationOptionsBegin(void);
void fixtureAnimationOptionsEnd(int native);
void fixtureAnimationOptionsNative(xge3d_animator* animator,xge3d_clip* clip);
void fixtureAnimationOptionsFail(int prefix);
