#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/language_view.h"
#include "ui/editor_view.h"
#include "ui/task_view.h"
#include "services/ui_font.h"
#include "services/file_sync.h"
#include "platform/path.h"
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Compile the real view in this test to replay malformed responses against a
 * checked native batch without adding production-only testing APIs. */
#include "../src/ui/language_view.c"
typedef struct fixture { const char *directory,*compiler; HDESK desktop; int started,failed; } fixture;
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"snapshot_view_test:%d: %s; %s; %s\n",__LINE__,#v,error.message,xideLanguageViewMessage(&app)); f->failed=1; goto cleanup; } } while(0)
static int writeText(const char* p,const char* s) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static int settle(xide_app* a) { if(xuiDispatchPendingEvents(a->context)!=XUI_OK) return 0; xideEditorPoll(a); xideFileSyncPoll(a); xideTaskViewPoll(a); xideLanguageViewPoll(a); xideWorkbenchLayout(a); return xuiLayout(a->context)==XUI_OK && !a->frameError; }
static int waitCheck(xide_app* a) { uint64_t until=GetTickCount64()+15000; do { if(!settle(a)) return 0; if(!xideLanguageViewBusy(a)) return 1; Sleep(5); } while(GetTickCount64()<until); return 0; }
static int capture(xide_app* a,const char* p) { if(!settle(a) || xuiUpdate(a->context,0)!=XUI_OK) return 0; xui_rect_i_t r={0,0,a->viewportWidth,a->viewportHeight}; return a->proxy.surfaceClear(&a->proxy,a->target,0xffeeeeeeu)==XUI_OK && xuiRender(a->context,a->target,&r,1)==XUI_OK && xideAppCapture(a,p)==XGE_OK; }
static int resize(xide_app* a,int w,int h) { xui_surface_desc_t d={0}; d.iKind=XUI_SURFACE_KIND_TEXTURE; d.iWidth=w; d.iHeight=h; d.iFormat=XUI_SURFACE_FORMAT_RGBA8; d.iFlags=XUI_SURFACE_USAGE_TARGET|XUI_SURFACE_ALPHA_PREMULTIPLIED; xui_surface target=NULL; if(a->proxy.surfaceCreate(&a->proxy,&target,&d)!=XUI_OK) return 0; a->proxy.surfaceDestroy(&a->proxy,a->target); a->target=target; a->viewportWidth=w; a->viewportHeight=h; return xuiInputViewport(a->context,(float)w,(float)h)==XUI_OK && settle(a); }
static int menuCheck(xide_app* a) { int index=-1; for(int i=0;i<xuiMenuGetItemCount(a->fileMenu);++i) if(xuiMenuGetItem(a->fileMenu,i)->iValue==XIDE_ACTION_CHECK_CURRENT) index=i; if(index<0 || xuiMenuOpenAt(a->fileMenu,a->root,80,20)!=XUI_OK || !settle(a)) return 0; xui_rect_t r=xuiMenuGetItemRect(a->fileMenu,index),world=xuiWidgetGetWorldRect(a->fileMenu); float x=world.fX+r.fX+r.fW/2,y=world.fY+r.fY+r.fH/2; return xuiInputPointerMove(a->context,x,y,0)==XUI_OK && xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(a); }
static int token(xui_widget editor,const char* word,int kind) { const char* text=xuiCodeEditGetText(editor),*p=strstr(text,word); xui_code_token_t ts[256]; int count=0; if(!p || xuiCodeTokenBufferGetTokens(xuiCodeEditGetTokenBuffer(editor),xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(editor)),ts,256,&count)!=XUI_OK) return 0; for(int i=0;i<count;++i) if(ts[i].iStartOffset==(int)(p-text) && ts[i].iKind==kind) return 1; return 0; }
static int centerBottom(xide_app* a,xide_document* d) { xui_dock_window_info_t editor={.iSize=sizeof(editor)},problem={.iSize=sizeof(problem)}; xui_dock_pane_info_t ep={.iSize=sizeof(ep)},pp={.iSize=sizeof(pp)}; return xuiDockPanelGetWindowInfo(a->dock,d->window,&editor)==XUI_OK && xuiDockPanelGetWindowInfo(a->dock,a->windows[XIDE_PROBLEMS],&problem)==XUI_OK && xuiDockPanelGetPaneInfo(a->dock,editor.iPane,&ep)==XUI_OK && xuiDockPanelGetPaneInfo(a->dock,problem.iPane,&pp)==XUI_OK && ep.tRect.fX==pp.tRect.fX && ep.tRect.fW==pp.tRect.fW && pp.tRect.fY>=ep.tRect.fY+ep.tRect.fH; }
static int killOwnedServer(void)
{
 HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0); if(snapshot==INVALID_HANDLE_VALUE) return 0;
 PROCESSENTRY32W process={.dwSize=sizeof(process)}; int killed=0;
 if(Process32FirstW(snapshot,&process)) do {
  if(process.th32ParentProcessID==GetCurrentProcessId() && !_wcsicmp(process.szExeFile,L"xlang-ls.exe")) {
   HANDLE owned=OpenProcess(PROCESS_TERMINATE|SYNCHRONIZE,FALSE,process.th32ProcessID); if(owned) { killed=TerminateProcess(owned,77) && WaitForSingleObject(owned,2000)==WAIT_OBJECT_0; CloseHandle(owned); } break;
  }
 } while(Process32NextW(snapshot,&process)); CloseHandle(snapshot); return killed;
}
static int replace(xide_document* d,const char* text) { xui_code_document core=xuiCodeEditGetDocument(d->editor); return xuiCodeDocumentReplace(core,0,xuiCodeDocumentGetLength(core),text)==XUI_OK; }
static int ready(xide_app* a,size_t expected) { uint64_t until=GetTickCount64()+20000; do { if(!settle(a)) return 0; if(!xideLanguageViewBusy(a) && xideLanguageViewDiagnosticCount(a)==expected && strstr(xideLanguageViewMessage(a),"快照检查：")) return 1; Sleep(5); } while(GetTickCount64()<until); return 0; }
static int idle(xide_app* a,int milliseconds) { uint64_t until=GetTickCount64()+(unsigned)milliseconds; do { if(!settle(a) || xideLanguageViewBusy(a) || xideLanguageViewDiagnosticCount(a)) return 0; Sleep(5); } while(GetTickCount64()<until); return 1; }
static int problemEnter(xide_app* a) { return xuiTreeViewSetSelected(a->panels[XIDE_PROBLEMS],1)==XUI_OK && xuiSetFocusWidget(a->context,a->panels[XIDE_PROBLEMS])==XUI_OK && xuiInputKeyDown(a->context,XUI_KEY_ENTER,0)==XUI_OK && xuiInputKeyUp(a->context,XUI_KEY_ENTER,0)==XUI_OK && settle(a); }
static int rejectMalformedSnapshot(xide_app* app,int mode)
{
 xide_language_view* view=app->languageView; xvalue* live=view->snapshotResult;
 xvalue* altered=xrtValueDeepClone(live); if(!altered) return 0;
 xvalue* groups=get(altered,"documents"); int okay=0;
 if(mode==0) {
  xvalue* list=NULL;
  for(size_t i=0;i<xrtValueCount(groups);++i) { xvalue* candidate=get(xrtValueArrayGet(groups,i),"diagnostics"); if(xrtValueCount(candidate)) { list=candidate; break; } }
  okay=list && put(altered,"count",xrtValueInt(0)) && xrtValueArrayAppend(list,xrtValueArrayGet(list,0));
 } else if(mode==1) okay=put(altered,"count",xrtValueInt(2));
 else if(mode==2) okay=xrtValueArrayAppend(groups,xrtValueArrayGet(groups,0));
 else if(mode==3) okay=put(xrtValueArrayGet(groups,0),"normalizedSha256",xrtValueString(sv("0000000000000000000000000000000000000000000000000000000000000000")));
 if(okay) okay=!applySnapshots(app,altered) && view->snapshotResult==live && xideLanguageViewDiagnosticCount(app)==1;
 xrtValueRelease(altered); return okay;
}
static int rechecked(xide_app* app,int oldId,size_t expected)
{ uint64_t until=GetTickCount64()+10000; do { if(!settle(app)) return 0; int id=0; if(app->languageView->snapshotResult && number(get(app->languageView->snapshotResult,"checkId"),&id) && id!=oldId && !xideLanguageViewBusy(app) && xideLanguageViewDiagnosticCount(app)==expected) return 1; Sleep(5); } while(GetTickCount64()<until); return 0; }
static int staleInvalidationIgnored(xide_app* app)
{
 xide_language_view* v=app->languageView; xvalue* before=v->snapshotResult; document_binding* root=NULL;
 for(size_t i=0;i<v->inputCount;++i) if(v->inputs[i].document==v->document) root=&v->inputs[i];
 if(!root || !before) return 0;
 int id=0; if(!number(get(before,"checkId"),&id)) return 0;
 xvalue* p=xrtValueObject(); put(p,"checkId",xrtValueInt(id-1)); put(p,"uri",xrtValueString(sv(root->uri))); put(p,"version",xrtValueInt(root->wireVersion)); publishedInvalidation(app,p);
 int okay=v->snapshotResult==before && xideLanguageViewDiagnosticCount(app)==1;
 put(p,"checkId",xrtValueInt(id)); put(p,"version",xrtValueInt(root->wireVersion+1)); publishedInvalidation(app,p);
 okay=okay && v->snapshotResult==before && xideLanguageViewDiagnosticCount(app)==1; xrtValueRelease(p); return okay;
}
static int verify(void* user)
{
 fixture* f=user; if(f->started) return XGE_OK; f->started=1; xide_app app={0}; xide_model_error error={0}; int began=0; char* owned[24]={0}; size_t ownedCount=0; xide_text_file baseline={0};
 #define KEEP(s) (owned[ownedCount++]=(s))
 char unique[90]; snprintf(unique,sizeof(unique),"native-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64());
 char *root=KEEP(xidePathResolve(f->directory,unique)),*entry=KEEP(xidePathResolve(root,"入口😀.xl")),*dep=KEEP(xidePathResolve(root,"dep.xl")),*native=KEEP(xidePathResolve(root,"helper.c")),*disk=KEEP(xidePathResolve(root,"disk.xl")),*virtualPath=KEEP(xidePathResolve(root,"virtual.xl")),*moved=KEEP(xidePathResolve(root,"改名.xl")),*settings=KEEP(xidePathResolve(root,"settings.json"));
 char *light=KEEP(xidePathResolve(root,"native-snapshot-light.png")),*dark=KEEP(xidePathResolve(root,"native-snapshot-dark.png")),*large=KEEP(xidePathResolve(root,"native-snapshot-large.png")),*diskImage=KEEP(xidePathResolve(root,"native-snapshot-disk.png"));
 const char *saved="func main(...) as int\n    return 0\nend\n",*mainText="import dep\nfunc main(...) as int\n    return dep.value()\nend\n",*good="export func value() as int\n    return 7\nend\n",*bad="export func value() as int\n    return \"bad\"\nend\n",*unicode="export func value() as int\n    string 中文 = \"😀\"; int x = missingName\n    return 7\nend\n";
 const char *nativeText="int helper(void) { return 42; }\n"; xide_file_snapshot before[3],after[3];
 CHECK(root && entry && dep && native && xrtDirCreateAll(root) && writeText(entry,saved) && writeText(dep,good) && writeText(native,nativeText));
 CHECK(xideFileSnapshotInspect(entry,&before[0],&error) && xideFileSnapshotInspect(dep,&before[1],&error) && xideFileSnapshotInspect(native,&before[2],&error));
 app.hidden=1; app.noSession=1; app.highDpi=1; app.dpiOverride=1.25f; app.compilerPath=f->compiler; app.settingsPath=settings; CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1;
 CHECK(xideActionCount()==71 && xideActionNamed("language.check")->id==68 && !xideActionQuery(&app,XIDE_ACTION_CHECK_CURRENT).enabled);
 CHECK(xideEditorOpen(&app,entry,&error) && xideEditorOpen(&app,dep,&error) && xideEditorOpen(&app,native,&error) && settle(&app));
 xide_document *xl=xideDocumentsFind(&app.documents,entry),*dependency=xideDocumentsFind(&app.documents,dep),*c=app.activeDocument;
 CHECK(xl && dependency && c && token(xl->editor,"func",XUI_CODE_TOKEN_KEYWORD) && token(c->editor,"return",XUI_CODE_TOKEN_KEYWORD) && !xideActionQuery(&app,XIDE_ACTION_CHECK_CURRENT).enabled);
 CHECK(replace(xl,mainText) && replace(dependency,unicode) && xideEditorShow(&app,xl)==XUI_OK && settle(&app) && xl->dirty && dependency->dirty && xideActionQuery(&app,XIDE_ACTION_CHECK_CURRENT).enabled && !xideActionQuery(&app,XIDE_ACTION_CHECK_SAVED).enabled);
 uint32_t xv=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xl->editor)),dv=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(dependency->editor)),cv=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(c->editor));
 CHECK(menuCheck(&app) && ready(&app,1) && centerBottom(&app,xl));
 for(int invalid=0;invalid<4;++invalid) CHECK(rejectMalformedSnapshot(&app,invalid));
 xui_code_diagnostic_t diagnostic={.iSize=sizeof(diagnostic)};
 CHECK(!xuiCodeAnnotationGetDiagnosticCount(xuiCodeEditGetAnnotations(xl->editor)) && xuiCodeAnnotationGetDiagnosticCount(xuiCodeEditGetAnnotations(dependency->editor))==1 && xuiCodeAnnotationGetDiagnostic(xuiCodeEditGetAnnotations(dependency->editor),0,&diagnostic)==XUI_OK && strstr(diagnostic.sMessage,"missingName"));
 int expected=(int)(strstr(unicode,"missingName")-unicode); CHECK(diagnostic.tRange.iStart==expected && problemEnter(&app) && app.activeDocument==dependency);
 int start=0,end=0; CHECK(xuiCodeSelectionGetRange(xuiCodeEditGetSelection(dependency->editor),&start,&end)==XUI_OK && start==expected && end==expected && xuiGetFocusWidget(app.context)==dependency->editor);
 CHECK(xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xl->editor))==xv && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(dependency->editor))==dv && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(c->editor))==cv && xl->dirty && dependency->dirty && !c->dirty && capture(&app,light));
 CHECK(xideWorkbenchApplyTheme(&app,1)==XUI_OK && capture(&app,dark));
 CHECK(xideUiFontApply(&app,app.uiFontPath,24,&error) && resize(&app,900,600) && capture(&app,large) && xideUiFontApply(&app,app.uiFontPath,16,&error) && resize(&app,1280,800));
 /* Unrelated C changes preserve the xlang result; automatic recheck keeps its original root. */
 CHECK(xideEditorShow(&app,c)==XUI_OK && xuiCodeDocumentInsert(xuiCodeEditGetDocument(c->editor),xuiCodeDocumentGetLength(xuiCodeEditGetDocument(c->editor)),"//编辑😀\n")==XUI_OK && settle(&app) && xideLanguageViewDiagnosticCount(&app)==1 && replace(dependency,good) && settle(&app) && !xideLanguageViewDiagnosticCount(&app) && ready(&app,0) && app.activeDocument==c);
 CHECK(replace(dependency,bad) && settle(&app) && ready(&app,1));
 /* Close/reuse of an unsaved virtual dependency clears its borrowed annotations. */
 CHECK(xideTextFileEncode(good,(int)strlen(good),0,XUI_CODE_EOL_LF,&baseline,&error));
 xide_document* virtualDoc=xideDocumentRecover(&app.documents,virtualPath,"virtual.xl","xlang",&baseline,bad,&error);
 CHECK(virtualDoc && !xidePathExists(virtualPath) && replace(xl,"import dep\nimport virtual\nfunc main(...) as int\n    return dep.value()+virtual.value()\nend\n") && settle(&app) && ready(&app,2));
 CHECK(xuiCodeAnnotationGetDiagnosticCount(xuiCodeEditGetAnnotations(dependency->editor))==1 && xuiCodeAnnotationGetDiagnosticCount(xuiCodeEditGetAnnotations(virtualDoc->editor))==1);
 CHECK(replace(xl,mainText) && xideDocumentClose(virtualDoc,1,&error) && settle(&app) && ready(&app,1));
 xide_document* reused=xideDocumentNew(&app.documents,"c",&error); CHECK(reused==virtualDoc && !xuiCodeAnnotationGetDiagnosticCount(xuiCodeEditGetAnnotations(reused->editor)) && xideDocumentClose(reused,1,&error));
 CHECK(xideEditorShow(&app,xl)==XUI_OK && xideActionExecute(&app,XIDE_ACTION_CHECK_CURRENT)==XIDE_ACTION_DISPATCHED && xideActionExecute(&app,XIDE_ACTION_CANCEL_CHECK)==XIDE_ACTION_DISPATCHED && waitCheck(&app) && replace(dependency,good) && idle(&app,800));
 CHECK(menuCheck(&app) && ready(&app,0) && xideActionQuery(&app,XIDE_ACTION_CANCEL_CHECK).enabled && xideActionExecute(&app,XIDE_ACTION_CANCEL_CHECK)==XIDE_ACTION_DISPATCHED && !xideActionQuery(&app,XIDE_ACTION_CANCEL_CHECK).enabled && idle(&app,700) && menuCheck(&app) && ready(&app,0));
 /* Multiple rapid edits postpone checking until the last edit has settled. */
 CHECK(replace(dependency,bad) && settle(&app)); Sleep(300); CHECK(replace(dependency,good) && settle(&app)); Sleep(300); CHECK(settle(&app) && !xideLanguageViewBusy(&app) && ready(&app,0));
 CHECK(replace(dependency,bad) && settle(&app) && ready(&app,1) && xideDocumentRename(xl,moved,&error) && settle(&app) && !xideLanguageViewDiagnosticCount(&app) && ready(&app,1) && xuiCodeDocumentCanUndo(xuiCodeEditGetDocument(xl->editor)));
 CHECK(killOwnedServer()); uint64_t until=GetTickCount64()+5000; do { CHECK(settle(&app)); if(!xideLanguageViewDiagnosticCount(&app)) break; Sleep(5); } while(GetTickCount64()<until);
 CHECK(!xideLanguageViewDiagnosticCount(&app) && strstr(xideLanguageViewMessage(&app),"已结束") && menuCheck(&app) && ready(&app,1));
 /* An unopened BOM/CRLF file receives diagnostics in its own coordinates. */
 char raw[1024]; size_t at=3; memcpy(raw,"\xef\xbb\xbf",3); for(const char* p=unicode;*p;++p) { if(*p=='\n') raw[at++]='\r'; raw[at++]=*p; } raw[at]=0;
 CHECK(writeText(disk,raw) && replace(xl,"import disk\nfunc main(...) as int\n    return disk.value()\nend\n") && replace(dependency,good) && settle(&app) && ready(&app,1) && !xideDocumentsFind(&app.documents,disk));
 CHECK(problemEnter(&app) && app.activeDocument && xidePathLexicalEqual(app.activeDocument->path,disk)); xide_document* diskDoc=app.activeDocument;
 CHECK(xuiCodeSelectionGetRange(xuiCodeEditGetSelection(diskDoc->editor),&start,&end)==XUI_OK && start==expected && end==expected && ready(&app,1) && xuiCodeAnnotationGetDiagnosticCount(xuiCodeEditGetAnnotations(diskDoc->editor))==1 && capture(&app,diskImage));
 CHECK(xideEditorClose(&app,diskDoc,0,&error) && settle(&app) && ready(&app,1));
 CHECK(writeText(disk,raw) && !xideTaskViewJump(&app,0) && !xideDocumentsFind(&app.documents,disk) && !xideLanguageViewDiagnosticCount(&app) && ready(&app,1));
 /* The unopened disk input is invalidated and rechecked without a jump/save. */
 CHECK(staleInvalidationIgnored(&app) && writeText(disk,good) && ready(&app,0));
 CHECK(writeText(disk,raw) && ready(&app,1)); int oldCheck=0; CHECK(number(get(app.languageView->snapshotResult,"checkId"),&oldCheck));
 CHECK(writeText(disk,raw) && rechecked(&app,oldCheck,1) && !xideDocumentsFind(&app.documents,disk));
 /* Exclusive writer lock: revoke old results, fail a recheck, then recover
    automatically after the same input becomes readable again. */
 wchar_t lockedPath[32768]; CHECK(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,disk,-1,lockedPath,32768));
 HANDLE locked=CreateFileW(lockedPath,GENERIC_READ,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL); CHECK(locked!=INVALID_HANDLE_VALUE);
 uint64_t lockUntil=GetTickCount64()+8000; do { CHECK(settle(&app)); if(app.languageView->retryFailures>0 && !xideLanguageViewDiagnosticCount(&app) && !xideLanguageViewBusy(&app)) break; Sleep(5); } while(GetTickCount64()<lockUntil);
 int retried=app.languageView->retryFailures>0 && !xideLanguageViewDiagnosticCount(&app); CHECK(CloseHandle(locked) && retried && ready(&app,1));
 /* GUI process environment changes restart the service/full document batch. */
 char* originalPackage=NULL; CHECK(xideLanguageCheckPackagePath(&originalPackage,&error)); KEEP(originalPackage);
 CHECK(SetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",L"XIDE_GUI_GUARD_ENV_TEST") && settle(&app) && !xideLanguageViewDiagnosticCount(&app) && ready(&app,1));
 if(originalPackage) { int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,originalPackage,-1,NULL,0); wchar_t* w=n ? malloc((size_t)n*sizeof(*w)) : NULL; CHECK(w && MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,originalPackage,-1,w,n) && SetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",w)); free(w); }
 else CHECK(SetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",NULL));
 CHECK(settle(&app) && !xideLanguageViewDiagnosticCount(&app) && ready(&app,1));
 CHECK(xideActionExecute(&app,XIDE_ACTION_CANCEL_CHECK)==XIDE_ACTION_DISPATCHED && idle(&app,100) && writeText(disk,good) && idle(&app,1400));
 CHECK(writeText(disk,raw) && xideEditorShow(&app,xl)==XUI_OK && menuCheck(&app) && ready(&app,1));
 /* The actual file coordinator observes an external writer while local edits are dirty. */
 CHECK(xideFileSnapshotInspect(moved,&after[0],&error) && xideFileSnapshotInspect(dep,&after[1],&error) && xideFileSnapshotInspect(native,&after[2],&error) && xideFileSnapshotEqual(&before[0],&after[0]) && xideFileSnapshotEqual(&before[1],&after[1]) && xideFileSnapshotEqual(&before[2],&after[2]));
 CHECK(replace(dependency,bad) && settle(&app) && ready(&app,1) && writeText(dep,"//external writer\nexport func value() as int\n    return 9\nend\n"));
 until=GetTickCount64()+5000; do { CHECK(settle(&app)); if(dependency->externalState==XIDE_WATCH_READY) break; Sleep(5); } while(GetTickCount64()<until);
 printf("conflict observation: state=%d dirty=%d bodyPreserved=%d diagnostics=%zu actionEnabled=%d\n",dependency->externalState,dependency->dirty,!strcmp(xuiCodeEditGetText(dependency->editor),bad),xideLanguageViewDiagnosticCount(&app),xideActionQuery(&app,XIDE_ACTION_CHECK_CURRENT).enabled);
 CHECK(dependency->externalState==XIDE_WATCH_READY && dependency->dirty && !strcmp(xuiCodeEditGetText(dependency->editor),bad) && !xideLanguageViewDiagnosticCount(&app) && xideEditorShow(&app,xl)==XUI_OK && !xideActionQuery(&app,XIDE_ACTION_CHECK_CURRENT).enabled);
 CHECK(writeText(dep,good)); until=GetTickCount64()+5000; do { CHECK(settle(&app)); if(!dependency->externalState) break; Sleep(5); } while(GetTickCount64()<until);
 CHECK(!dependency->externalState && ready(&app,1) && xideFileSnapshotInspect(dep,&after[1],&error) && !memcmp(before[1].sha256,after[1].sha256,32)); before[1]=after[1];
 CHECK(xideFileSnapshotInspect(moved,&after[0],&error) && xideFileSnapshotInspect(dep,&after[1],&error) && xideFileSnapshotInspect(native,&after[2],&error) && xideFileSnapshotEqual(&before[0],&after[0]) && xideFileSnapshotEqual(&before[1],&after[1]) && xideFileSnapshotEqual(&before[2],&after[2]) && !xidePathExists(virtualPath));
 /* A complete 64-document batch can be checked twice without dropping lifecycle messages. */
 xide_document* extras[63]={0};
 for(int i=0;i<63;++i) {
  char leaf[48]; snprintf(leaf,sizeof(leaf),"unused-%d.xl",i); char* name=xidePathResolve(root,leaf);
  CHECK(name && xideTextFileEncode("",0,0,XUI_CODE_EOL_LF,&baseline,&error)); extras[i]=xideDocumentRecover(&app.documents,name,leaf,"xlang",&baseline,good,&error); free(name); CHECK(extras[i]);
  if(i==61) { CHECK(xideEditorShow(&app,xl)==XUI_OK && settle(&app) && xideActionQuery(&app,XIDE_ACTION_CHECK_CURRENT).enabled && menuCheck(&app) && ready(&app,1) && strstr(xideLanguageViewMessage(&app),"64 份打开文档") && menuCheck(&app) && ready(&app,1)); }
 }
 CHECK(settle(&app) && !xideLanguageViewDiagnosticCount(&app) && !xideActionQuery(&app,XIDE_ACTION_CHECK_CURRENT).enabled);
 for(int i=0;i<63;++i) CHECK(xideDocumentClose(extras[i],1,&error));
 CHECK(settle(&app) && ready(&app,1));
 CHECK(xideEditorClose(&app,xl,1,&error) && settle(&app) && !xideLanguageViewDiagnosticCount(&app)); reused=xideDocumentNew(&app.documents,"xlang",&error); CHECK(reused==xl && !xuiCodeAnnotationGetDiagnosticCount(xuiCodeEditGetAnnotations(reused->editor)) && xideEditorShow(&app,reused)==XUI_OK && !xideActionQuery(&app,XIDE_ACTION_CHECK_CURRENT).enabled && idle(&app,700));
 printf("snapshot_view_test passed: actual current menu, dirty recursive and virtual inputs, per-file Unicode annotations/problem Enter, malformed count/group/hash rejection before UI mutation, C/xlang syntax and layout, checked-root debounce, published automatic cancel, 64-document resync/65-document bound, rename/close/reuse, real server crash/full resync, post-publication disk/identity automatic recheck, stale notification rejection, package-environment restart, unreadable-input backoff/recovery, cancellation teardown, fingerprint guarded BOM/CRLF disk jump and unchanged source bytes/post-writer baselines; renders in %s.\n",root);
cleanup:
 xideTextFileClear(&baseline); if(began) xgeEnd(); xideAppDestroy(&app); for(size_t i=0;i<ownedCount;++i) free(owned[i]); xgeQuit(); return XGE_OK;
}
static DWORD WINAPI worker(void* data) { fixture* f=data; if(!SetThreadDesktop(f->desktop)) return 1; xge_desc_t d={0}; d.iWidth=1600; d.iHeight=1000; d.sTitle="xlang snapshot view verification"; d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; d.iRunMode=XGE_RUN_GAME_LOOP; int result=xgeInit(&d); if(result==XGE_OK) result=xgeRun(verify,f); xgeUnit(); return result==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv) { if(argc!=3 || !xrtDirCreateAll(argv[1])) return 2; fixture f={.directory=argv[1],.compiler=argv[2]}; wchar_t name[100]; swprintf(name,100,L"XlangSnapshotVerification-%lu",GetCurrentProcessId()); f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) return 1; HANDLE t=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1; if(t) { if(WaitForSingleObject(t,120000)!=WAIT_OBJECT_0) { fputs("snapshot_view_test timed out\n",stderr); ExitProcess(1); } GetExitCodeThread(t,&code); CloseHandle(t); } CloseDesktop(f.desktop); return (int)code; }
