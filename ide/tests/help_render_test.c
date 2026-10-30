#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/catalog_view.h"
#include "ui/editor_view.h"
#include "ui/help_document.h"
#include "platform/path.h"
#include "services/ui_font.h"
#include "theme/theme.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { ++f->checks; if(!(expr)) { fprintf(stderr,"help_render_test:%d: %s\n",__LINE__,#expr); f->failed=1; goto cleanup; } } while(0)
typedef struct fixture {
    xide_app app;
    HDESK desktop;
    const char* directory;
    char *settings,*markdown,*font;
    xide_document *c,*xl;
    uint32_t cVersion,xlVersion;
    xui_code_selection_t cSelection,xlSelection;
    size_t initialOffscreenBytes;
    int checks,failed,started,baseline;
} fixture;
static const char cText[]="/* 未保存 C 中文 😀 */\nint native_answer(void) { return 42; }\n";
static const char xlText[]="// 未保存 xlang 中文 😀\nfunc native_answer(...) as int\n    return 43\nend\n";
static int intersection(xui_rect_t a,xui_rect_t b)
{
    return a.fW>0 && a.fH>0 && b.fW>0 && b.fH>0 && (int64_t)a.fX<(int64_t)b.fX+b.fW && (int64_t)a.fX+a.fW>b.fX && (int64_t)a.fY<(int64_t)b.fY+b.fH && (int64_t)a.fY+a.fH>b.fY;
}
static xui_rect_t viewport(xide_app* app)
{
    return xuiWidgetGetWorldRect(xuiScrollViewGetViewportWidget(app->help));
}
static int frame(xide_app* app)
{
    return xideAppFrame(app)==XGE_OK && !app->frameError;
}
static int preserve(fixture* f)
{
    xui_code_selection_t c={0},xl={0};
    xuiCodeSelectionGetState(xuiCodeEditGetSelection(f->c->editor),&c);
    xuiCodeSelectionGetState(xuiCodeEditGetSelection(f->xl->editor),&xl);
    return !strcmp(xuiCodeEditGetText(f->c->editor),cText) && !strcmp(xuiCodeEditGetText(f->xl->editor),xlText) &&
        xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(f->c->editor))==f->cVersion && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(f->xl->editor))==f->xlVersion &&
        c.iCaretOffset==f->cSelection.iCaretOffset && c.iAnchorOffset==f->cSelection.iAnchorOffset &&
        xl.iCaretOffset==f->xlSelection.iCaretOffset && xl.iAnchorOffset==f->xlSelection.iAnchorOffset &&
        !strcmp(xuiCodeEditGetLanguage(f->c->editor),"c") && !strcmp(xuiCodeEditGetLanguage(f->xl->editor),"xlang");
}
static size_t cacheBytes(xide_app* app,xui_widget widget)
{
    size_t total=0;
    for(int i=0;i<xuiWidgetGetCacheStateCount(widget);++i) {
        xui_surface surface=xuiWidgetGetCacheSurface(widget,xuiWidgetGetCacheStateId(widget,i));xui_surface_desc_t desc={0};
        if(surface && app->proxy.surfaceGetDesc(&app->proxy,surface,&desc)==XUI_OK) total+=(size_t)desc.iWidth*desc.iHeight*4;
    }
    return total;
}
static int bounded(fixture* f,const char* stage)
{
    xide_app* app=&f->app;xui_rect_t clip=viewport(app);int codes=0,cached=0,outside=0;size_t bytes=0,offscreen=0;
    xide_help_view* view=xideCatalogHelpDocument(app);
    for(size_t i=0;i<xideHelpCodeCount(view);++i) {
        xui_widget child=xideHelpCode(view,i);
        ++codes;size_t n=cacheBytes(app,child);bytes+=n;if(n)++cached;
        if(!xuiWidgetGetEffectiveVisible(child) || !intersection(xuiWidgetGetWorldRect(child),clip)) {offscreen+=n;if(n)++outside;}
    }
    printf("Help cache stage=%s blocks=%d cached=%d offscreen=%d logicalBytes=%llu offscreenBytes=%llu\n",stage,codes,cached,outside,(unsigned long long)bytes,(unsigned long long)offscreen);fflush(stdout);
    if(!strcmp(stage,"default"))f->initialOffscreenBytes=offscreen;
    size_t labelBytes=0,labelOffscreen=0;int labels=0,labelCached=0;
    for(xui_widget child=xuiWidgetGetFirstChild(xideHelpContent(view));child;child=xuiWidgetGetNextSibling(child)) {
        if(xuiWidgetGetType(child)!=xuiLabelGetType(app->context))continue;
        ++labels;size_t n=cacheBytes(app,child);labelBytes+=n;if(n)++labelCached;
        if(!xuiWidgetGetEffectiveVisible(child) || !intersection(xuiWidgetGetWorldRect(child),clip))labelOffscreen+=n;
    }
    printf("Help text stage=%s labels=%d cached=%d logicalBytes=%llu offscreenBytes=%llu\n",stage,labels,labelCached,(unsigned long long)labelBytes,(unsigned long long)labelOffscreen);fflush(stdout);
    return codes>0 && (f->baseline || (!offscreen && !labelOffscreen));
}
static xui_widget codeAt(xide_app* app,int index)
{
    return index<0 ? NULL : xideHelpCode(xideCatalogHelpDocument(app),(size_t)index);
}
static int reveal(xide_app* app,xui_widget code)
{
    xui_rect_t r=xuiWidgetGetWorldRect(code),v=viewport(app);float offset=0;
    return xuiScrollViewGetOffset(app->help,NULL,&offset)==XUI_OK && xuiScrollViewSetOffset(app->help,0,offset+r.fY-v.fY)==XUI_OK && frame(app) && intersection(xuiWidgetGetWorldRect(code),viewport(app));
}
/* The fixed CodeEdit paints through its bitmap callback. Check actual pixels
   so a policy change which silently blanks a visible example cannot pass. */
