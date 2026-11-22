#include "foreign_xui_accessibility_provider.h"
#include <assert.h>
#include <stdio.h>

static xmemdebugsnapshot before;
void fixtureAccessibleHeapBegin(void)
{
 assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
 xrtMemDebugSnapshot(&before);
}

void fixtureAccessibleBudget(int allowance)
{
 fixtureAccessibleHeapBegin();
 assert(xrtMemDebugFailAfter(allowance));
}

void fixtureAccessibleHeapEnd(void)
{
 xrtMemDebugFailClear();
 xmemdebugsnapshot after;
 xrtMemDebugSnapshot(&after);
 assert(after.LiveCount==before.LiveCount && after.LiveBytes==before.LiveBytes);
 assert(after.InvalidFreeCount==before.InvalidFreeCount && after.DoubleFreeCount==before.DoubleFreeCount);
}

void fixtureAccessibleHotEnd(int native)
{
 xmemdebugsnapshot after;
 xrtMemDebugSnapshot(&after);
 printf("Accessibility count %s alloc=%llu free=%llu realloc=%llu\n",native?"C":"xlang",
  (unsigned long long)(after.AllocCount-before.AllocCount),
  (unsigned long long)(after.FreeCount-before.FreeCount),
  (unsigned long long)(after.ReallocCount-before.ReallocCount));
 assert(after.AllocCount==before.AllocCount && after.FreeCount==before.FreeCount &&
  after.ReallocCount==before.ReallocCount);
}

void fixtureAccessibleNativeCount(xui_widget widget)
{
 for(int i=0;i<1000;++i) assert(xuiWidgetGetAccessibleNodeCountChecked(widget)==1);
}
