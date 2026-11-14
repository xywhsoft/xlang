#include "foreign_nullable_array_owned.h"
#include "foreign_xui_render_backend.c"
static xmemdebugsnapshot arrayBound,arrayHeap;
void fixtureNullableArrayBoundBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&arrayBound);}
int fixtureNullableArrayBoundEnd(void)
{
 xmemdebugsnapshot now;xrtMemDebugSnapshot(&now);
 uint64 count=now.AllocCount-arrayBound.AllocCount+now.ReallocCount-arrayBound.ReallocCount+8;
 assert(count>=8&&count<8192);return (int)count;
}
void fixtureNullableArrayHeapBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&arrayHeap);}
void fixtureNullableArrayHeapEqual(void)
{
 xmemdebugsnapshot now;xrtMemDebugSnapshot(&now);
 if(now.LiveCount!=arrayHeap.LiveCount||now.LiveBytes!=arrayHeap.LiveBytes||now.InvalidFreeCount!=arrayHeap.InvalidFreeCount||now.DoubleFreeCount!=arrayHeap.DoubleFreeCount)
  printf("Nullable array heap: live=%llu/%llu bytes=%llu/%llu invalid=%llu/%llu double=%llu/%llu\n",
  (unsigned long long)arrayHeap.LiveCount,(unsigned long long)now.LiveCount,
  (unsigned long long)arrayHeap.LiveBytes,(unsigned long long)now.LiveBytes,
  (unsigned long long)arrayHeap.InvalidFreeCount,(unsigned long long)now.InvalidFreeCount,
  (unsigned long long)arrayHeap.DoubleFreeCount,(unsigned long long)now.DoubleFreeCount);
 assert(now.LiveCount==arrayHeap.LiveCount&&now.LiveBytes==arrayHeap.LiveBytes&&now.InvalidFreeCount==arrayHeap.InvalidFreeCount&&now.DoubleFreeCount==arrayHeap.DoubleFreeCount);
}