static int coloredRole(xide_app* app,xui_widget code,ide_color_role role)
{
    for(int i=0;i<xuiWidgetGetCacheStateCount(code);++i) {
        xui_surface surface=xuiWidgetGetCacheSurface(code,xuiWidgetGetCacheStateId(code,i));xui_surface_desc_t d={0};
        if(!surface || app->proxy.surfaceGetDesc(&app->proxy,surface,&d)!=XUI_OK)continue;
        size_t count=(size_t)d.iWidth*d.iHeight;unsigned char* pixels=malloc(count*4);if(!pixels)return 0;
        int result=app->proxy.surfaceReadRGBA(&app->proxy,surface,pixels,d.iWidth*4);size_t coloredCount=0;
        uint32_t keyword=ideThemeColor(app->theme,role);unsigned char red=(unsigned char)(keyword>>24),green=(unsigned char)(keyword>>16),blue=(unsigned char)(keyword>>8);
        /* Thin antialiased glyphs need not contain an opaque palette pixel. */
        if(result==XUI_OK)for(size_t j=0;j<count;++j)if(abs((int)pixels[j*4]-red)<=8 && abs((int)pixels[j*4+1]-green)<=8 && abs((int)pixels[j*4+2]-blue)<=8 && pixels[j*4+3])++coloredCount;
        free(pixels);if(coloredCount>3)return 1;
    }
    return 0;
}
static int colored(xide_app* app,xui_widget code) { return coloredRole(app,code,IDE_KEYWORD); }
/* Nonempty Label text and valid geometry alone cannot detect the blank-text
   regression: check glyph pixels produced by the native drawing callback. */
