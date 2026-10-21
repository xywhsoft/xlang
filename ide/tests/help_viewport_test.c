#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/catalog_view.h"
#include "ui/editor_view.h"
#include "ui/help_document.h"
#include "platform/path.h"
#include "services/editor_font.h"
#include "theme/theme.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(e) do {++f->checks;if(!(e)){fprintf(stderr,"help_viewport_test:%d: %s\n",__LINE__,#e);f->failed=1;goto cleanup;}} while(0)
typedef struct fixture {xide_app app;HDESK desktop;const char* directory;char *markdown,*cText,*xlText,*settings,*font;int checks,failed,started;} fixture;
static char* clipboard;
static int setClipboard(xui_proxy proxy,const char* text) {free(clipboard);clipboard=malloc(strlen(text)+1);if(!clipboard)return XUI_ERROR_OUT_OF_MEMORY;strcpy(clipboard,text);return XUI_OK;}
static int getClipboard(xui_proxy proxy,char* text,int capacity) {int bytes=clipboard ? (int)strlen(clipboard) : 0;if(!text || capacity<=bytes)return bytes+1;memcpy(text,clipboard ? clipboard : "",bytes+1);return bytes;}
/* Compile the production app with only its initial proxy registration routed
   through a private clipboard. The SDK forbids changing a live proxy, and this
   fixture must not read or replace the developer's system clipboard. */
