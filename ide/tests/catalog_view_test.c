#include "app/xide_app.h"
#include "ui/catalog_view.h"
#include "ui/editor_view.h"
#include "lang/languages.h"
#include "theme/theme.h"
#include <stdio.h>
#include <string.h>
#define CHECK(expr) do { if(!(expr)) { fprintf(stderr,"catalog_view_test:%d: %s; %s\n",__LINE__,#expr,error.message); failed=1; goto cleanup; } } while(0)
static int settle(xide_app* app)
{
    if(xuiDispatchPendingEvents(app->context)!=XUI_OK) return 0;
    xideEditorPoll(app); xideWorkbenchLayout(app); if(xuiLayout(app->context)!=XUI_OK) return 0; xideCatalogViewLayout(app); if(xuiLayout(app->context)!=XUI_OK) return 0; xideCatalogViewAfterLayout(app); return xuiLayout(app->context)==XUI_OK;
}
static size_t indexOf(xide_app* app,const char* name)
{
    for(size_t i=0;i<app->catalog.count;++i) if(!strcmp(app->catalog.entries[i].name,name)) return i;
    return app->catalog.count;
}
static int contains(xui_code_completion_item_t* items,int count,const char* name)
{
    for(int i=0;i<count;++i) if(!strcmp(items[i].sLabel,name) && !strcmp(items[i].sInsertText,name)) return 1;
    return 0;
}
static int probe(xui_widget editor,const char* text,xui_code_completion_item_t* items,int capacity,int* count)
{
    if(xuiCodeEditSetText(editor,text)!=XUI_OK || xideRefreshEditorSyntax(editor)!=XUI_OK) return 0;
    xui_code_document doc=xuiCodeEditGetDocument(editor); int offset=xuiCodeDocumentGetLength(doc);
    if(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(editor),doc,offset,0)!=XUI_OK) return 0;
    return xuiCodeProviderRequestCompletion(xuiCodeEditGetProviders(editor),editor,offset,"",items,capacity,count)==XUI_OK;
}
static int key(xide_app* app,int key,uint32_t mods)
{
    return xuiInputKeyDownEx(app->context,key,mods,NULL)==XUI_OK && xuiInputKeyUpEx(app->context,key,mods,NULL)==XUI_OK && settle(app);
}
int main(void)
{
    xide_app app={0}; int failed=0,count=0; xide_model_error error={0}; xui_code_completion_item_t items[512];
    app.hidden=1; app.viewportWidth=1280; app.viewportHeight=800;
    CHECK(xuiCreate(&app.context)==XUI_OK && ideThemeInit(app.context,IDE_THEME_CLASSIC)==XUI_OK && xuiInputViewport(app.context,1280,800)==XUI_OK);
    CHECK(xideWorkbenchCreate(&app)==XUI_OK && settle(&app)); CHECK(app.catalog.count>4300 && xideCatalogVisibleCount(&app)==app.catalog.count);
    xide_document* doc=xideDocumentNew(&app.documents,"xlang",&error); CHECK(doc && xideEditorShow(&app,doc)==XUI_OK && settle(&app));
    xui_code_document core=xuiCodeEditGetDocument(doc->editor); uint32_t version=xuiCodeDocumentGetVersion(core);
    CHECK(xuiSetFocusWidget(app.context,app.commands)==XUI_OK);
    xideWorkbenchShowHelp(&app,1); CHECK(settle(&app)); CHECK(xuiGetFocusWidget(app.context)==app.commands && xuiCodeDocumentGetVersion(core)==version && !doc->dirty);
    CHECK(xideCatalogViewSearch(&app,"math sqrt float")==XUI_OK && xideCatalogVisibleCount(&app)>0 && xideCatalogVisibleCount(&app)<10);
    CHECK(xuiEditSetText(xideCatalogSearchWidget(&app),"not_a_real_command_963")==XUI_OK && settle(&app)); CHECK(xideCatalogVisibleCount(&app)==0 && xuiCodeDocumentGetVersion(core)==version);
    CHECK(xuiEditSetText(xideCatalogSearchWidget(&app),"")==XUI_OK && settle(&app) && xideCatalogVisibleCount(&app)==app.catalog.count);
    xideWorkbenchShowHelp(&app,1); CHECK(xideCatalogViewInsertExample(&app) && settle(&app)); CHECK(doc->dirty && strstr(xuiCodeEditGetText(doc->editor),"class Counter"));
    CHECK(key(&app,'Z',XUI_MOD_CTRL) && !strcmp(xuiCodeEditGetText(doc->editor),"") && !doc->dirty);
    CHECK(key(&app,'Y',XUI_MOD_CTRL) && strstr(xuiCodeEditGetText(doc->editor),"class Counter"));
    CHECK(probe(doc->editor,"math.sq",items,512,&count) && count>10 && contains(items,count,"sqrt"));
    CHECK(xuiSetFocusWidget(app.context,doc->editor)==XUI_OK && settle(&app)); CHECK(xuiCodeEditShowCompletion(doc->editor)==XUI_OK && settle(&app) && xuiCodeEditIsCompletionOpen(doc->editor));
    CHECK(xuiCodeEditGetCompletionCount(doc->editor)==1 && key(&app,XUI_KEY_ENTER,0)); CHECK(!strcmp(xuiCodeEditGetText(doc->editor),"math.sqrt"));
    CHECK(key(&app,'Z',XUI_MOD_CTRL) && !strcmp(xuiCodeEditGetText(doc->editor),"math.sq"));
    CHECK(probe(doc->editor,"// math.sq",items,512,&count) && count==0);
    CHECK(probe(doc->editor,"/* math.sq",items,512,&count) && count==0);
    CHECK(probe(doc->editor,"\"math.sq",items,512,&count) && count==0);
    CHECK(probe(doc->editor,"int 变量 = math.sq",items,512,&count) && count>10 && contains(items,count,"sqrt"));
    CHECK(probe(doc->editor,"math",items,512,&count) && xuiSetFocusWidget(app.context,doc->editor)==XUI_OK && settle(&app));
    CHECK(xuiInputSetModifiers(app.context,0)==XUI_OK);
    CHECK(xuiInputTextEx(app.context,'.',NULL)==XUI_OK && settle(&app) && xuiCodeEditIsCompletionOpen(doc->editor));
    CHECK(xuiCodeEditCancelCompletion(doc->editor)==XUI_OK);
    CHECK(probe(doc->editor,"math",items,512,&count) && xuiInputTextEx(app.context,'.',NULL)==XUI_OK);
    CHECK(xuiInputTextEx(app.context,'s',NULL)==XUI_OK && settle(&app)); CHECK(!xuiCodeEditIsCompletionOpen(doc->editor));
    CHECK(probe(doc->editor,"math.",items,512,&count) && count>10); for(int i=0;i<count;++i) CHECK(!strchr(items[i].sInsertText,'.'));
    CHECK(probe(doc->editor,"math",items,1,&count) && count==1);
    CHECK(probe(doc->editor,"",NULL,0,&count) && count==0);
    CHECK(probe(doc->editor,"",items,512,&count) && count>100 && count<=512);
    CHECK(xideDocumentSetLanguage(doc,"c",&error)); CHECK(probe(doc->editor,"math.sq",items,512,&count) && count==0);
    CHECK(probe(doc->editor,"ret",items,512,&count) && count>=40 && contains(items,count,"return"));
    CHECK(xuiSetFocusWidget(app.context,doc->editor)==XUI_OK && settle(&app));
    /* The IDE hotkey uses explicit completion after unbinding the SDK route. */
    CHECK(xuiInputKeyDownEx(app.context,XUI_KEY_SPACE,XUI_MOD_CTRL,NULL)==XUI_OK);
    CHECK(xuiInputKeyUpEx(app.context,XUI_KEY_SPACE,XUI_MOD_CTRL,NULL)==XUI_OK && settle(&app));
    CHECK(xuiCodeEditIsCompletionOpen(doc->editor));
    xideWorkbenchShowHelp(&app,1); CHECK(!xideCatalogViewInsertExample(&app)); CHECK(!strcmp(xuiCodeEditGetText(doc->editor),"ret"));
    CHECK(xuiCodeEditCancelCompletion(doc->editor)==XUI_OK); CHECK(xideDocumentSetLanguage(doc,"xlang",&error));
    size_t split=indexOf(&app,"string.split"); CHECK(split<app.catalog.count); xideWorkbenchShowHelp(&app,(int)split); CHECK(settle(&app));
    CHECK(strstr(xuiLabelGetText(app.helpParameters),"参数") && xuiWidgetGetLayoutType(app.helpContent)==XUI_LAYOUT_COLUMN);
    app.viewportWidth=900; app.viewportHeight=600; CHECK(xuiInputViewport(app.context,900,600)==XUI_OK && settle(&app));
    CHECK(strstr(xuiLabelGetText(app.helpParameters),"默认 -1") && xuiCodeEditIsReadonly(app.helpExample));
    xideWorkbenchShowHelp(&app,(int)indexOf(&app,"log.printf")); CHECK(settle(&app) && strstr(xuiLabelGetText(app.helpParameters),"c_vararg · 变参"));
    size_t method=indexOf(&app,"csv.Reader.read"); if(method<app.catalog.count) CHECK(!xideCatalogCanComplete(&app.catalog.entries[method],"xlang"));
    CHECK(xideEditorClose(&app,doc,1,&error) && settle(&app)); CHECK(xuiCodeProviderRequestCompletion(xuiCodeEditGetProviders(doc->editor),doc->editor,0,"",items,512,&count)==XUI_OK && count==0);
    xide_document* reopened=xideDocumentNew(&app.documents,"c",&error); CHECK(reopened==doc && xideEditorShow(&app,reopened)==XUI_OK && settle(&app)); CHECK(probe(reopened->editor,"ret",items,512,&count) && count>=40 && contains(items,count,"return"));
    puts("catalog_view_test passed: native search/help, focus and document isolation, atomic example/qualified completion undo, Ctrl+Space, C/xlang separation, strings/comments, UTF-8 and closed-slot reuse");
cleanup:
    xideAppDestroy(&app); return failed;
}