static int textVisible(fixture* f,const char* text,ide_color_role role)
{
    /* The explicit historical bitmap-retention mode predates body drawing. */
    if(f->baseline)return 1;
    xide_app* app=&f->app;
    for(xui_widget child=xuiWidgetGetFirstChild(xideHelpContent(xideCatalogHelpDocument(app)));child;child=xuiWidgetGetNextSibling(child)) {
        if(xuiWidgetGetType(child)==xuiLabelGetType(app->context) && !strcmp(xuiLabelGetText(child),text))
            return reveal(app,child) && coloredRole(app,child,role) && bounded(f,"visible-text") && preserve(f);
    }
    return 0;
}
static int capture(fixture* f,const char* leaf)
{
    char* path=xidePathResolve(f->directory,leaf);int okay=path && xideAppCapture(&f->app,path)==XGE_OK;free(path);return okay;
}
static int run(void* user)
{
    fixture* f=user;xide_app* app=&f->app;xide_model_error error={0};xui_widget firstC=NULL,firstXl=NULL,last=NULL;xui_code_selection_t before={0},after={0};
    f->started=1;app->hidden=1;app->noSession=1;app->maxFrames=10000;app->dpiOverride=1;
    CHECK(xrtDirCreateAll(f->directory));f->settings=xidePathResolve(f->directory,"preferences.json");CHECK(f->settings);app->settingsPath=f->settings;
    CHECK(xideAppCreate(app)==XUI_OK && frame(app));CHECK(bounded(f,"default"));CHECK(capture(f,"native-help-default.png"));
    f->c=xideDocumentNew(&app->documents,"c",&error);f->xl=xideDocumentNew(&app->documents,"xlang",&error);CHECK(f->c && f->xl);
    CHECK(xideEditorShow(app,f->c)==XUI_OK && xuiCodeDocumentInsert(xuiCodeEditGetDocument(f->c->editor),0,cText)==XUI_OK);
    CHECK(xideEditorShow(app,f->xl)==XUI_OK && xuiCodeDocumentInsert(xuiCodeEditGetDocument(f->xl->editor),0,xlText)==XUI_OK && frame(app));
    CHECK(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(f->c->editor),xuiCodeEditGetDocument(f->c->editor),5,0)==XUI_OK);
    CHECK(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(f->xl->editor),xuiCodeEditGetDocument(f->xl->editor),8,0)==XUI_OK);
    f->cVersion=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(f->c->editor));f->xlVersion=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(f->xl->editor));
    xuiCodeSelectionGetState(xuiCodeEditGetSelection(f->c->editor),&f->cSelection);xuiCodeSelectionGetState(xuiCodeEditGetSelection(f->xl->editor),&f->xlSelection);
    f->markdown=calloc(65536,1);CHECK(f->markdown);strcpy(f->markdown,"# 渲染与阅读\n\nC 与 xlang 只读示例。\n\n- 列表正文不能消失\n\n| 名称 | 说明 |\n| --- | --- |\n| 正文单元格 | 表格文字不能消失 |\n\n");
    for(int i=0;i<40;++i) {
        char block[512];snprintf(block,sizeof(block),"## 示例 %d\n\n中文长文档与屏幕外代码块。\n\n```%s\n%s```\n\n",i,i%2?"xlang":"c",i%2?xlText:cText);strcat(f->markdown,block);
    }
    xide_help_document document={"test:render","长文档渲染",f->markdown,"test/help-render.md",1};const xide_help_document* references[]={&document};
    CHECK(xideHelpSet(xideCatalogHelpDocument(app),references,1)==XUI_OK && frame(app));CHECK(bounded(f,"custom"));CHECK(preserve(f));
    CHECK(textVisible(f,"渲染与阅读",IDE_ACCENT));
    CHECK(textVisible(f,"C 与 xlang 只读示例。",IDE_TEXT));
    CHECK(textVisible(f,"• 列表正文不能消失",IDE_TEXT));
    CHECK(textVisible(f,"名称：正文单元格\n说明：表格文字不能消失",IDE_TEXT));
    CHECK(capture(f,"native-help-body.png"));
    firstC=codeAt(app,0);firstXl=codeAt(app,1);last=codeAt(app,39);CHECK(firstC && firstXl && last);
    CHECK(xuiCodeEditIsReadonly(firstC) && xuiCodeEditIsReadonly(firstXl));CHECK(!strcmp(xuiCodeEditGetLanguage(firstC),"c") && !strcmp(xuiCodeEditGetLanguage(firstXl),"xlang"));
    CHECK(reveal(app,firstC) && colored(app,firstC));CHECK(bounded(f,"visible-c") && preserve(f));CHECK(capture(f,"native-help-c.png"));
    CHECK(reveal(app,firstXl) && colored(app,firstXl));CHECK(bounded(f,"visible-xlang") && preserve(f));CHECK(capture(f,"native-help-xlang.png"));
    CHECK(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(firstXl),xuiCodeEditGetDocument(firstXl),5,0)==XUI_OK);
    xuiCodeSelectionGetState(xuiCodeEditGetSelection(firstXl),&before);
    for(int i=0;i<40;++i) {CHECK(reveal(app,codeAt(app,i)));CHECK(bounded(f,"scroll") && preserve(f));}
    CHECK(xideHelpJump(xideCatalogHelpDocument(app),xideHelpChapterCount(xideCatalogHelpDocument(app))-1) && frame(app));CHECK(bounded(f,"chapter-jump") && preserve(f));
    CHECK(reveal(app,last) && colored(app,last));CHECK(capture(f,"native-help-end.png"));
    CHECK(reveal(app,firstXl));xuiCodeSelectionGetState(xuiCodeEditGetSelection(firstXl),&after);CHECK(before.iCaretOffset==after.iCaretOffset && before.iAnchorOffset==after.iAnchorOffset);
    CHECK(xuiDockPanelHideWindow(app->dock,app->windows[XIDE_HELP])==XUI_OK && frame(app));CHECK(bounded(f,"hidden") && preserve(f));
    CHECK(xuiDockPanelDockWindowToPane(app->dock,app->windows[XIDE_HELP],app->helpPane)==XUI_OK && xuiDockPanelSetPaneActiveWindow(app->dock,app->helpPane,app->windows[XIDE_HELP])==XUI_OK && frame(app));CHECK(reveal(app,firstXl) && colored(app,firstXl));CHECK(bounded(f,"shown") && preserve(f));
    CHECK(xideWorkbenchApplyTheme(app,IDE_THEME_GRAPHITE)==XUI_OK && frame(app));CHECK(reveal(app,firstC) && colored(app,firstC));CHECK(bounded(f,"theme") && preserve(f));
    CHECK(textVisible(f,"渲染与阅读",IDE_ACCENT) && textVisible(f,"C 与 xlang 只读示例。",IDE_TEXT));
    app->dpiOverride=1.25f;CHECK(frame(app));CHECK(reveal(app,firstC) && colored(app,firstC));CHECK(bounded(f,"dpi-125") && preserve(f));CHECK(capture(f,"native-help-dpi-125.png"));
    CHECK(textVisible(f,"C 与 xlang 只读示例。",IDE_TEXT));
    app->dpiOverride=2;CHECK(frame(app));CHECK(reveal(app,firstXl) && colored(app,firstXl));CHECK(bounded(f,"dpi-200") && preserve(f));CHECK(capture(f,"native-help-dpi-200.png"));
    CHECK(textVisible(f,"• 列表正文不能消失",IDE_TEXT));
    CHECK(textVisible(f,"名称：正文单元格\n说明：表格文字不能消失",IDE_TEXT));
    app->dpiOverride=1;CHECK(frame(app));CHECK(reveal(app,firstXl) && colored(app,firstXl));CHECK(bounded(f,"dpi-restored") && preserve(f));
    f->font=xidePathResolve(NULL,app->uiFontPath);CHECK(f->font && xideUiFontApply(app,f->font,20,&error) && frame(app));
    CHECK(textVisible(f,"渲染与阅读",IDE_ACCENT) && textVisible(f,"C 与 xlang 只读示例。",IDE_TEXT));
    CHECK(textVisible(f,"名称：正文单元格\n说明：表格文字不能消失",IDE_TEXT));CHECK(capture(f,"native-help-body-font-20.png"));
    CHECK(xuiCodeDocumentUndo(xuiCodeEditGetDocument(f->xl->editor))==XUI_OK && frame(app) && !strcmp(xuiCodeEditGetText(f->xl->editor),""));
    CHECK(xuiCodeDocumentRedo(xuiCodeEditGetDocument(f->xl->editor))==XUI_OK && frame(app) && !strcmp(xuiCodeEditGetText(f->xl->editor),xlText));
    if(f->baseline)CHECK(f->initialOffscreenBytes>1024*1024);
    printf("help_render_test passed: %d native checks; default offscreen bytes=%llu; baseline=%d; visible heading/paragraph/list/table and C/xlang glyph pixels, offscreen text/code retirement, forty block scrolls, chapter jump, hidden/show, theme, 125/200%% DPI, UI font and draft selection/version/undo preservation\n",f->checks,(unsigned long long)f->initialOffscreenBytes,f->baseline);fflush(stdout);
