#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/recovery_view.h"
#include "ui/editor_view.h"
#include "ui/font_view.h"
#include "services/session.h"
#include "services/ui_font.h"
#include "services/editor_font.h"
#include "platform/path.h"
#include "platform/state_file.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"global_recovery_view_test:%d: %s; %s; %s\n",__LINE__,#v,error.message,xideRecoveryViewMessage(&app)); f->failed=1; goto cleanup; } } while(0)
typedef struct fixture { const char* directory; HDESK desktop; int started,failed; } fixture;
static char* copy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static int writeText(const char* p,const char* s) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static int equals(const char* p,const char* s) { size_t n=0; unsigned char* data=xrtFileReadAllLimit(p,16*1024*1024,&n); int okay=data && n==strlen(s) && !memcmp(data,s,n); xrtFree(data); return okay; }
static int settle(xide_app* app)
{ if(xuiDispatchPendingEvents(app->context)!=XUI_OK) return 0; xideRecoveryViewPoll(app); xideEditorPoll(app); xideWorkbenchLayout(app); return xuiLayout(app->context)==XUI_OK && !app->frameError; }
static int idle(xide_app* app)
{ uint64_t end=GetTickCount64()+15000; do { if(!settle(app)) return 0; if(!xideRecoveryViewBusy(app)) return 1; Sleep(2); } while(GetTickCount64()<end); return 0; }
static int pointer(xide_app* app,xui_rect_t r)
{ int x=r.fX+(r.fW>240 ? 100 : r.fW/2),y=r.fY+r.fH/2; return xuiInputPointerMove(app->context,x,y,0)==XUI_OK && xuiInputPointerDown(app->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(app->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(app); }
static int click(xide_app* app,int control)
{ xui_widget w=xideRecoveryViewControl(app,control),scroll=xideDialogGetScrollViewWidget(xideRecoveryViewWindow(app)); return settle(app) && xuiScrollViewEnsureChildVisible(scroll,w)==XUI_OK && settle(app) && pointer(app,xuiWidgetGetWorldRect(w)); }
static int capture(xide_app* app,const char* path)
{ xui_rect_i_t r={0,0,app->viewportWidth,app->viewportHeight}; return settle(app) && xuiUpdate(app->context,0)==XUI_OK && app->proxy.surfaceClear(&app->proxy,app->target,0xffeeeeeeu)==XUI_OK && xuiRender(app->context,app->target,&r,1)==XUI_OK && xideAppCapture(app,path)==XUI_OK; }
#include "platform/window_state.h"
static int chooseGlobal(xide_app* app,int kind)
{
    if(xideRecoveryViewIsOpen(app) && !click(app,XIDE_RECOVERY_CLOSE)) return 0;
    return xuiComboBoxSetSelected(xideRecoveryViewControl(app,XIDE_RECOVERY_GLOBAL_KIND),kind)==XUI_OK && xideRecoveryViewOpen(app) && idle(app);
}
static int repairGlobal(xide_app* app,int kind)
{ return chooseGlobal(app,kind) && click(app,XIDE_RECOVERY_GLOBAL_REPAIR) && click(app,XIDE_RECOVERY_CONFIRM) && idle(app); }
static int verify(void* data)
{
    fixture* f=data; f->started=1; xide_app app={0}; xide_model_error error={0}; xide_state_file state={0}; xide_settings loaded={0}; int began=0;
    char *root=NULL,*storage=NULL,*settings=NULL,*index=NULL,*window=NULL,*a=NULL,*b=NULL,*path=NULL,*image=NULL,*backup=NULL,*tool=NULL,*transient=NULL; HANDLE lock=INVALID_HANDLE_VALUE;
    const char *damaged="{broken preferences",*badIndex="{\"format\":\"xlang-ide-session-index-1\",\"kind\":false,\"path\":1}",*badWindow="{broken window";
    const char *c="int current(void) { return 42; } // 中文😀\n",*xl="func main(...) as int\n    return 42\nend\n";
    char* id=xideCreateId("ui"); root=id ? xidePathResolve(f->directory,id) : NULL; free(id); CHECK(root && xrtDirCreateAll(root));
    storage=xidePathResolve(root,"中文 全局状态"); settings=xidePathResolve(root,"独立 偏好/settings.json"); index=xidePathResolve(storage,"recent.json"); window=xidePathResolve(storage,"window.json");
    a=xidePathResolve(root,"source.c"); b=xidePathResolve(root,"main.xl"); path=xidePathDirectory(settings);
    CHECK(storage && settings && index && window && a && b && path && xrtDirCreateAll(path) && xrtDirCreateAll(storage)); free(path); path=NULL;
    CHECK(writeText(settings,damaged) && writeText(index,badIndex) && writeText(window,badWindow) && writeText(a,c) && writeText(b,xl));
    app.maxFrames=10000; app.highDpi=1; app.dpiOverride=1.25f; app.sessionRoot=storage; app.settingsPath=settings;
    CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1; CHECK(!app.preferences.source && strstr(xideSessionIssue(&app),"index"));
    CHECK(xideEditorOpen(&app,a,&error)); xide_document* cd=app.activeDocument; CHECK(!xuiCodeDocumentInsert(xuiCodeEditGetDocument(cd->editor),0,"// unsaved C\n"));
    CHECK(xideEditorOpen(&app,b,&error)); xide_document* xd=app.activeDocument; CHECK(!xuiCodeDocumentInsert(xuiCodeEditGetDocument(xd->editor),0,"// unsaved xlang\n"));
    xui_code_selection_t cs={0},xs={0}; cs.iCaretOffset=8; cs.iAnchorOffset=2; xs.iCaretOffset=12; xs.iAnchorOffset=4;
    CHECK(!xuiCodeSelectionSetState(xuiCodeEditGetSelection(cd->editor),xuiCodeEditGetDocument(cd->editor),&cs) && !xuiCodeSelectionSetState(xuiCodeEditGetSelection(xd->editor),xuiCodeEditGetDocument(xd->editor),&xs) && settle(&app));
    uint32_t cv=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(cd->editor)),xv=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xd->editor));
    xide_window_geometry before={0},after={0}; CHECK(xideWindowCapture(xgePlatformNativeHandle(),&before,&error));
    CHECK(chooseGlobal(&app,XIDE_GLOBAL_INDEX) && click(&app,XIDE_RECOVERY_GLOBAL_REPAIR) && click(&app,XIDE_RECOVERY_CANCEL_REPAIR) && equals(index,badIndex));
    CHECK(click(&app,XIDE_RECOVERY_GLOBAL_REPAIR)); image=xidePathResolve(root,"native-global-index-confirm.png");
    CHECK(image && !xuiScrollViewEnsureChildVisible(xideDialogGetScrollViewWidget(xideRecoveryViewWindow(&app)),xideRecoveryViewControl(&app,XIDE_RECOVERY_CONFIRM)) && capture(&app,image)); free(image); image=NULL;
    CHECK(click(&app,XIDE_RECOVERY_CONFIRM) && idle(&app) && strstr(xideRecoveryViewMessage(&app),"最近工程索引已重建"));
    backup=copy(xideRecoveryViewBackupPath(&app)); path=backup ? xidePathResolve(backup,"original.bin") : NULL; CHECK(path && equals(path,badIndex)); free(path); path=NULL; free(backup); backup=NULL;
    CHECK(xideStateOpen(&state,index,"xlang-ide-session-index-1",&error)); xstrview kind={0}; CHECK(xrtValueGetString(xrtValueObjectGet(state.source,(xstrview){"kind",4}),&kind) && kind.Size==4 && !memcmp(kind.Data,"none",4)); xideStateClear(&state);
    CHECK(repairGlobal(&app,XIDE_GLOBAL_WINDOW) && strstr(xideRecoveryViewMessage(&app),"主窗口位置已重建"));
    CHECK(xideWindowCapture(xgePlatformNativeHandle(),&after,&error) && !memcmp(&before,&after,sizeof(before)));
    backup=copy(xideRecoveryViewBackupPath(&app)); path=xidePathResolve(backup,"original.bin"); CHECK(path && equals(path,badWindow)); free(path); path=NULL; free(backup); backup=NULL;
    CHECK(xideStateOpen(&state,window,"xlang-ide-window-1",&error) && xideWindowGeometryRead(xrtValueObjectGet(state.source,(xstrview){"placement",9}),&after) && !memcmp(&before,&after,sizeof(before))); xideStateClear(&state);
    CHECK(chooseGlobal(&app,XIDE_GLOBAL_SETTINGS)); wchar_t w[4096]; CHECK(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,settings,-1,w,4096));
    lock=CreateFileW(w,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL); CHECK(lock!=INVALID_HANDLE_VALUE);
    CHECK(click(&app,XIDE_RECOVERY_GLOBAL_REPAIR) && click(&app,XIDE_RECOVERY_CONFIRM) && idle(&app) && equals(settings,damaged) && strstr(xideRecoveryViewMessage(&app),"Cannot lock original") && !xideRecoveryViewBackupPath(&app)); CloseHandle(lock); lock=INVALID_HANDLE_VALUE;
    CHECK(repairGlobal(&app,XIDE_GLOBAL_SETTINGS) && strstr(xideRecoveryViewMessage(&app),"程序偏好已重建") && app.preferences.source);
    backup=copy(xideRecoveryViewBackupPath(&app)); path=xidePathResolve(backup,"original.bin"); CHECK(path && equals(path,damaged)); free(path); path=NULL; free(backup); backup=NULL;
    CHECK(xideSettingsLoad(&loaded,settings,&error) && loaded.editorFontSize==app.codeFontSize && loaded.uiFontSize==app.uiFontSize && xidePathEqual(loaded.editorFont,app.codeFontPath) && xidePathEqual(loaded.uiFont,app.uiFontPath)); xideSettingsClear(&loaded);
    CHECK(cv==xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(cd->editor)) && xv==xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xd->editor)) && cd->dirty && xd->dirty && app.activeDocument==xd);
    xui_code_selection_t selection={0}; xuiCodeSelectionGetState(xuiCodeEditGetSelection(cd->editor),&selection); CHECK(selection.iCaretOffset==cs.iCaretOffset && selection.iAnchorOffset==cs.iAnchorOffset);
    xuiCodeSelectionGetState(xuiCodeEditGetSelection(xd->editor),&selection); CHECK(selection.iCaretOffset==xs.iCaretOffset && selection.iAnchorOffset==xs.iAnchorOffset);
    CHECK(!strcmp(cd->language,"c") && !strcmp(xd->language,"xlang") && equals(a,c) && equals(b,xl));
    CHECK(xuiCodeDocumentCanUndo(xuiCodeEditGetDocument(cd->editor)) && xuiCodeDocumentCanUndo(xuiCodeEditGetDocument(xd->editor)));
    CHECK(!xuiCodeDocumentUndo(xuiCodeEditGetDocument(cd->editor)) && !strcmp(xuiCodeEditGetText(cd->editor),c) && !xuiCodeDocumentRedo(xuiCodeEditGetDocument(cd->editor)) && strstr(xuiCodeEditGetText(cd->editor),"// unsaved C\n"));
    CHECK(!xuiCodeDocumentUndo(xuiCodeEditGetDocument(xd->editor)) && !strcmp(xuiCodeEditGetText(xd->editor),xl) && !xuiCodeDocumentRedo(xuiCodeEditGetDocument(xd->editor)) && strstr(xuiCodeEditGetText(xd->editor),"// unsaved xlang\n"));
    image=xidePathResolve(root,"native-global-repaired.png"); CHECK(image && capture(&app,image)); free(image); image=NULL;
    CHECK(click(&app,XIDE_RECOVERY_CLOSE) && xideSessionSave(&app,0,&error));
    /* Valid unknown fields survive an explicit preference reconstruction. */
    tool=xidePathResolve(root,"tool.exe"); transient=xidePathResolve(root,"transient.exe"); CHECK(tool && transient && writeText(tool,"fixture") && writeText(transient,"temporary override")); app.compilerPath=transient; app.cServerPath=transient;
    CHECK(writeText(settings,"{\"format\":\"xlang-ide-settings-1\",\"future\":77,\"toolchain\":{\"compiler\":\"../tool.exe\",\"cServer\":\"../tool.exe\"},\"editor\":{\"futureEditor\":true},\"ui\":{\"font\":{\"futureFont\":88}}}") && xideSettingsLoad(&app.preferences,settings,&error));
    CHECK(xideUiFontApply(&app,app.uiFontPath,40,&error) && chooseGlobal(&app,XIDE_GLOBAL_SETTINGS) && click(&app,XIDE_RECOVERY_GLOBAL_REPAIR));
    image=xidePathResolve(root,"native-global-large-confirm.png"); CHECK(image && !xuiScrollViewEnsureChildVisible(xideDialogGetScrollViewWidget(xideRecoveryViewWindow(&app)),xideRecoveryViewControl(&app,XIDE_RECOVERY_CONFIRM)) && capture(&app,image)); free(image); image=NULL;
    CHECK(click(&app,XIDE_RECOVERY_CONFIRM) && idle(&app) && xrtValueObjectGet(app.preferences.source,(xstrview){"future",6}));
    xvalue* ui=xrtValueObjectGet(app.preferences.source,(xstrview){"ui",2}),*font=xrtValueObjectGet(ui,(xstrview){"font",4}); CHECK(xrtValueObjectGet(font,(xstrview){"futureFont",10}) && app.preferences.uiFontSize==40);
    CHECK(xidePathEqual(app.preferences.compiler,tool) && (XIDE_ENABLE_C_LSP ? xidePathEqual(app.preferences.cServer,tool) : !app.preferences.cServer) && app.compilerPath==transient && app.cServerPath==transient);
    CHECK(click(&app,XIDE_RECOVERY_CLOSE) && xgeEnd()==XGE_OK); began=0; xideAppDestroy(&app);
    /* Preference recovery remains available when scoped persistence is disabled. */
    CHECK(writeText(settings,damaged)); memset(&app,0,sizeof(app)); app.hidden=1; app.noSession=1; app.settingsPath=settings; app.maxFrames=10000;
    CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1;
    CHECK(chooseGlobal(&app,XIDE_GLOBAL_SETTINGS) && !xuiWidgetGetEnabled(xideRecoveryViewControl(&app,XIDE_RECOVERY_REPAIR)) && xuiWidgetGetEnabled(xideRecoveryViewControl(&app,XIDE_RECOVERY_GLOBAL_REPAIR)));
    CHECK(click(&app,XIDE_RECOVERY_GLOBAL_REPAIR) && click(&app,XIDE_RECOVERY_CONFIRM) && idle(&app) && app.preferences.source);
    puts("global_recovery_view_test passed: native index/window/preferences confirmations and locked failure, independent raw copies, native geometry preserved, current fonts and unknown members, live C/xlang versions/selection/dirty/undo/source protection, 40 DIP scrolling and no-session preference recovery; 3 actual PNGs");
