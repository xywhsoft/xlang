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
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"recovery_view_test:%d: %s; %s; %s\n",__LINE__,#v,error.message,xideRecoveryViewMessage(&app)); f->failed=1; goto cleanup; } } while(0)
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
static int choose(xide_app* app,const char* part,int archived)
{
    xui_widget list=xideRecoveryViewControl(app,XIDE_RECOVERY_LIST); int index=-1;
    for(int i=0;i<xuiListViewGetItemCount(list);++i) { const char* s=xuiListViewGetItemText(list,i); if(s && strstr(s,part) && (!!strstr(s,"备份")==archived)) { index=i; break; } }
    if(index<0) return 0;
    xui_widget scroll=xideDialogGetScrollViewWidget(xideRecoveryViewWindow(app));
    if(xuiScrollViewEnsureChildVisible(scroll,list)!=XUI_OK || xuiListViewEnsureVisible(list,index)!=XUI_OK || !settle(app)) return 0;
    xui_rect_t row=xuiListViewGetItemRect(list,index),world=xuiWidgetGetWorldRect(list); row.fX+=world.fX; row.fY+=world.fY;
    return pointer(app,row) && idle(app) && xuiListViewGetSelected(list)==index;
}
static int capture(xide_app* app,const char* path)
{ xui_rect_i_t r={0,0,app->viewportWidth,app->viewportHeight}; return settle(app) && xuiUpdate(app->context,0)==XUI_OK && app->proxy.surfaceClear(&app->proxy,app->target,0xffeeeeeeu)==XUI_OK && xuiRender(app->context,app->target,&r,1)==XUI_OK && xideAppCapture(app,path)==XUI_OK; }
static int verify(void* data)
{
    fixture* f=data; f->started=1; xide_app app={0}; xide_model_error error={0}; xide_state_file state={0}; int began=0;
    char *root=NULL,*storage=NULL,*settings=NULL,*a=NULL,*b=NULL,*directory=NULL,*metadata=NULL,*orphanC=NULL,*orphanX=NULL,*invalid=NULL,*image=NULL,*largeImage=NULL,*repairedImage=NULL,*backup=NULL,*copyState=NULL;
    HANDLE locked=INVALID_HANDLE_VALUE;
    const char *c="int source(void) { return 1; }\n",*xl="func main(...) as int\n    return 0\nend\n",*recoveredC="int recovered(void) { return 77; } // 中文😀\n",*recoveredX="func recovered(...) as int\n    return 88\nend\n";
    char* id=xideCreateId("ui"); root=id ? xidePathResolve(f->directory,id) : NULL; free(id); CHECK(root && xrtDirCreateAll(root));
    storage=xidePathResolve(root,"中文 恢复"); settings=xidePathResolve(root,"settings.json"); a=xidePathResolve(root,"source.c"); b=xidePathResolve(root,"main.xl"); image=xidePathResolve(root,"native-recovery-preview.png"); largeImage=xidePathResolve(root,"native-recovery-large.png"); repairedImage=xidePathResolve(root,"native-recovery-repaired.png"); CHECK(storage && settings && a && b && image && largeImage && repairedImage && writeText(a,c) && writeText(b,xl));
    app.hidden=1; app.maxFrames=10000; app.highDpi=1; app.dpiOverride=1.25f; app.sessionRoot=storage; app.settingsPath=settings;
    CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1;
    CHECK(xideEditorOpen(&app,a,&error) && xideEditorOpen(&app,b,&error) && !xuiCodeDocumentInsert(xuiCodeEditGetDocument(app.activeDocument->editor),0,"// saved recovery\n") && xideSessionSave(&app,0,&error));
    directory=copy(xideSessionDirectory(&app)); metadata=xideRecoveryPath(directory,"session.json",&error);
    orphanC=xideRecoveryPath(directory,"recovery/doc-11111111-2222-3333-4444-555555555555-1111111111111111.text",&error); orphanX=xideRecoveryPath(directory,"recovery/doc-22222222-3333-4444-5555-666666666666-2222222222222222.text",&error); invalid=xideRecoveryPath(directory,"recovery/broken.text",&error);
    CHECK(directory && metadata && orphanC && orphanX && invalid && writeText(orphanC,recoveredC) && writeText(orphanX,recoveredX)); const unsigned char bad[]={0xff,0}; CHECK(xrtFileWriteAtomic(invalid,(xbytesview){bad,sizeof(bad)}));
    CHECK(xgeEnd()==XGE_OK); began=0; xideAppDestroy(&app); CHECK(writeText(metadata,"{damaged current session"));
    memset(&app,0,sizeof(app)); app.hidden=1; app.maxFrames=10000; app.highDpi=1; app.dpiOverride=1.25f; app.sessionRoot=storage; app.settingsPath=settings;
    CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1; CHECK(*xideSessionIssue(&app));
    CHECK(xideEditorOpen(&app,a,&error)); xide_document* cd=app.activeDocument; CHECK(!xuiCodeDocumentInsert(xuiCodeEditGetDocument(cd->editor),0,"// current unsaved C\n"));
    CHECK(xideEditorOpen(&app,b,&error)); xide_document* xd=app.activeDocument; CHECK(!xuiCodeDocumentInsert(xuiCodeEditGetDocument(xd->editor),0,"// current unsaved xlang\n"));
    uint32_t cv=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(cd->editor)),xv=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xd->editor));
    xideDispatchAction(&app,XIDE_ACTION_RECOVERY); CHECK(xideRecoveryViewIsOpen(&app) && idle(&app) && xuiListViewGetItemCount(xideRecoveryViewControl(&app,XIDE_RECOVERY_LIST))>=4);
    CHECK(!xideFontViewOpen(&app)); xideDispatchAction(&app,XIDE_ACTION_NEW_FILE); CHECK(app.activeDocument==xd);
    CHECK(click(&app,XIDE_RECOVERY_REPAIR) && click(&app,XIDE_RECOVERY_CANCEL_REPAIR) && equals(metadata,"{damaged current session"));
    CHECK(choose(&app,"broken.text",0) && !xuiWidgetGetEnabled(xideRecoveryViewControl(&app,XIDE_RECOVERY_RESTORE)) && strstr(xideRecoveryViewMessage(&app),"UTF-8"));
    CHECK(choose(&app,"1111111111111111.text",0) && !strcmp(xuiCodeEditGetText(xideRecoveryViewControl(&app,XIDE_RECOVERY_CODE)),recoveredC));
    xui_widget preview=xideRecoveryViewControl(&app,XIDE_RECOVERY_CODE); CHECK(xuiCodeEditIsReadonly(preview) && !xuiSetFocusWidget(app.context,preview) && !xuiInputText(app.context,'N') && settle(&app) && !strcmp(xuiCodeEditGetText(preview),recoveredC));
    CHECK(xuiWidgetGetEnabled(xideRecoveryViewControl(&app,XIDE_RECOVERY_REPAIR)) && xuiWidgetGetEffectiveEnabled(xideRecoveryViewControl(&app,XIDE_RECOVERY_REPAIR)));
    CHECK(!(xuiWidgetGetStateId(xideRecoveryViewControl(&app,XIDE_RECOVERY_REPAIR))&XUI_WIDGET_STATE_DISABLED));
    CHECK(capture(&app,image)); CHECK(!xuiComboBoxSetSelected(xideRecoveryViewControl(&app,XIDE_RECOVERY_LANGUAGE),0) && click(&app,XIDE_RECOVERY_RESTORE) && !xideRecoveryViewIsOpen(&app));
    xide_document* recovered=app.activeDocument; CHECK(recovered && !recovered->path && recovered->dirty && !strcmp(recovered->language,"c") && !strcmp(xuiCodeEditGetText(recovered->editor),recoveredC));
    CHECK(xideRecoveryViewOpen(&app) && idle(&app) && choose(&app,"2222222222222222.text",0) && !xuiComboBoxSetSelected(xideRecoveryViewControl(&app,XIDE_RECOVERY_LANGUAGE),1) && click(&app,XIDE_RECOVERY_RESTORE));
    CHECK(!app.activeDocument->path && !strcmp(app.activeDocument->language,"xlang") && !strcmp(xuiCodeEditGetText(app.activeDocument->editor),recoveredX));
    CHECK(xideRecoveryViewOpen(&app) && idle(&app) && choose(&app,"1111111111111111.text",0));
    CHECK(xideUiFontApply(&app,app.uiFontPath,40,&error) && settle(&app) && xuiListViewGetFont(xideRecoveryViewControl(&app,XIDE_RECOVERY_LIST))==app.uiFont && xuiCodeEditGetFont(preview)==app.codeFont);
    float pw=0,ph=0; CHECK(!xideDialogGetContentSize(xideRecoveryViewWindow(&app),&pw,&ph) && pw>app.viewportWidth && ph>app.viewportHeight);
    CHECK(click(&app,XIDE_RECOVERY_REPAIR) && (xuiWidgetGetStateId(xideRecoveryViewControl(&app,XIDE_RECOVERY_REPAIR))&XUI_WIDGET_STATE_DISABLED) && !xuiScrollViewEnsureChildVisible(xideDialogGetScrollViewWidget(xideRecoveryViewWindow(&app)),xideRecoveryViewControl(&app,XIDE_RECOVERY_CONFIRM)) && capture(&app,largeImage) && click(&app,XIDE_RECOVERY_CANCEL_REPAIR));
    float sx=0,sy=0; CHECK(!xideDialogGetScroll(xideRecoveryViewWindow(&app),&sx,&sy) && sx==0 && sy==0);
    CHECK(!(xuiWidgetGetStateId(xideRecoveryViewControl(&app,XIDE_RECOVERY_REPAIR))&XUI_WIDGET_STATE_DISABLED));
    CHECK(xideUiFontApply(&app,app.uiFontPath,24,&error) && settle(&app));
    wchar_t w[4096]; CHECK(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,metadata,-1,w,4096)); locked=CreateFileW(w,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL); CHECK(locked!=INVALID_HANDLE_VALUE);
    CHECK(click(&app,XIDE_RECOVERY_REPAIR) && click(&app,XIDE_RECOVERY_CONFIRM) && idle(&app) && equals(metadata,"{damaged current session") && strstr(xideRecoveryViewMessage(&app),"Cannot lock original"));
    CHECK(!xideDialogGetScroll(xideRecoveryViewWindow(&app),&sx,&sy) && sx==0 && sy==0);
    CHECK(!(xuiWidgetGetStateId(xideRecoveryViewControl(&app,XIDE_RECOVERY_REPAIR))&XUI_WIDGET_STATE_DISABLED));
    CloseHandle(locked); locked=INVALID_HANDLE_VALUE;
    CHECK(click(&app,XIDE_RECOVERY_REPAIR) && click(&app,XIDE_RECOVERY_CONFIRM) && idle(&app) && strstr(xideRecoveryViewMessage(&app),"当前会话已重建"));
    CHECK(!xideDialogGetScroll(xideRecoveryViewWindow(&app),&sx,&sy) && sx==0 && sy==0);
    CHECK(!(xuiWidgetGetStateId(xideRecoveryViewControl(&app,XIDE_RECOVERY_REPAIR))&XUI_WIDGET_STATE_DISABLED));
    backup=copy(xideRecoveryViewBackupPath(&app)); copyState=backup ? xidePathResolve(backup,"session.json") : NULL; CHECK(backup && copyState && equals(copyState,"{damaged current session"));
    CHECK(xideStateOpen(&state,metadata,"xlang-ide-session-1",&error) && xideSessionValidate(state.source)); xideStateClear(&state);
    CHECK(equals(a,c) && equals(b,xl) && cv==xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(cd->editor)) && xv==xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xd->editor)) && cd->dirty && xd->dirty);
    CHECK(capture(&app,repairedImage) && click(&app,XIDE_RECOVERY_REFRESH) && idle(&app) && choose(&app,"1111111111111111.text",1) && !strcmp(xuiCodeEditGetText(preview),recoveredC));
    CHECK(click(&app,XIDE_RECOVERY_CLOSE) && !xideRecoveryViewIsOpen(&app) && xideSessionSave(&app,0,&error));
    puts("recovery_view_test passed: native protected-state manager, real row/button input, read-only preview, invalid UTF-8 rejection, C/xlang untitled recovery, modal isolation, large UI font scrolling, locked repair preservation, complete backup/rebuild, active editor version/dirty state and source bytes, archived preview and actual screenshots");
