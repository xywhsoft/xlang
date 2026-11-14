#include "foreign_xui_code_replace.h"
#include <assert.h>
#include <stdio.h>
static xmemdebugsnapshot prefix,hot;
void fixtureSearchBudget(int value)
{if(value<0)xrtMemDebugFailClear();else{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(value));}}
void fixtureSearchPrefixBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&prefix);}
void fixtureSearchPrefixEnd(void){xrtMemDebugFailClear();xrtClearError();}
void fixtureSearchHeapEqual(void)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 if(after.LiveCount!=prefix.LiveCount||after.LiveBytes!=prefix.LiveBytes)
  fprintf(stderr,"Search live memory changed: %zu/%zu -> %zu/%zu\n",prefix.LiveCount,prefix.LiveBytes,after.LiveCount,after.LiveBytes);
 assert(after.LiveCount==prefix.LiveCount&&after.LiveBytes==prefix.LiveBytes&&after.InvalidFreeCount==prefix.InvalidFreeCount&&after.DoubleFreeCount==prefix.DoubleFreeCount);
}
void fixtureSearchBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&hot);}
void fixtureSearchReport(int kind)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 printf("CodeReplace heap kind=%d alloc=%llu free=%llu realloc=%llu\n",kind,
  (unsigned long long)(after.AllocCount-hot.AllocCount),(unsigned long long)(after.FreeCount-hot.FreeCount),
  (unsigned long long)(after.ReallocCount-hot.ReallocCount));
}
void fixtureSearchNativeHot(xui_code_document document,int regex)
{
 for(int i=0;i<(regex ? 100:1000);++i){
  int count=-1,status=regex ? xuiCodeSearchReplaceAllRegex(document,"absent","x",0,&count,NULL,0):
   xuiCodeSearchReplaceAllPlain(document,"absent","x",0,&count);
  assert(status==XUI_OK&&count==0);
 }
}
