#include "foreign_xui_code_commands.h"
#include <assert.h>
#include <stdio.h>
static xmemdebugsnapshot commandPrefix,commandHot;
void fixtureCommandBudget(int n)
{if(n<0)xrtMemDebugFailClear();else{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(n));}}
void fixtureCommandPrefixBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&commandPrefix);}
void fixtureCommandPrefixEnd(void){xrtMemDebugFailClear();xrtClearError();}
void fixtureCommandHeapEqual(void)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 assert(after.LiveCount==commandPrefix.LiveCount&&after.LiveBytes==commandPrefix.LiveBytes&&
  after.InvalidFreeCount==commandPrefix.InvalidFreeCount&&after.DoubleFreeCount==commandPrefix.DoubleFreeCount);
}
void fixtureCommandHotBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&commandHot);}
void fixtureCommandHotReport(int kind)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 printf("CodeCommand heap kind=%d alloc=%llu free=%llu realloc=%llu\n",kind,
  (unsigned long long)(after.AllocCount-commandHot.AllocCount),(unsigned long long)(after.FreeCount-commandHot.FreeCount),
  (unsigned long long)(after.ReallocCount-commandHot.ReallocCount));
}
void fixtureCommandNativeHot(xui_code_command_map map)
{
 for(unsigned i=0;i<1000;++i){int command=-1;xui_code_key_binding_t binding={0};
  assert(xuiCodeCommandMapGetCount(map)>0);
  assert(xuiCodeCommandMapFind(map,XUI_KEY_F2,XUI_MOD_CTRL,&command)==XUI_OK&&command==7777);
  assert(xuiCodeCommandMapGetBinding(map,0,&binding)==XUI_OK&&binding.iSize==sizeof(binding));
  assert(xuiCodeCommandMapBind(map,XUI_KEY_F2,XUI_MOD_CTRL,7777)==XUI_OK);
 }
}
void fixtureCommandNativeCreate(void)
{xui_code_command_map map=NULL;assert(xuiCodeCommandMapCreate(&map)==XUI_OK);xuiCodeCommandMapDestroy(map);}
int fixtureCommandDuringCall(xui_code_command_map map,void (*callback)(void*),void* user)
{callback(user);return xuiCodeCommandMapGetCount(map)>0 ? XUI_OK:XUI_ERROR_INVALID_STATE;}
