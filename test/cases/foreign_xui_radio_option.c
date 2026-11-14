#include "foreign_xui_radio_option.h"
#include "foreign_xui_text_layout.c"
static xmemdebugsnapshot optionPrefix,optionHeap;
static int optionPrefixIndex,optionAllowance;
void fixtureOptionBudget(int n){if(n>=0)optionAllowance=n;fixtureLayoutBudget(n);}
void fixtureOptionCheckFont(xui_widget widget,xui_font font)
{assert(xuiRadioGetFont(widget)==font);}
void fixtureOptionCleanup(void)
{assert(references==1&&fonts==2&&paints==shapes&&surfaces==0&&draws==0);}
void fixtureOptionPrefixBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&optionPrefix);++optionPrefixIndex;}
void fixtureOptionPrefixEnd(void){xrtMemDebugFailClear();xrtClearError();}
void fixtureOptionHeapEqual(void)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 if(after.LiveCount!=optionPrefix.LiveCount||after.LiveBytes!=optionPrefix.LiveBytes||after.InvalidFreeCount!=optionPrefix.InvalidFreeCount||after.DoubleFreeCount!=optionPrefix.DoubleFreeCount){
  printf("Option prefix=%d budget=%d live=%llu/%llu bytes=%llu/%llu invalid=%llu/%llu double=%llu/%llu\n",optionPrefixIndex,optionAllowance,(unsigned long long)optionPrefix.LiveCount,(unsigned long long)after.LiveCount,(unsigned long long)optionPrefix.LiveBytes,(unsigned long long)after.LiveBytes,(unsigned long long)optionPrefix.InvalidFreeCount,(unsigned long long)after.InvalidFreeCount,(unsigned long long)optionPrefix.DoubleFreeCount,(unsigned long long)after.DoubleFreeCount);fflush(stdout);
 }
 assert(after.LiveCount==optionPrefix.LiveCount&&after.LiveBytes==optionPrefix.LiveBytes&&after.InvalidFreeCount==optionPrefix.InvalidFreeCount&&after.DoubleFreeCount==optionPrefix.DoubleFreeCount);
}
void fixtureOptionHeapBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&optionHeap);}
void fixtureOptionHeapReport(int kind)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 printf("Option heap kind=%d alloc=%llu free=%llu realloc=%llu\n",kind,
  (unsigned long long)(after.AllocCount-optionHeap.AllocCount),(unsigned long long)(after.FreeCount-optionHeap.FreeCount),(unsigned long long)(after.ReallocCount-optionHeap.ReallocCount));
}
void fixtureOptionNativeCreate(xui_widget group)
{
 const xui_radio_desc_t desc={.iSize=sizeof(desc),.sText="temporary"};
 for(int i=0;i<100;++i){xui_widget radio=NULL;assert(xuiRadioGroupAddOption(group,&radio,&desc)==XUI_OK&&radio);assert(xuiWidgetDestroyChecked(radio)==XUI_OK);}
}
void fixtureOptionNativeQueries(xui_widget group,xui_widget radio)
{
 for(int i=0;i<1000;++i){assert(xuiWidgetGetChildCount(group)==1);assert(xuiRadioGetIndicatorSize(radio)==19.5f);assert(xuiRadioGetGap(radio)==7.25f);assert(xuiRadioGetUseBuiltinAtlas(radio)==1);}
}
