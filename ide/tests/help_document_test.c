#include "ui/catalog_view.h"
#include "ui/editor_view.h"
#include "lang/languages.h"
#include "ui/help_document.h"
#include "theme/theme.h"
#include "platform/path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(expr) do { if(!(expr)) { fprintf(stderr,"help_document_test:%d: %s; %s\n",__LINE__,#expr,error.message); failed=1; goto cleanup; } } while(0)
static int settle(xide_app* app)
{
    if(xuiDispatchPendingEvents(app->context)!=XUI_OK) return 0;
    xideEditorPoll(app); xideWorkbenchLayout(app); if(xuiLayout(app->context)!=XUI_OK) return 0;
    xideCatalogViewLayout(app); if(xuiLayout(app->context)!=XUI_OK) return 0; xideCatalogViewAfterLayout(app);
    return xuiLayout(app->context)==XUI_OK;
}
static int click(xide_app* app,xui_widget widget)
{
    xui_rect_t rect=xuiWidgetGetWorldRect(widget); float x=rect.fX+rect.fW/2,y=rect.fY+rect.fH/2;
    return xuiInputPointerMove(app->context,x,y,0)==XUI_OK && xuiInputPointerDown(app->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK &&
        xuiInputPointerUp(app->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(app);
}
static int choose(xide_app* app,xui_widget combo,int index)
{
    if(xuiComboBoxOpen(combo)!=XUI_OK || xuiLayout(app->context)!=XUI_OK) return 0;
    xui_widget menu=xuiComboBoxGetMenuWidget(combo); xui_rect_t item=xuiMenuGetItemRect(menu,index),rect=xuiWidgetGetWorldRect(menu);
    float x=rect.fX+item.fX+item.fW/2,y=rect.fY+item.fY+item.fH/2;
    return xuiInputPointerMove(app->context,x,y,0)==XUI_OK && xuiInputPointerDown(app->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK &&
        xuiInputPointerUp(app->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(app) && xuiComboBoxGetSelected(combo)==index;
}
static int init(xide_app* app)
{
    app->hidden=1; app->viewportWidth=1280; app->viewportHeight=800;
    return xuiCreate(&app->context)==XUI_OK && ideThemeInit(app->context,IDE_THEME_CLASSIC)==XUI_OK && xuiInputViewport(app->context,1280,800)==XUI_OK && xideWorkbenchCreate(app)==XUI_OK && settle(app);
}
int main(int argc,char** argv)
{
    xide_app app={0}; xide_model_error error={0}; int failed=0; char *root=NULL,*path=NULL,*markdown=NULL,*missing=NULL;
    CHECK(argc==3); root=xidePathResolve(argv[1],"中文 帮助恢复"); CHECK(root && xrtDirCreateAll(root)); path=xidePathResolve(root,"commands.json"); CHECK(path);
    const char* invalid="{\"format\":\"damaged\"}";
    CHECK(xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)invalid,strlen(invalid)}));
    app.catalogPath=path; CHECK(init(&app)); CHECK(app.catalog.count==0 && strstr(xuiLabelGetText(app.helpTitle),"不可用"));
    CHECK(xuiWidgetGetVisible(xideCatalogStatusWidget(&app)) && !xuiWidgetGetVisible(app.helpExample));
    xide_document* doc=xideDocumentNew(&app.documents,"xlang",&error); CHECK(doc && xideEditorShow(&app,doc)==XUI_OK && settle(&app));
    CHECK(xuiCodeEditSetText(doc->editor,"保留的编辑内容") == XUI_OK && settle(&app));
    uint32_t version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(doc->editor));
    CHECK(click(&app,xideCatalogReloadWidget(&app)) && app.catalog.count==0 && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(doc->editor))==version);
    size_t bytes=0; unsigned char* data=xrtFileReadAllLimit(argv[2],64u*1024u*1024u,&bytes); CHECK(data);
    int saved=xrtFileWriteAtomic(path,(xbytesview){data,bytes}); xrtFree(data); CHECK(saved);
    CHECK(click(&app,xideCatalogReloadWidget(&app)) && app.catalog.count>4400 && !xuiWidgetGetVisible(xideCatalogStatusWidget(&app)));
    CHECK(!strcmp(xuiCodeEditGetText(doc->editor),"保留的编辑内容") && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(doc->editor))==version);
    CHECK(xideCatalogVisibleCount(&app)==app.catalog.count && xideHelpChapterCount(xideCatalogHelpDocument(&app))>10);
    const xide_command* entries=app.catalog.entries; size_t count=app.catalog.count;
    CHECK(xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)invalid,strlen(invalid)}));
    CHECK(click(&app,xideCatalogReloadWidget(&app)) && app.catalog.entries==entries && app.catalog.count==count);
    CHECK(strstr(xuiLabelGetText(xideCatalogStatusWidget(&app)),"上一份有效目录") && xideHelpChapterCount(xideCatalogHelpDocument(&app))>10);
    CHECK(xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(doc->editor))==version);
    const xide_command* sqrt=NULL; for(size_t i=0;i<app.catalog.count;++i) if(!strcmp(app.catalog.entries[i].name,"math.sqrt")) { sqrt=&app.catalog.entries[i]; break; }
    CHECK(sqrt && xideCatalogViewSearch(&app,"math sqrt")==XUI_OK); xideWorkbenchShowHelp(&app,(int)(sqrt-app.catalog.entries)); CHECK(settle(&app));
    CHECK(xuiCodeEditSetText(doc->editor,"math.sq")==XUI_OK && xideRefreshEditorSyntax(doc->editor)==XUI_OK && xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(doc->editor),xuiCodeEditGetDocument(doc->editor),7,0)==XUI_OK);
    CHECK(xuiSetFocusWidget(app.context,doc->editor)==XUI_OK && settle(&app) && xuiCodeEditShowCompletion(doc->editor)==XUI_OK && settle(&app) && xuiCodeEditIsCompletionOpen(doc->editor));
    version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(doc->editor));
    data=xrtFileReadAllLimit(argv[2],64u*1024u*1024u,&bytes); CHECK(data); saved=xrtFileWriteAtomic(path,(xbytesview){data,bytes}); xrtFree(data); CHECK(saved);
    CHECK(xideCatalogViewReload(&app) && settle(&app) && !xuiCodeEditIsCompletionOpen(doc->editor));
    CHECK(!strcmp(xuiEditGetText(xideCatalogSearchWidget(&app)),"math sqrt") && !strcmp(xuiLabelGetText(app.helpTitle),"math.sqrt"));
    CHECK(xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(doc->editor))==version && !strcmp(xuiCodeEditGetText(doc->editor),"math.sq"));
    xide_help_view* help=xideCatalogHelpDocument(&app);
    markdown=calloc(24000,1); CHECK(markdown);
    strcpy(markdown,"# 总览\n\n正文包含 **加粗文字** 和行内代码。\n\n- 第一项\n- 第二项\n\n1. 有序第一项\n2. 有序第二项\n\n```c\nint main(void) { return 0; }\n```\n\n```xlang\nfunc main(...) as int\n    return 0\nend\n```\n\n| 名称 | 说明 |\n| --- | --- |\n| 中文值 | 完整的表格文字 |\n\n");
    for(int i=0;i<70;++i) { char chapter[220]; snprintf(chapter,sizeof(chapter),"## 长章节 %d\n\n这是完整的长段落，用于检查所有章节都可访问且不修改编辑文件。\n\n",i); strcat(markdown,chapter); }
    xide_help_document document={"test:long","测试长文档",markdown,"test/help.md",1}; const xide_help_document* refs[]={&document};
    CHECK(xideHelpSet(help,refs,1)==XUI_OK && settle(&app)); CHECK(xideHelpChapterCount(help)==72 && xuiWidgetGetVisible(xideHelpPages(help)));
    CHECK(xuiWidgetGetLayoutType(xideHelpContent(help))==XUI_LAYOUT_COLUMN);
    int codeCount=0,bullet=0,table=0;
    for(xui_widget child=xuiWidgetGetFirstChild(xideHelpContent(help));child;child=xuiWidgetGetNextSibling(child)) {
        if(xuiWidgetGetType(child)==xuiLabelGetType(app.context)) {
            const char* text=xuiLabelGetText(child); if(strstr(text,"• 第一项")) bullet=1; if(strstr(text,"名称：中文值") && strstr(text,"说明：完整的表格文字")) table=1;
            CHECK(!strstr(text,"**加粗文字**"));
        }
    }
    codeCount=(int)xideHelpCodeCount(help);
    for(int i=0;i<codeCount;++i) { xui_widget code=xideHelpCode(help,(size_t)i); CHECK(xuiCodeEditIsReadonly(code)); CHECK(!strcmp(xuiCodeEditGetLanguage(code),i==0 ? "c" : "xlang")); }
    CHECK(codeCount==2 && bullet && table);
    xui_widget first=xuiWidgetGetFirstChild(xideHelpContent(help)); CHECK(xideHelpSet(help,refs,1)==XUI_OK && first==xuiWidgetGetFirstChild(xideHelpContent(help)));
    CHECK(choose(&app,xideHelpPages(help),1) && choose(&app,xideHelpNavigator(help),5)); CHECK(strstr(xideHelpChapterTitle(help,69),"长章节 67"));
    float offset=0; CHECK(xuiScrollViewGetOffset(app.help,NULL,&offset)==XUI_OK && offset>1000);
    xui_rect_t navigator=xuiWidgetGetWorldRect(xideHelpNavigator(help)),scroll=xuiWidgetGetWorldRect(app.help);
    CHECK(navigator.fY+navigator.fH<=scroll.fY && xuiWidgetGetVisible(xideHelpNavigator(help)));
    CHECK(xuiSetFocusWidget(app.context,app.commands)==XUI_OK && xideHelpJump(help,3) && settle(&app));
    CHECK(xuiGetFocusWidget(app.context)==app.commands && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(doc->editor))==version);
    CHECK(!xideHelpJump(help,72));
    CHECK(xideWorkbenchApplyTheme(&app,IDE_THEME_GRAPHITE)==XUI_OK);
    app.viewportWidth=900; app.viewportHeight=600; CHECK(xuiInputViewport(app.context,900,600)==XUI_OK && settle(&app)); CHECK(xuiWidgetGetChildCount(xideHelpContent(help))>70);
    CHECK(xideHelpSet(help,NULL,0)==XUI_OK && settle(&app) && xideHelpChapterCount(help)==0 && !xuiWidgetGetVisible(xideHelpNavigator(help)));
    xideAppDestroy(&app); memset(&app,0,sizeof(app)); missing=xidePathResolve(root,"missing/commands.json"); CHECK(missing); app.catalogPath=missing; CHECK(init(&app) && app.catalog.count==0);
    CHECK(strstr(xuiLabelGetText(xideCatalogStatusWidget(&app)),"Cannot read") && !xuiWidgetGetVisible(app.helpExample));
    CHECK(xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)invalid,strlen(invalid)}));
    puts("help_document_test passed: native startup recovery/reload preservation, shared Markdown blocks, C/xlang read-only fences, vertical tables/lists, 72 chapter navigation with real menus and edit/focus isolation");
cleanup:
    xideAppDestroy(&app); free(root); free(path); free(markdown); free(missing); return failed;
}