static int privateSetProxy(xui_context context,const xui_proxy_t* original) {xui_proxy_t proxy=*original;proxy.clipboardSetText=setClipboard;proxy.clipboardGetText=getClipboard;return xuiSetProxy(context,&proxy);}
#define xuiSetProxy privateSetProxy
#include "app/xide_app.c"
#undef xuiSetProxy
static int renderFrame(xide_app* app) {return xideAppFrame(app)==XGE_OK && !app->frameError;}
static xui_rect_t viewport(xide_app* app) {return xuiWidgetGetWorldRect(xuiScrollViewGetViewportWidget(app->help));}
static int capture(fixture* f,const char* name) {char* p=xidePathResolve(f->directory,name);int result=p && xideAppCapture(&f->app,p)==XGE_OK;free(p);return result;}
static int bounded(xide_app* app) {
    xide_help_view* view=xideCatalogHelpDocument(app);int caches=0;
    for(size_t i=0;i<xideHelpCodeCount(view);++i) {xui_widget code=xideHelpCode(view,i);
        for(int j=0;j<xuiWidgetGetCacheStateCount(code);++j) {xui_surface s=xuiWidgetGetCacheSurface(code,xuiWidgetGetCacheStateId(code,j));xui_surface_desc_t d={0};if(!s)continue;
            if(app->proxy.surfaceGetDesc(&app->proxy,s,&d)!=XUI_OK || d.iHeight>viewport(app).fH+128 || d.iWidth>viewport(app).fW+8)return 0;
            ++caches;printf("Bounded help cache %dx%d logical pixels at DPI %.2f\n",d.iWidth,d.iHeight,app->dpiScale);
        }
    }return caches==1;
}
static int colored(xide_app* app,xui_widget code,ide_color_role role) {
    for(int j=0;j<xuiWidgetGetCacheStateCount(code);++j) {xui_surface s=xuiWidgetGetCacheSurface(code,xuiWidgetGetCacheStateId(code,j));xui_surface_desc_t d={0};if(!s || app->proxy.surfaceGetDesc(&app->proxy,s,&d)!=XUI_OK)continue;
        size_t n=(size_t)d.iWidth*d.iHeight,count=0;unsigned char* pixels=malloc(n*4);if(!pixels)return 0;int result=app->proxy.surfaceReadRGBA(&app->proxy,s,pixels,d.iWidth*4);uint32_t color=ideThemeColor(app->theme,role);
        if(result==XUI_OK)for(size_t i=0;i<n;++i)if(pixels[i*4]==(unsigned char)(color>>24) && pixels[i*4+1]==(unsigned char)(color>>16) && pixels[i*4+2]==(unsigned char)(color>>8) && pixels[i*4+3])++count;
        free(pixels);if(count>3)return 1;
    }return 0;
}
static int reveal(xide_app* app,xui_widget code,int line) {
    xui_widget block=xuiWidgetGetParent(code);float top=xuiWidgetGetWorldRect(block).fY-xuiWidgetGetWorldRect(app->helpContent).fY;
    return xuiScrollViewSetOffset(app->help,0,top+line*xideEditorLineHeight(app))==XUI_OK && renderFrame(app);
}
static int key(xide_app* app,xui_widget code,int key,uint32_t modifiers) {return xuiSetFocusWidget(app->context,code)==XUI_OK && xuiInputKeyDown(app->context,key,modifiers)==XUI_OK && xuiInputKeyUp(app->context,key,modifiers)==XUI_OK && xuiDispatchPendingEvents(app->context)==XUI_OK && renderFrame(app);}
static int caretVisible(xide_app* app,xui_widget code) {
    xui_code_selection_t state={0};int line=0;xuiCodeSelectionGetState(xuiCodeEditGetSelection(code),&state);xuiCodeDocumentOffsetToLineColumn(xuiCodeEditGetDocument(code),state.iCaretOffset,&line,NULL);
    float y=xuiWidgetGetWorldRect(xuiWidgetGetParent(code)).fY+4+line*xideEditorLineHeight(app);xui_rect_t v=viewport(app);
    return y>=v.fY-1 && y+xideEditorLineHeight(app)<=v.fY+v.fH+1;
}
static int run(void* user) {
    fixture* f=user;xide_app* app=&f->app;xide_model_error error={0};xide_document *draftC=NULL,*draftXl=NULL;xui_widget c=NULL,xl=NULL;uint32_t cVersion=0,xlVersion=0;float before=0,after=0;xui_code_selection_t selection={0},retained={0};
    f->started=1;app->hidden=1;app->noSession=1;app->dpiOverride=1;app->maxFrames=10000;
    CHECK(xrtDirCreateAll(f->directory));f->settings=xidePathResolve(f->directory,"preferences.json");CHECK(f->settings);app->settingsPath=f->settings;
    CHECK(xideAppCreate(app)==XUI_OK && renderFrame(app));
    draftC=xideDocumentNew(&app->documents,"c",&error);draftXl=xideDocumentNew(&app->documents,"xlang",&error);CHECK(draftC && draftXl);
    CHECK(xideEditorShow(app,draftC)==XUI_OK && xuiCodeDocumentInsert(xuiCodeEditGetDocument(draftC->editor),0,"int draft = 17; // 未保存\n")==XUI_OK);
    CHECK(xideEditorShow(app,draftXl)==XUI_OK && xuiCodeDocumentInsert(xuiCodeEditGetDocument(draftXl->editor),0,"var draft as int = 18 // 未保存\n")==XUI_OK && renderFrame(app));
    cVersion=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(draftC->editor));xlVersion=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(draftXl->editor));
    f->cText=calloc(300000,1);f->xlText=calloc(300000,1);f->markdown=calloc(650000,1);CHECK(f->cText && f->xlText && f->markdown);
    for(int i=0;i<2500;++i) {char s[100];snprintf(s,sizeof(s),i==300 ? "/* 跨视口注释开始 %d\n" : i>300 && i<735 ? "注释 int return %d\n" : i==735 ? "*/ int value_%04d = 735;\n" : "int value_%04d = 17; // 中文 😀\n",i);strcat(f->cText,s);}
    for(int i=0;i<3200;++i) {char s[100];snprintf(s,sizeof(s),i==900 ? "/* 跨视口 xlang 注释 %d\n" : i>900 && i<1335 ? "注释 var return %d\n" : i==1335 ? "*/ var value_%04d as int = 18\n" : "var value_%04d as int = 18 // 中文 😀\n",i);strcat(f->xlText,s);}
    strcpy(f->markdown,"# 超长单块帮助\n\n## C 示例\n\n```c\n");strcat(f->markdown,f->cText);strcat(f->markdown,"```\n\n## xlang 示例\n\n```xlang\n");strcat(f->markdown,f->xlText);strcat(f->markdown,"```\n\n## 末尾说明\n\n两个完整示例之后的段落仍可阅读。\n");
    xide_help_document document={"test:viewport","完整长代码阅读",f->markdown,"test/long-code.md",1};const xide_help_document* refs[]={&document};
    xide_help_view* view=xideCatalogHelpDocument(app);CHECK(xideHelpSet(view,refs,1)==XUI_OK && renderFrame(app) && xideHelpCodeCount(view)==2);
    c=xideHelpCode(view,0);xl=xideHelpCode(view,1);CHECK(c && xl && xuiCodeEditIsReadonly(c) && xuiCodeEditIsReadonly(xl));
    CHECK(!strcmp(xuiCodeEditGetText(c),f->cText) && !strcmp(xuiCodeEditGetText(xl),f->xlText));CHECK(xuiCodeDocumentGetLineCount(xuiCodeEditGetDocument(c))==2501 && xuiCodeDocumentGetLineCount(xuiCodeEditGetDocument(xl))==3201);
    CHECK(xuiWidgetGetWorldRect(xuiWidgetGetParent(c)).fH>32768 && xuiWidgetGetWorldRect(xuiWidgetGetParent(xl)).fH>32768);
    CHECK(reveal(app,c,0) && bounded(app) && colored(app,c,IDE_TYPE));CHECK(capture(f,"native-long-c-start.png"));
    xuiScrollViewGetOffset(app->help,NULL,&before);xui_rect_t v=viewport(app);CHECK(xuiInputPointerWheel(app->context,v.fX+120,v.fY+30,0,-2,0)==XUI_OK && xuiDispatchPendingEvents(app->context)==XUI_OK && renderFrame(app));xuiScrollViewGetOffset(app->help,NULL,&after);CHECK(after>before && bounded(app));
    CHECK(reveal(app,c,315) && bounded(app) && colored(app,c,IDE_COMMENT) && !colored(app,c,IDE_TYPE));CHECK(capture(f,"native-long-c-comment.png"));
    CHECK(reveal(app,c,1200) && bounded(app) && colored(app,c,IDE_TYPE));CHECK(capture(f,"native-long-c-middle.png"));
    CHECK(key(app,c,XUI_KEY_END,XUI_MOD_CTRL) && caretVisible(app,c) && bounded(app));CHECK(capture(f,"native-long-c-key-end.png"));
    CHECK(key(app,c,XUI_KEY_HOME,XUI_MOD_CTRL) && caretVisible(app,c) && bounded(app));
    CHECK(key(app,c,'A',XUI_MOD_CTRL) && key(app,c,'C',XUI_MOD_CTRL));CHECK(clipboard && !strcmp(clipboard,f->cText));
    CHECK(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(c),xuiCodeEditGetDocument(c),500,0)==XUI_OK);xuiCodeSelectionGetState(xuiCodeEditGetSelection(c),&selection);
    for(int i=0;i<8;++i)CHECK(reveal(app,i%2 ? xl : c,i%2 ? 2800 : 2100) && bounded(app));
    xuiCodeSelectionGetState(xuiCodeEditGetSelection(c),&retained);CHECK(selection.iCaretOffset==retained.iCaretOffset && selection.iAnchorOffset==retained.iAnchorOffset);
    CHECK(reveal(app,xl,915) && bounded(app) && colored(app,xl,IDE_COMMENT) && !colored(app,xl,IDE_KEYWORD));CHECK(capture(f,"native-long-xlang-comment.png"));
    CHECK(reveal(app,xl,1800) && bounded(app) && colored(app,xl,IDE_KEYWORD));CHECK(capture(f,"native-long-xlang-middle.png"));
    CHECK(key(app,xl,XUI_KEY_END,XUI_MOD_CTRL) && caretVisible(app,xl) && bounded(app));CHECK(key(app,xl,'A',XUI_MOD_CTRL) && key(app,xl,'C',XUI_MOD_CTRL));CHECK(clipboard && !strcmp(clipboard,f->xlText));
    CHECK(xideHelpJump(view,xideHelpChapterCount(view)-1) && renderFrame(app));CHECK(capture(f,"native-long-help-after.png"));
    CHECK(xideWorkbenchApplyTheme(app,IDE_THEME_GRAPHITE)==XUI_OK && renderFrame(app));app->dpiOverride=2;CHECK(renderFrame(app) && reveal(app,xl,1800) && bounded(app) && colored(app,xl,IDE_KEYWORD));CHECK(capture(f,"native-long-xlang-dpi-200.png"));
    CHECK(reveal(app,c,2490) && bounded(app) && colored(app,c,IDE_TYPE));CHECK(capture(f,"native-long-c-dpi-200-end.png"));
    CHECK(reveal(app,xl,1800));xuiScrollViewGetOffset(app->help,NULL,&before);xvalue* reading=xideHelpCaptureReading(view,NULL);CHECK(reading);
    CHECK(reveal(app,c,0));xideHelpRestoreReading(view,reading);xrtValueRelease(reading);CHECK(renderFrame(app));xuiScrollViewGetOffset(app->help,NULL,&after);CHECK(before==after && bounded(app));
    CHECK(xuiDockPanelHideWindow(app->dock,app->windows[XIDE_HELP])==XUI_OK && renderFrame(app));CHECK(xuiWidgetGetCacheStateCount(c)==0 && xuiWidgetGetCacheStateCount(xl)==0);
    CHECK(xuiDockPanelDockWindowToPane(app->dock,app->windows[XIDE_HELP],app->helpPane)==XUI_OK && xuiDockPanelSetPaneActiveWindow(app->dock,app->helpPane,app->windows[XIDE_HELP])==XUI_OK && renderFrame(app));CHECK(reveal(app,xl,1800) && bounded(app));
    f->font=xideWindowsFontPath("consola.ttf");CHECK(f->font && xideEditorFontApply(app,f->font,24,&error) && renderFrame(app));CHECK(reveal(app,c,1200) && bounded(app) && colored(app,c,IDE_TYPE));CHECK(capture(f,"native-long-c-font-24.png"));
    CHECK(key(app,c,XUI_KEY_DELETE,0) && key(app,c,'X',XUI_MOD_CTRL));
    CHECK(!strcmp(xuiCodeEditGetText(c),f->cText) && !strcmp(xuiCodeEditGetText(xl),f->xlText));
    CHECK(xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(draftC->editor))==cVersion && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(draftXl->editor))==xlVersion);
    CHECK(!strcmp(xuiCodeEditGetText(draftC->editor),"int draft = 17; // 未保存\n") && !strcmp(xuiCodeEditGetText(draftXl->editor),"var draft as int = 18 // 未保存\n"));
    printf("help_viewport_test passed: %d native checks; 2500 C and 3200 xlang lines, full-text copy, outer wheel, caret navigation, syntax across viewport boundaries, selection, chapters and 200%% DPI\n",f->checks);fflush(stdout);
