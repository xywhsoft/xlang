#include "foreign_xui_code_folding.h"
#include <assert.h>
#include <stdio.h>
static xmemdebugsnapshot prefix,hot;
static const xui_code_fold_range_t ranges[2]={{0,0,2,0,0},{0,4,6,0,0}};
void fixtureFoldBudget(int value)
{if(value<0)xrtMemDebugFailClear();else{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(value));}}
void fixtureFoldPrefixBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&prefix);}
void fixtureFoldPrefixEnd(void){xrtMemDebugFailClear();xrtClearError();}
void fixtureFoldHeapEqual(void)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 if(after.LiveCount!=prefix.LiveCount||after.LiveBytes!=prefix.LiveBytes)
  fprintf(stderr,"Fold live memory changed: %zu/%zu -> %zu/%zu\n",prefix.LiveCount,prefix.LiveBytes,after.LiveCount,after.LiveBytes);
 assert(after.LiveCount==prefix.LiveCount&&after.LiveBytes==prefix.LiveBytes&&
  after.InvalidFreeCount==prefix.InvalidFreeCount&&after.DoubleFreeCount==prefix.DoubleFreeCount);
}
void fixtureFoldHotBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&hot);}
void fixtureFoldHotReport(int kind)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 printf("CodeFold heap kind=%d alloc=%llu free=%llu realloc=%llu\n",kind,
  (unsigned long long)(after.AllocCount-hot.AllocCount),(unsigned long long)(after.FreeCount-hot.FreeCount),
  (unsigned long long)(after.ReallocCount-hot.ReallocCount));
}
void fixtureFoldNativeHot(xui_code_fold_state state)
{
 for(int i=0;i<1000;++i){
  xui_code_fold_range_t range;int out;
  assert(xuiCodeFoldStateGetCount(state)==2);
  assert(xuiCodeFoldStateGetRange(state,0,&range)==XUI_OK&&range.iEndLine==2);
  assert(xuiCodeFoldStateIsLineVisible(state,1,&out)==XUI_OK&&out==0);
  assert(xuiCodeFoldStateGetVisibleLineCount(state,7,&out)==XUI_OK&&out==3);
  assert(xuiCodeFoldStateLineToVisibleRow(state,4,&out)==XUI_OK&&out==2);
  assert(xuiCodeFoldStateVisibleRowToLine(state,7,1,&out)==XUI_OK&&out==3);
 }
}
void fixtureFoldNativeCreate(void)
{xui_code_fold_state state=NULL;assert(xuiCodeFoldStateCreate(&state)==XUI_OK);xuiCodeFoldStateDestroy(state);}
void fixtureFoldNativeSet(xui_code_fold_state state)
{assert(xuiCodeFoldStateSetRanges(state,ranges,2)==XUI_OK);}
int fixtureFoldDuringCall(xui_code_fold_state state,void (*callback)(void*),void* user)
{callback(user);return xuiCodeFoldStateGetCount(state)==2 ? XUI_OK:XUI_ERROR_INVALID_STATE;}