cleanup:
    if(locked!=INVALID_HANDLE_VALUE) CloseHandle(locked);
    xideStateClear(&state); if(began) xgeEnd(); xideAppDestroy(&app); free(root); free(storage); free(settings); free(a); free(b); free(directory); free(metadata); free(orphanC); free(orphanX); free(invalid); free(image); free(largeImage); free(repairedImage); free(backup); free(copyState); xgeQuit(); return XGE_OK;
}
static DWORD WINAPI worker(void* data)
{
    fixture* f=data; if(!SetThreadDesktop(f->desktop)) return 1; xge_desc_t desc={0}; desc.iWidth=1600; desc.iHeight=1000; desc.sTitle="xlang IDE recovery verification"; desc.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; desc.iRunMode=XGE_RUN_GAME_LOOP;
    int result=xgeInit(&desc); if(result==XGE_OK) result=xgeRun(verify,f); xgeUnit(); return result==XGE_OK && f->started && !f->failed ? 0 : 1;
}
int main(int argc,char** argv)
{
    if(argc!=2 || !xrtDirCreateAll(argv[1])) return 2;
    fixture f={0}; f.directory=argv[1]; wchar_t name[100]; swprintf(name,100,L"XlangRecoveryVerification-%lu",GetCurrentProcessId());
    f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) return 1;
    HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1;
    if(thread) { if(WaitForSingleObject(thread,90000)!=WAIT_OBJECT_0) { fputs("recovery_view_test timed out\n",stderr); ExitProcess(1); } GetExitCodeThread(thread,&code); CloseHandle(thread); }
    CloseDesktop(f.desktop); return (int)code;
}
