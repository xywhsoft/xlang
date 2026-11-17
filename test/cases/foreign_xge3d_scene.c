#include "foreign_xge3d_scene.h"
#include <assert.h>
#include <stdio.h>
static xmemdebugsnapshot before,hot,cold;
void fixture3dSceneBudget(int allowance)
{if(allowance<0){xrtMemDebugFailClear();xrtClearError();}else assert(xrtMemDebugFailAfter(allowance));}
void fixture3dSceneMark(void)
{assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
void fixture3dSceneBalanced(void)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 /* Resident compiler/code/catalog caches are outside this native-body check;
  * all live scene/view failures are also checked by module retirement. */
 assert(after.InvalidFreeCount==before.InvalidFreeCount && after.DoubleFreeCount==before.DoubleFreeCount);
}
void fixture3dSceneHeapBegin(void){xrtMemDebugSnapshot(&hot);}
void fixture3dSceneHeapEnd(void)
{xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);assert(after.AllocCount==hot.AllocCount);puts("scene-query-xrt-allocations=0");}
void fixture3dSceneColdBegin(void){xrtMemDebugSnapshot(&cold);}
void fixture3dSceneColdEnd(int kind)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 printf("scene language cold kind=%d allocations=%llu bytes=%llu\n",kind,
  (unsigned long long)(after.AllocCount-cold.AllocCount),(unsigned long long)(after.LiveBytes-cold.LiveBytes));
}
int fixture3dSceneCall(xge3d_scene* scene,void(*callback)(void*),void* user)
{
 int status=xge3dSceneAccessBegin(scene);if(status!=XGE_OK)return status;
 callback(user);xerror* error=xrtTakeError();
 int ended=xge3dSceneAccessEnd(scene);if(error){xrtClearError();xrtSetErrorTake(error);}
 return ended;
}
size_t fixture3dSceneNativeCount(xge3d_scene* scene)
{
 size_t n=0;
 for(unsigned i=0;i<1000;++i){assert(xge3dSceneAccessBegin(scene)==XGE_OK);n=xge3dSceneNodeCount(scene);assert(xge3dSceneAccessEnd(scene)==XGE_OK);}
 return n;
}
