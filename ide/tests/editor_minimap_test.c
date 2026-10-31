#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "platform/path.h"
#include "theme/theme.h"
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct fixture {
    xide_app app;
    HDESK desktop;
    const char* directory;
    char *settings,*text;
    int checks,failed,started;
} fixture;
#define CHECK(expr) do { ++f->checks; if(!(expr)) { fprintf(stderr,"editor_minimap_test:%d: %s\n",__LINE__,#expr); f->failed=1; goto cleanup; } } while(0)
static int frame(xide_app* app) { return xideAppFrame(app)==XGE_OK && !app->frameError; }
static int minimap(xui_widget editor)
{
    int visible=0;float width=0;
    return xuiCodeEditGetMinimap(editor,&visible,&width)==XUI_OK && visible && width==96.0f &&
        (xuiCodeEditGetDisplayOptions(editor)&XUI_CODE_EDIT_SHOW_MINIMAP);
}
static int capture(fixture* f,const char* name)
{
    char* path=xidePathResolve(f->directory,name);
    int okay=path && xideAppCapture(&f->app,path)==XGE_OK;free(path);return okay;
}
static int run(void* user)
{
    fixture* f=user;xide_app* app=&f->app;xide_model_error error={0};
    f->started=1;app->hidden=1;app->noSession=1;app->disableAutomaticDiagnostics=1;app->maxFrames=10000;app->dpiOverride=1;
    CHECK(xrtDirCreateAll(f->directory));f->settings=xidePathResolve(f->directory,"preferences.json");CHECK(f->settings);app->settingsPath=f->settings;
    CHECK(xideAppCreate(app)==XUI_OK && frame(app));
    CHECK(minimap(app->editors[0]) && minimap(app->editors[1]));
    CHECK(!(xuiCodeEditGetDisplayOptions(app->helpExample)&XUI_CODE_EDIT_SHOW_MINIMAP));
    CHECK(!(xuiCodeEditGetDisplayOptions(app->panels[XIDE_OUTPUT])&XUI_CODE_EDIT_SHOW_MINIMAP));
    f->text=calloc(131072,1);CHECK(f->text);
    for(int i=0;i<800;++i) {
        char line[128];snprintf(line,sizeof(line),"// 中文 line %d\nint value_%d = %d;\n",i,i,i);strcat(f->text,line);
    }
    xide_document* c=xideDocumentNew(&app->documents,"c",&error);
    xide_document* xl=xideDocumentNew(&app->documents,"xlang",&error);CHECK(c && xl && minimap(c->editor) && minimap(xl->editor));
    CHECK(xuiCodeEditSetText(c->editor,f->text)==XUI_OK && xuiCodeEditSetText(xl->editor,f->text)==XUI_OK);
    for(int language=0;language<2;++language) {
        xide_document* d=language?xl:c;xui_widget edit=d->editor;
        CHECK(xideEditorShow(app,d)==XUI_OK && frame(app));
        xui_code_document doc=xuiCodeEditGetDocument(edit);uint32_t version=xuiCodeDocumentGetVersion(doc);
        xui_code_selection_t before={0},after={0};
        CHECK(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(edit),doc,5,0)==XUI_OK);
        CHECK(xuiCodeSelectionGetState(xuiCodeEditGetSelection(edit),&before)==XUI_OK);
        for(int theme=0;theme<IDE_THEME_COUNT;++theme) {
            CHECK(xideWorkbenchApplyTheme(app,theme)==XUI_OK && frame(app));
            xui_style_property_t property={0};
            CHECK(xuiWidgetGetResolvedStyleProperty(edit,"codeedit.minimap.viewport_color",&property)==XUI_OK);
            CHECK(property.tValue.iType==XUI_STYLE_VALUE_COLOR &&
                property.tValue.iColor==((ideThemeColor(theme,IDE_ACCENT)&0xffffff00u)|46u));
            for(int dpi=0;dpi<3;++dpi) {
                app->dpiOverride=dpi==0?1.0f:dpi==1?1.25f:2.0f;CHECK(frame(app));
                CHECK(minimap(edit) && !xuiWidgetGetVisible(xuiCodeEditGetVScrollBarWidget(edit)));
                xui_rect_t map=xuiCodeEditGetMinimapRect(edit),world=xuiWidgetGetWorldRect(edit);
                CHECK(map.fW>0 && map.fW<=96.0f && map.fH>0 && map.fX+map.fW<=world.fW+0.01f);
                CHECK(xuiCodeEditSetScroll(edit,0,0)==XUI_OK && frame(app));
                float x=world.fX+map.fX+map.fW/2,y=world.fY+map.fY+map.fH*0.25f,scroll=0,dragged=0;
                CHECK(xuiInputPointerDown(app->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && frame(app));
                CHECK(xuiCodeEditGetScroll(edit,NULL,&scroll)==XUI_OK && scroll>0);
                y=world.fY+map.fY+map.fH*0.8f;
                CHECK(xuiInputPointerMove(app->context,x,y,XUI_POINTER_BUTTON_LEFT)==XUI_OK && frame(app));
                CHECK(xuiCodeEditGetScroll(edit,NULL,&dragged)==XUI_OK && dragged>scroll);
                CHECK(xuiInputPointerUp(app->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && frame(app));
                CHECK(xuiGetPointerCapture(app->context)==NULL);
                CHECK(xuiCodeSelectionGetState(xuiCodeEditGetSelection(edit),&after)==XUI_OK &&
                    before.iCaretOffset==after.iCaretOffset && before.iAnchorOffset==after.iAnchorOffset &&
                    xuiCodeDocumentGetVersion(doc)==version && !strcmp(xuiCodeEditGetText(edit),f->text));
            }
        }
    }
    app->dpiOverride=1;CHECK(xideWorkbenchApplyTheme(app,IDE_THEME_GRAPHITE)==XUI_OK && xideEditorShow(app,c)==XUI_OK && frame(app));
    CHECK(xuiCodeEditSetScroll(c->editor,0,0)==XUI_OK && frame(app));
    xui_rect_t editor=xuiWidgetGetWorldRect(c->editor);float x=editor.fX+120,y=editor.fY+60;
    CHECK(xuiInputPointerDown(app->context,x,y,XUI_POINTER_BUTTON_RIGHT,XUI_POINTER_BUTTON_RIGHT)==XUI_OK &&
        xuiInputPointerUp(app->context,x,y,XUI_POINTER_BUTTON_RIGHT,0)==XUI_OK && frame(app));
    xui_widget menu=xuiCodeEditGetMenuWidget(c->editor);
    CHECK(menu && xuiMenuIsOpen(menu) && xuiGetFocusWidget(app->context)==menu);
    CHECK(capture(f,"editor-minimap-menu.png"));
    CHECK(xuiInputKeyDownEx(app->context,XUI_KEY_DOWN,0,NULL)==XUI_OK &&
        xuiInputKeyUpEx(app->context,XUI_KEY_DOWN,0,NULL)==XUI_OK && frame(app));
    CHECK(xuiMenuIsOpen(menu) && xuiGetFocusWidget(app->context)==menu);
    CHECK(xuiInputKeyDownEx(app->context,XUI_KEY_ESCAPE,0,NULL)==XUI_OK &&
        xuiInputKeyUpEx(app->context,XUI_KEY_ESCAPE,0,NULL)==XUI_OK && frame(app));
    CHECK(xideEditorClose(app,c,1,&error) && xideEditorClose(app,xl,1,&error) && frame(app));
    xide_document* reused=xideDocumentNew(&app->documents,"c",&error);
    CHECK(reused==c && minimap(reused->editor) && !xuiCodeDocumentCanUndo(xuiCodeEditGetDocument(reused->editor)));
    CHECK(xideEditorShow(app,reused)==XUI_OK && frame(app));
    CHECK(capture(f,"editor-minimap-empty.png"));
    printf("editor_minimap_test passed: %d native checks; C/xlang drafts, demos, 4 themes, 100/125/200%% DPI, minimap click/drag/capture, text/selection preservation, context menu keyboard navigation, slot reuse\n",f->checks);fflush(stdout);
cleanup:
    xideAppDestroy(app);xgeSetQuitRequestCallback(NULL,NULL);xgeQuit();return XGE_OK;
}
static DWORD WINAPI worker(void* user)
{
    fixture* f=user;if(!SetThreadDesktop(f->desktop))return 1;
    xge_desc_t d={0};d.iWidth=1600;d.iHeight=1000;d.sTitle="xlang IDE private minimap verification";d.iFlags=XGE_INIT_WINDOW;d.iRunMode=XGE_RUN_GAME_LOOP;
    int result=xgeInit(&d);if(result==XGE_OK)result=xgeRun(run,f);xideAppDestroy(&f->app);xgeUnit();return result==XGE_OK && f->started && !f->failed?0:1;
}
int main(int argc,char** argv)
{
    if(argc!=2)return 2;
    fixture f={0};f.directory=argv[1];wchar_t name[100];swprintf(name,100,L"XlangIDEMinimap-%lu",GetCurrentProcessId());
    f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);if(!f.desktop)return 1;
    HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL);DWORD code=1;
    if(thread){if(WaitForSingleObject(thread,60000)!=WAIT_OBJECT_0){fputs("editor_minimap_test timed out\n",stderr);ExitProcess(1);}GetExitCodeThread(thread,&code);CloseHandle(thread);}
    CloseDesktop(f.desktop);free(f.settings);free(f.text);return (int)code;
}
