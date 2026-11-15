#include "foreign_xui_offset_queries.h"
/* Reuse the existing owned CPU font backend, not a production adapter. */
#include "foreign_xui_input_extended.c"
#include <stdio.h>

void fixtureOffsetPrepare(xui_widget editor)
{
    xui_context ui=xuiWidgetGetContext(editor);xui_widget root=NULL;
    assert(xuiWidgetCreate(ui,&root)==XUI_OK&&xuiInputViewport(ui,200,120)==XUI_OK);
    assert(xuiWidgetSetRect(root,(xui_rect_t){0,0,200,120})==XUI_OK);
    assert(xuiWidgetSetRect(editor,(xui_rect_t){20,30,160,80})==XUI_OK);
    assert(xuiWidgetAddChild(root,editor)==XUI_OK&&xuiSetRootWidget(ui,root)==XUI_OK&&xuiLayout(ui)==XUI_OK);
}
int fixtureOffsetAlias(xui_widget editor,int offset,const xui_rect_t* language)
{
    xui_rect_t direct={0},alias={0};
    int a=xuiCodeEditGetOffsetRect(editor,offset,&direct);
    int b=xuiCodeEditGetTextOffsetRect(editor,offset,&alias);
    if(a!=b)return XUI_ERROR_INVALID_STATE;
    if(a!=XUI_OK)return a;
    return memcmp(&direct,&alias,sizeof(alias))==0&&memcmp(&direct,language,sizeof(direct))==0?XUI_OK:XUI_ERROR_INVALID_STATE;
}
static xmemdebugsnapshot offsetBefore;
void fixtureOffsetBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&offsetBefore);}
void fixtureOffsetReport(int native)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    printf("Offset queries heap %s: alloc=%llu free=%llu realloc=%llu; rect=%zu\n",native?"native":"language",
        (unsigned long long)(after.AllocCount-offsetBefore.AllocCount),(unsigned long long)(after.FreeCount-offsetBefore.FreeCount),
        (unsigned long long)(after.ReallocCount-offsetBefore.ReallocCount),sizeof(xui_rect_t));
}
void fixtureOffsetNativeHot(xui_widget editor)
{
    for(int i=0;i<1000;++i){
        xui_rect_t first={0},next={0};
        assert(xuiCodeEditGetOffsetRect(editor,0,&first)==XUI_OK&&xuiCodeEditGetOffsetRect(editor,1,&next)==XUI_OK);
        assert(next.fX>first.fX&&next.fY==first.fY&&first.fW==2);
    }
}