cleanup:
    xideAppDestroy(app);xgeSetQuitRequestCallback(NULL,NULL);xgeQuit();return XGE_OK;
}
static DWORD WINAPI worker(void* user)
{
    fixture* f=user;if(!SetThreadDesktop(f->desktop))return 1;xge_desc_t d={0};d.iWidth=3840;d.iHeight=2160;d.sTitle="xlang IDE private help rendering verification";d.iFlags=XGE_INIT_WINDOW;d.iRunMode=XGE_RUN_GAME_LOOP;
    int result=xgeInit(&d);if(result==XGE_OK)result=xgeRun(run,f);xideAppDestroy(&f->app);xgeUnit();return result==XGE_OK && f->started && !f->failed?0:1;
}
int main(int argc,char** argv)
{
    if(argc!=2 && !(argc==3 && !strcmp(argv[2],"--baseline")))return 2;
    fixture f={0};f.directory=argv[1];f.baseline=argc==3;wchar_t name[100];swprintf(name,100,L"XlangIDEHelpRender-%lu",GetCurrentProcessId());
    f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);if(!f.desktop)return 1;
    HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL);DWORD code=1;
    if(thread){if(WaitForSingleObject(thread,60000)!=WAIT_OBJECT_0){fputs("help_render_test: native verification timed out\n",stderr);ExitProcess(1);}GetExitCodeThread(thread,&code);CloseHandle(thread);}
    CloseDesktop(f.desktop);free(f.settings);free(f.markdown);free(f.font);return (int)code;
}