cleanup:
    if(lock!=INVALID_HANDLE_VALUE) CloseHandle(lock);
    xideStateClear(&state); xideSettingsClear(&loaded); if(began) xgeEnd(); xideAppDestroy(&app);
    free(root); free(storage); free(settings); free(index); free(window); free(a); free(b); free(path); free(image); free(backup); free(tool); free(transient); xgeQuit(); return XGE_OK;
}
static DWORD WINAPI worker(void* data)
{
    fixture* f=data; if(!SetThreadDesktop(f->desktop)) return 1; xge_desc_t desc={0}; desc.iWidth=1600; desc.iHeight=1000; desc.sTitle="xlang IDE global recovery verification"; desc.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; desc.iRunMode=XGE_RUN_GAME_LOOP;
    int result=xgeInit(&desc); if(result==XGE_OK) result=xgeRun(verify,f); xgeUnit(); return result==XGE_OK && f->started && !f->failed ? 0 : 1;
}
int main(int argc,char** argv)
{
    if(argc!=2 || !xrtDirCreateAll(argv[1])) return 2;
    fixture f={0}; f.directory=argv[1]; wchar_t name[100]; swprintf(name,100,L"XlangRecoveryVerification-%lu",GetCurrentProcessId());
    f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) return 1;
    HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1;
    if(thread) { if(WaitForSingleObject(thread,90000)!=WAIT_OBJECT_0) { fputs("global_recovery_view_test timed out\n",stderr); ExitProcess(1); } GetExitCodeThread(thread,&code); CloseHandle(thread); }
    CloseDesktop(f.desktop); return (int)code;
}
