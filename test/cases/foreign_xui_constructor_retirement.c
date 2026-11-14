/* Full owned CPU resource provider. Production methods remain declarations. */
#include "foreign_xui_constructor_retirement.h"
#include "foreign_xui_render_backend.c"
static xmemdebugsnapshot boundBefore,heapBefore;
static int currentKind,currentBudget;
void fixtureRetireSelect(int kind,int budget){currentKind=kind;currentBudget=budget;}
void fixtureRetireBoundBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&boundBefore);}
int fixtureRetireBoundEnd(void)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 uint64 n=after.AllocCount-boundBefore.AllocCount+after.ReallocCount-boundBefore.ReallocCount+8;
 assert(n>=8&&n<=8192);return (int)n;
}
void fixtureRetireHeapBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&heapBefore);}
void fixtureRetireHeapEqual(void)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 if(after.LiveCount!=heapBefore.LiveCount||after.LiveBytes!=heapBefore.LiveBytes||after.InvalidFreeCount!=heapBefore.InvalidFreeCount||after.DoubleFreeCount!=heapBefore.DoubleFreeCount){
  printf("Language retirement kind=%d budget=%d live=%llu/%llu bytes=%llu/%llu invalid=%llu/%llu double=%llu/%llu\n",currentKind,currentBudget,(unsigned long long)heapBefore.LiveCount,(unsigned long long)after.LiveCount,(unsigned long long)heapBefore.LiveBytes,(unsigned long long)after.LiveBytes,(unsigned long long)heapBefore.InvalidFreeCount,(unsigned long long)after.InvalidFreeCount,(unsigned long long)heapBefore.DoubleFreeCount,(unsigned long long)after.DoubleFreeCount);fflush(stdout);
 }
 assert(after.LiveCount==heapBefore.LiveCount&&after.LiveBytes==heapBefore.LiveBytes&&after.InvalidFreeCount==heapBefore.InvalidFreeCount&&after.DoubleFreeCount==heapBefore.DoubleFreeCount);
}
void fixtureRetireMenuCheck(xui_widget menu,int added)
{
 assert(xuiMenuGetItemCount(menu)==(added?2:1));const xui_menu_item_t* first=xuiMenuGetItem(menu,0);
 assert(first&&!strcmp(first->sText,"kept")&&!strcmp(first->sShortcut,"Ctrl+K")&&first->iValue==1);
 if(added){const xui_menu_item_t* second=xuiMenuGetItem(menu,1);assert(second&&!strcmp(second->sText,"new")&&!strcmp(second->sShortcut,"Ctrl+N")&&second->iValue==7);}
}
