#include "foreign_xui_edit_search.h"
#include <assert.h>
#include <stdio.h>
static xmemdebugsnapshot before;
void fixtureEditSearchHeapBegin(void)
{assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
void fixtureEditSearchHeapReport(int kind)
{
    xmemdebugsnapshot now;xrtMemDebugSnapshot(&now);
    printf("Edit search heap %s: alloc=%llu free=%llu realloc=%llu\n",kind==2?"native":kind==1?"language":"literal",
        (unsigned long long)(now.AllocCount-before.AllocCount),
        (unsigned long long)(now.FreeCount-before.FreeCount),
        (unsigned long long)(now.ReallocCount-before.ReallocCount));
}
void fixtureEditSearchNativePerformance(xui_widget editor)
{
    const xui_find_options_t options={sizeof(options),"absent",NULL,0,-1,0,-1,0};
    for(int i=0;i<1000;++i){int count=-1;assert(xuiCodeEditFindAll(editor,&options,&count)==XUI_OK && count==0);}
}
int fixtureEditSearchSelection(xui_widget editor,int start,int end)
{
    int a=-1,b=-1;
    return xuiCodeSelectionGetRange(xuiCodeEditGetSelection(editor),&a,&b)==XUI_OK && a==start && b==end;
}
