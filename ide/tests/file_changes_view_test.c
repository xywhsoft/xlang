#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/file_changes_view.h"
#include "services/file_sync.h"
#include "ui/editor_view.h"
#include "ui/workspace_view.h"
#include "ui/font_view.h"
#include "ui/recovery_view.h"
#include "services/ui_font.h"
#include "services/editor_font.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"file_changes_view_test:%d: %s; %s; %s\n",__LINE__,#v,error.message,xideFileChangesViewMessage(&app)); f->failed=1; goto cleanup; } } while(0)
typedef struct fixture { const char* directory; HDESK desktop; int started,failed; } fixture;
static int writeText(const char* p,const char* s) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static int equals(const char* p,const char* s) { size_t n=0; unsigned char* raw=xrtFileReadAllLimit(p,16*1024*1024,&n); int same=raw && n==strlen(s) && !memcmp(raw,s,n); xrtFree(raw); return same; }
static xui_widget findInput(xide_app* app,xui_widget w)
{ if(xuiWidgetGetType(w)==xuiWidgetFindType(app->context,"input")) return w; for(xui_widget c=xuiWidgetGetFirstChild(w);c;c=xuiWidgetGetNextSibling(c)) { xui_widget p=findInput(app,c); if(p) return p; } return NULL; }
static int writeValue(const char* path,const xvalue* value)
{ size_t n=0; char* text=xrtJsonStringify(value,true,&n); int okay=text && xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,n}); xrtFree(text); return okay; }
static int settle(xide_app* app)
{ if(xuiDispatchPendingEvents(app->context)!=XUI_OK) return 0; xideEditorPoll(app); xideFileSyncPoll(app); xideFileChangesViewPoll(app); xideWorkbenchLayout(app); return xuiLayout(app->context)==XUI_OK && !app->frameError; }
static int pending(xide_app* app,size_t count)
{ uint64_t due=GetTickCount64()+12000; do { if(!settle(app)) return 0; size_t ready=0; for(size_t i=0;i<xideFileSyncCount(app);++i) if(xideFileSyncAt(app,i)->snapshot.state==XIDE_WATCH_READY) ++ready; if(ready>=count && xideFileSyncSettled(app)) return 1; Sleep(10); } while(GetTickCount64()<due); return 0; }
static int pointer(xide_app* app,xui_rect_t r)
{ int x=r.fX+(r.fW>240 ? 100 : r.fW/2),y=r.fY+r.fH/2; return xuiInputPointerMove(app->context,x,y,0)==XUI_OK && xuiInputPointerDown(app->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(app->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(app); }
static int click(xide_app* app,int id)
{ xui_widget w=xideFileChangesViewControl(app,id),scroll=xideDialogGetScrollViewWidget(xideFileChangesViewWindow(app)); return settle(app) && xuiScrollViewEnsureChildVisible(scroll,w)==XUI_OK && settle(app) && pointer(app,xuiWidgetGetWorldRect(w)); }
static int choose(xide_app* app,const char* name)
{ xui_widget list=xideFileChangesViewControl(app,XIDE_CHANGES_LIST); int index=-1; for(int i=0;i<xuiListViewGetItemCount(list);++i) if(strstr(xuiListViewGetItemText(list,i),name)) { index=i; break; } if(index<0) return 0; if(xuiScrollViewEnsureChildVisible(xideDialogGetScrollViewWidget(xideFileChangesViewWindow(app)),list)!=XUI_OK || xuiListViewEnsureVisible(list,index)!=XUI_OK || !settle(app)) return 0; xui_rect_t r=xuiListViewGetItemRect(list,index),world=xuiWidgetGetWorldRect(list); r.fX+=world.fX; r.fY+=world.fY; return pointer(app,r) && xuiListViewGetSelected(list)==index; }
static int capture(xide_app* app,const char* path)
{ xui_rect_i_t r={0,0,app->viewportWidth,app->viewportHeight}; return settle(app) && xuiUpdate(app->context,0)==XUI_OK && app->proxy.surfaceClear(&app->proxy,app->target,0xffeeeeeeu)==XUI_OK && xuiRender(app->context,app->target,&r,1)==XUI_OK && xideAppCapture(app,path)==XUI_OK; }
static int verify(void* data)
{
    fixture* f=data; f->started=1; xide_app app={0}; xide_model_error error={0}; int began=0; xide_project project={0}; xvalue* value=NULL; char *root=NULL,*a=NULL,*b=NULL,*settings=NULL,*image=NULL,*dark=NULL,*large=NULL,*manifest=NULL,*modelImage=NULL;
    const char* c="int original = 1; // 中文😀\n",*xl="func main(...) as int\n    return 0\nend\n",*disk="int external = 2; // 当前磁盘\n";
    char* id=xideCreateId("ui"); root=id ? xidePathResolve(f->directory,id) : NULL; free(id); CHECK(root && xrtDirCreateAll(root));
    a=xidePathResolve(root,"source.c"); b=xidePathResolve(root,"main.xl"); settings=xidePathResolve(root,"settings.json"); image=xidePathResolve(root,"native-file-changes-c.png"); dark=xidePathResolve(root,"native-file-changes-xlang.png"); large=xidePathResolve(root,"native-file-changes-large.png"); CHECK(a && b && settings && image && dark && large && writeText(a,c) && writeText(b,xl));
    manifest=xidePathResolve(root,"app.xlp"); modelImage=xidePathResolve(root,"native-file-changes-manifest.png");
    CHECK(manifest && modelImage && xideProjectCreate(&project,manifest,"Native model",b,&error) && xideProjectAddFile(&project,a,NULL,&error) && xideProjectSave(&project,NULL,&error));
    app.projectPath=manifest;
    app.hidden=1; app.noSession=1; app.maxFrames=10000; app.highDpi=1; app.dpiOverride=1.25f; app.settingsPath=settings;
    CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1;
    CHECK(xideEditorOpen(&app,a,&error)); xide_document* cd=app.activeDocument; xui_code_document core=xuiCodeEditGetDocument(cd->editor);
    CHECK(xuiCodeDocumentInsert(core,0,"// local unsaved C\n")==XUI_OK && xideEditorOpen(&app,b,&error)); xide_document* xd=app.activeDocument;
    CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(xd->editor),0,"// local xlang\n")==XUI_OK && pending(&app,0) && writeText(a,disk) && writeText(b,"func external(...) as int\n    return 99\nend\n") && pending(&app,2));
    xideDispatchAction(&app,XIDE_ACTION_FILE_CHANGES); CHECK(xideFileChangesViewIsOpen(&app) && !xideFontViewOpen(&app) && !xideRecoveryViewOpen(&app));
    xideDispatchAction(&app,XIDE_ACTION_NEW_FILE); CHECK(app.activeDocument==xd);
    CHECK(choose(&app,"source.c") && strstr(xuiCodeEditGetText(xideFileChangesViewControl(&app,XIDE_CHANGES_LOCAL)),"local unsaved C") && !strcmp(xuiCodeEditGetText(xideFileChangesViewControl(&app,XIDE_CHANGES_DISK)),disk));
    xui_widget preview=xideFileChangesViewControl(&app,XIDE_CHANGES_DISK); CHECK(xuiCodeEditIsReadonly(preview) && !strcmp(xuiCodeEditGetLanguage(preview),"c") && !xuiSetFocusWidget(app.context,preview) && !xuiInputText(app.context,'Q') && settle(&app) && !strcmp(xuiCodeEditGetText(preview),disk));
    CHECK(capture(&app,image)); uint32_t version=xuiCodeDocumentGetVersion(core);
    CHECK(click(&app,XIDE_CHANGES_RELOAD) && click(&app,XIDE_CHANGES_CANCEL) && cd->dirty && xuiCodeDocumentGetVersion(core)==version && equals(a,disk));
    CHECK(click(&app,XIDE_CHANGES_KEEP) && cd->dirty && xideDocumentCheckExternal(cd) && equals(a,disk) && !xideDocumentSave(cd,NULL,0,&error));
    CHECK(writeText(a,"int newer = 3;\n") && pending(&app,2) && choose(&app,"source.c") && click(&app,XIDE_CHANGES_RELOAD));
    CHECK(writeText(a,"int newest = 4;\n")); uint64_t due=GetTickCount64()+12000; do { CHECK(settle(&app)); if(strstr(xuiCodeEditGetText(preview),"newest")) break; Sleep(10); } while(GetTickCount64()<due);
    CHECK(strstr(xuiCodeEditGetText(preview),"newest") && !xuiWidgetGetVisible(xideFileChangesViewControl(&app,XIDE_CHANGES_CONFIRM)) && cd->dirty);
    CHECK(click(&app,XIDE_CHANGES_RELOAD) && click(&app,XIDE_CHANGES_CONFIRM) && !cd->dirty && !strcmp(xuiCodeEditGetText(cd->editor),"int newest = 4;\n") && equals(a,"int newest = 4;\n"));
    CHECK(xuiCodeDocumentUndo(core)==XUI_OK && cd->dirty && strstr(xuiCodeEditGetText(cd->editor),"local unsaved C")); CHECK(xuiCodeDocumentRedo(core)==XUI_OK && !cd->dirty);
    CHECK(choose(&app,"main.xl") && !strcmp(xuiCodeEditGetLanguage(preview),"xlang") && xideWorkbenchApplyTheme(&app,1)==XUI_OK && capture(&app,dark));
    CHECK(xideUiFontApply(&app,app.uiFontPath,40,&error) && settle(&app) && xuiListViewGetFont(xideFileChangesViewControl(&app,XIDE_CHANGES_LIST))==app.uiFont && xuiCodeEditGetFont(preview)==app.codeFont);
    float cw=0,ch=0; CHECK(!xideDialogGetContentSize(xideFileChangesViewWindow(&app),&cw,&ch) && cw>app.viewportWidth && ch>app.viewportHeight);
    CHECK(click(&app,XIDE_CHANGES_RELOAD) && (xuiWidgetGetStateId(xideFileChangesViewControl(&app,XIDE_CHANGES_RELOAD))&XUI_WIDGET_STATE_DISABLED) && !xuiScrollViewEnsureChildVisible(xideDialogGetScrollViewWidget(xideFileChangesViewWindow(&app)),xideFileChangesViewControl(&app,XIDE_CHANGES_CONFIRM)) && capture(&app,large) && click(&app,XIDE_CHANGES_CANCEL));
    CHECK(!(xuiWidgetGetStateId(xideFileChangesViewControl(&app,XIDE_CHANGES_RELOAD))&XUI_WIDGET_STATE_DISABLED));
    CHECK(xideUiFontApply(&app,app.uiFontPath,20,&error) && xideEditorFontApply(&app,app.codeFontPath,22,&error) && xuiCodeEditGetFont(preview)==app.codeFont && xuiCodeEditGetFont(xideFileChangesViewControl(&app,XIDE_CHANGES_LOCAL))==app.codeFont && xd->dirty);
    CHECK(click(&app,XIDE_CHANGES_CLOSE) && !xideFileChangesViewIsOpen(&app) && xd->dirty && equals(b,"func external(...) as int\n    return 99\nend\n"));
    xui_widget property=findInput(&app,app.properties); CHECK(property && !xuiEditSetText(property,"Native unapplied draft") && xideWorkspaceViewHasDraft(&app));
    value=xideProjectExport(&app.workspaceModel.projects[0],&error); CHECK(value); xvalue* ide=xrtValueClone(xrtValueObjectGet(value,(xstrview){"ide",3})); CHECK(ide && xrtValueObjectSetNew(ide,(xstrview){"name",4},xrtValueString((xstrview){"Foreign model",13})) && xrtValueObjectSetNew(value,(xstrview){"ide",3},ide) && writeValue(manifest,value)); xrtValueRelease(value); value=NULL;
    CHECK(pending(&app,2) && xideFileChangesViewOpen(&app) && choose(&app,"app.xlp") && strstr(xuiCodeEditGetText(xideFileChangesViewControl(&app,XIDE_CHANGES_LOCAL)),"Native unapplied draft"));
    CHECK(!xuiCodeEditSetScroll(xideFileChangesViewControl(&app,XIDE_CHANGES_LOCAL),0,100000));
    CHECK(capture(&app,modelImage) && click(&app,XIDE_CHANGES_RELOAD) && click(&app,XIDE_CHANGES_CANCEL) && xideWorkspaceViewHasDraft(&app) && !strcmp(xuiEditGetText(property),"Native unapplied draft"));
    CHECK(click(&app,XIDE_CHANGES_RELOAD) && !xuiEditSetText(property,"Changed since review") && click(&app,XIDE_CHANGES_CONFIRM) && xideWorkspaceViewHasDraft(&app) && !strcmp(app.workspaceModel.projects[0].name,"Native model"));
    CHECK(click(&app,XIDE_CHANGES_RELOAD) && click(&app,XIDE_CHANGES_CONFIRM) && !xideWorkspaceViewHasDraft(&app) && !strcmp(app.workspaceModel.projects[0].name,"Foreign model") && xd->dirty && xd->open && cd->open);
    CHECK(click(&app,XIDE_CHANGES_CLOSE));
    puts("file_changes_view_test passed: real native rows/buttons and C/xlang read-only comparison, modal isolation, keep/cancel/source-byte preservation, stale disk consent rejection, confirmed reload and undo, two fonts/themes/DPI, 40-DIP two-axis scrolling and four screenshots; actual unapplied property preview/cancel, stale property consent rejection and confirmed model refresh");
cleanup:
    if(began) xgeEnd();
    xideAppDestroy(&app); xideProjectClear(&project); xrtValueRelease(value); free(manifest); free(modelImage); free(root); free(a); free(b); free(settings); free(image); free(dark); free(large); xgeQuit(); return XGE_OK;
}
static DWORD WINAPI worker(void* data)
{ fixture* f=data; if(!SetThreadDesktop(f->desktop)) return 1; xge_desc_t d={0}; d.iWidth=1600; d.iHeight=1000; d.sTitle="xlang IDE disk coordination verification"; d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; d.iRunMode=XGE_RUN_GAME_LOOP; int result=xgeInit(&d); if(result==XGE_OK) result=xgeRun(verify,f); xgeUnit(); return result==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv)
{ if(argc!=2 || !xrtDirCreateAll(argv[1])) return 2; fixture f={.directory=argv[1]}; wchar_t name[100]; swprintf(name,100,L"XlangDiskVerification-%lu",GetCurrentProcessId()); f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) return 1; HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1; if(thread) { if(WaitForSingleObject(thread,90000)!=WAIT_OBJECT_0) { fputs("file_changes_view_test timed out\n",stderr); ExitProcess(1); } GetExitCodeThread(thread,&code); CloseHandle(thread); } CloseDesktop(f.desktop); return (int)code; }
