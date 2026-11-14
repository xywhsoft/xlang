#include "foreign_xui_code_models.h"
#include <assert.h>
#include <stdio.h>
static xmemdebugsnapshot before;
void fixtureCodeModelsBudget(int value)
{if(value<0)xrtMemDebugFailClear();else{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(value));}}
void fixtureCodeModelsBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
void fixtureCodeModelsReport(int native)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    printf("CodeModels heap %s: alloc=%llu free=%llu realloc=%llu\n",native?"native":"language",
        (unsigned long long)(after.AllocCount-before.AllocCount),(unsigned long long)(after.FreeCount-before.FreeCount),
        (unsigned long long)(after.ReallocCount-before.ReallocCount));
}
void fixtureCodeModelsHot(xui_code_selection_model selection,xui_code_document document,xui_code_theme theme)
{
    for(int i=0;i<1000;++i){
        int start=-1,end=-1;xui_code_selection_t state={0};xui_code_style_t style={0};
        assert(xuiCodeSelectionSetRange(selection,document,0,3)==XUI_OK);
        assert(xuiCodeSelectionGetRange(selection,&start,&end)==XUI_OK && start==0 && end==3);
        assert(xuiCodeSelectionGetState(selection,&state)==XUI_OK && state.iCaretOffset==3);
        assert(xuiCodeThemeGetStyle(theme,XUI_CODE_STYLE_KEYWORD,&style)==XUI_OK);
        assert(xuiCodeThemeSetStyle(theme,XUI_CODE_STYLE_KEYWORD,&style)==XUI_OK);
    }
}
uint32_t fixtureCodeModelsEditorStyle(xui_widget editor,int styleId)
{xui_code_style_t style={0};assert(xuiCodeThemeGetStyle(xuiCodeEditGetTheme(editor),styleId,&style)==XUI_OK);return style.iForeground;}
