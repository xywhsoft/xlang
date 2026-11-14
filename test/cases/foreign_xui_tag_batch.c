#include "foreign_xui_tag_batch.h"
#include "foreign_xui_text_layout.c"
static xmemdebugsnapshot tagPrefix;
void fixtureTagBudget(int n){fixtureLayoutBudget(n);}
void fixtureTagCleanup(void)
{assert(references==1&&fonts==1&&paints==shapes&&surfaces==0&&draws==0);}
void fixtureTagPrefixBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&tagPrefix);}
void fixtureTagPrefixEnd(void){xrtMemDebugFailClear();xrtClearError();}
void fixtureTagHeapEqual(void)
{
 xmemdebugsnapshot now;xrtMemDebugSnapshot(&now);
 assert(now.LiveCount==tagPrefix.LiveCount&&now.LiveBytes==tagPrefix.LiveBytes&&now.InvalidFreeCount==tagPrefix.InvalidFreeCount&&now.DoubleFreeCount==tagPrefix.DoubleFreeCount);
}
void fixtureTagHotBegin(void){fixtureLayoutHeapBegin();}
void fixtureTagHotReport(int kind){fixtureLayoutHeapReport(kind);}
void fixtureTagNativeHot(xui_widget widget)
{
 const char* values[]={"a","b","c"};
 for(int i=0;i<1000;++i)assert(xuiTagInputSetTags(widget,values,3)==XUI_OK);
}