cleanup:xideAppDestroy(app);xgeSetQuitRequestCallback(NULL,NULL);xgeQuit();return XGE_OK;
}
static DWORD WINAPI worker(void* user) {fixture* f=user;if(!SetThreadDesktop(f->desktop))return 1;xge_desc_t d={0};d.iWidth=3840;d.iHeight=2160;d.sTitle="xlang IDE private help viewport verification";d.iFlags=XGE_INIT_WINDOW;d.iRunMode=XGE_RUN_GAME_LOOP;int result=xgeInit(&d);if(result==XGE_OK)result=xgeRun(run,f);xideAppDestroy(&f->app);xgeUnit();return result==XGE_OK && f->started && !f->failed?0:1;}
int main(int argc,char** argv) {if(argc!=2)return 2;fixture f={0};f.directory=argv[1];wchar_t n[100];swprintf(n,100,L"XlangHelpViewport-%lu",GetCurrentProcessId());f.desktop=CreateDesktopW(n,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);if(!f.desktop)return 1;HANDLE t=CreateThread(NULL,0,worker,&f,0,NULL);DWORD c=1;if(t){if(WaitForSingleObject(t,60000)!=WAIT_OBJECT_0){fputs("help_viewport_test: timed out\n",stderr);ExitProcess(1);}GetExitCodeThread(t,&c);CloseHandle(t);}CloseDesktop(f.desktop);free(f.markdown);free(f.cText);free(f.xlText);free(f.settings);free(f.font);free(clipboard);return (int)c;}
