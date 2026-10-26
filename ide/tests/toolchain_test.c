#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/toolchain_view.h"
#include "ui/workspace_view.h"
#include "ui/editor_view.h"
#include "ui/task_view.h"
#include "ui/references_view.h"
#include "ui/resource_view.h"
#include "ui/font_view.h"
#include "ui/import_view.h"
#include "ui/template_view.h"
#include "ui/configuration_view.h"
#include "ui/recovery_view.h"
#include "ui/file_changes_view.h"
#include "services/ui_font.h"
#include "services/file_sync.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct fixture { const char *directory,*compiler; HDESK desktop; int started,failed; } fixture;
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"toolchain_test:%d: %s; %s; %s\n",__LINE__,#v,error.message,xideToolchainViewMessage(&app)); f->failed=1; goto cleanup; } } while(0)
static int writeText(const char* p,const char* s) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static char* copy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static wchar_t* wide(const char* s) { int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,NULL,0); wchar_t* w=n ? malloc((size_t)n*sizeof(*w)) : NULL; if(w) MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,w,n); return w; }
static int attributes(const char* p,DWORD value) { wchar_t* w=wide(p); int okay=w && SetFileAttributesW(w,value); free(w); return okay; }
static int removeFile(const char* p) { wchar_t* w=wide(p); int okay=w && DeleteFileW(w); free(w); return okay; }
static int counter(const char* p) { size_t n=0; char* s=(char*)xrtFileReadAll(p,&n); int result=s ? atoi(s) : 0; xrtFree(s); return result; }
static int request(xide_toolchains* cache,const char* compiler,const char* directory,int force,xide_toolchain_profile* p,xide_model_error* error)
{
 xide_toolchain_request* q=xideToolchainRequest(cache,compiler,directory,force,error); if(!q) return 0; uint64_t until=GetTickCount64()+25000;
 enum xide_toolchain_request_state state;
 do { state=xideToolchainRequestState(q,error->message,sizeof(error->message)); if(state!=XIDE_TOOL_QUERY_RUNNING) break; Sleep(5); } while(GetTickCount64()<until);
 int okay=state==XIDE_TOOL_QUERY_READY && xideToolchainRequestResult(q,p); xideToolchainRequestDestroy(q); return okay;
}
static int settle(xide_app* a) { if(xuiDispatchPendingEvents(a->context)!=XUI_OK) return 0; xideEditorPoll(a); xideFileSyncPoll(a); xideTaskViewPoll(a); xideToolchainViewPoll(a); xideWorkbenchLayout(a); return xuiLayout(a->context)==XUI_OK && !a->frameError; }
static int pointer(xide_app* a,xui_rect_t r) { float x=r.fX+r.fW/2,y=r.fY+r.fH/2; return xuiInputPointerMove(a->context,x,y,0)==XUI_OK && xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(a); }
static int click(xide_app* a,int id)
{
 xui_widget w=xideToolchainViewControl(a,id),scroll=xideDialogGetScrollViewWidget(xideToolchainViewWindow(a)); if(xuiScrollViewEnsureChildVisible(scroll,w)!=XUI_OK || !settle(a)) return 0;
 xui_rect_t r=xuiWidgetGetWorldRect(w),visible=xuiWidgetGetWorldRect(scroll); float right=r.fX+r.fW,bottom=r.fY+r.fH;
 if(r.fX<visible.fX) r.fX=visible.fX;
 if(r.fY<visible.fY) r.fY=visible.fY;
 if(right>visible.fX+visible.fW) right=visible.fX+visible.fW;
 if(bottom>visible.fY+visible.fH) bottom=visible.fY+visible.fH;
 r.fW=right-r.fX; r.fH=bottom-r.fY; return r.fW>0 && r.fH>0 && pointer(a,r);
}
static int capture(xide_app* a,const char* path) { if(!settle(a) || xuiUpdate(a->context,0)!=XUI_OK) return 0; xui_rect_i_t r={0,0,a->viewportWidth,a->viewportHeight}; return a->proxy.surfaceClear(&a->proxy,a->target,0xffeeeeeeu)==XUI_OK && xuiRender(a->context,a->target,&r,1)==XUI_OK && xideAppCapture(a,path)==XGE_OK; }
static int resize(xide_app* a,int width,int height)
{ xui_surface_desc_t d={0}; d.iKind=XUI_SURFACE_KIND_TEXTURE; d.iWidth=width; d.iHeight=height; d.iFormat=XUI_SURFACE_FORMAT_RGBA8; d.iFlags=XUI_SURFACE_USAGE_TARGET|XUI_SURFACE_ALPHA_PREMULTIPLIED; xui_surface next=NULL; if(a->proxy.surfaceCreate(&a->proxy,&next,&d)!=XUI_OK) return 0; a->proxy.surfaceDestroy(&a->proxy,a->target); a->target=next; a->viewportWidth=width; a->viewportHeight=height; return xuiInputViewport(a->context,(float)width,(float)height)==XUI_OK && settle(a); }
static int token(xui_widget editor,const char* word,int kind)
{
 const char* source=xuiCodeEditGetText(editor); const char* p=strstr(source,word); if(!p) return 0;
 xui_code_token_t ts[256]; int count=0; if(xuiCodeTokenBufferGetTokens(xuiCodeEditGetTokenBuffer(editor),xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(editor)),ts,256,&count)!=XUI_OK) return 0;
 for(int i=0;i<count;++i) if(ts[i].iStartOffset==(int)(p-source) && ts[i].iKind==kind) return 1;
 return 0;
}
static int waitView(xide_app* a) { uint64_t until=GetTickCount64()+25000; do { if(!settle(a)) return 0; if(!xideToolchainViewBusy(a)) return 1; Sleep(5); } while(GetTickCount64()<until); return 0; }
static int waitTask(xide_tasks* tasks,xide_task* task,xide_task_info* info) { uint64_t until=GetTickCount64()+25000; do { xideTasksPoll(tasks); xideTaskInfo(task,info); if(info->state>=XIDE_TASK_SUCCEEDED) return 1; Sleep(5); } while(GetTickCount64()<until); return 0; }
static int menuOpen(xide_app* a)
{
 int i=-1; for(int j=0;j<xuiMenuGetItemCount(a->fileMenu);++j) if(xuiMenuGetItem(a->fileMenu,j)->iValue==XIDE_ACTION_TOOLCHAIN_INFO) i=j;
 if(i<0 || xuiMenuOpenAt(a->fileMenu,a->root,80,40)!=XUI_OK || !settle(a)) return 0;
 xui_rect_t r=xuiMenuGetItemRect(a->fileMenu,i),world=xuiWidgetGetWorldRect(a->fileMenu); r.fX+=world.fX; r.fY+=world.fY; return pointer(a,r);
}
static int sameBytes(const char* path,const char* expected) { size_t n=0; char* s=(char*)xrtFileReadAll(path,&n); int okay=s && n==strlen(expected) && !memcmp(s,expected,n); xrtFree(s); return okay; }
static int verify(void* user)
{
 fixture* f=user; if(f->started) return XGE_OK; f->started=1; xide_app app={0}; xide_model_error error={0}; int began=0;
 xide_toolchains* cache=NULL; xide_toolchain_request* pending=NULL; xide_toolchain_profile p={0},saved={0}; xide_project project={0}; xide_tasks* tasks=NULL; xide_task_info info={0}; char* owned[80]={0}; size_t ownedCount=0; xvalue* baseline=NULL;
 #define KEEP(s) (owned[ownedCount++]=(s))
 char unique[100]; snprintf(unique,sizeof(unique),"toolchain-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64());
 char *root=KEEP(xidePathResolve(f->directory,unique)),*tools=KEEP(xidePathResolve(root,"工具链 😀")),*other=KEEP(xidePathResolve(root,"另一工作目录"));
 char *compiler=KEEP(xidePathResolve(tools,"xl.exe")),*runtime=KEEP(xidePathResolve(tools,"xl.dll")),*frontend=KEEP(xidePathResolve(tools,"xlc.dll"));
 char helper[32768]; wchar_t executable[32768]; CHECK(GetModuleFileNameW(NULL,executable,32768));
 wchar_t* slash=wcsrchr(executable,L'\\'); CHECK(slash); wcscpy(slash+1,L"xide_toolchain_program.exe"); CHECK(WideCharToMultiByte(CP_UTF8,0,executable,-1,helper,sizeof(helper),NULL,NULL));
 char *mode=KEEP(xidePathResolve(root,"profile-mode.txt")),*count=KEEP(xidePathResolve(root,"profile-count.txt")),*signal=KEEP(xidePathResolve(root,"profile-signal.txt")),*buildCount=KEEP(xidePathResolve(root,"profile-builds.txt"));
 char *entry=KEEP(xidePathResolve(root,"main.xl")),*native=KEEP(xidePathResolve(root,"native.c")),*manifest=KEEP(xidePathResolve(root,"项目.xlp")),*settings=KEEP(xidePathResolve(root,"settings.json")),*sessions=KEEP(xidePathResolve(root,"sessions"));
 char *image=KEEP(xidePathResolve(root,"native-toolchain-info.png")),*imageFailed=KEEP(xidePathResolve(root,"native-toolchain-failed.png")),*imageLarge=KEEP(xidePathResolve(root,"native-toolchain-large-font.png")),*imageReal=KEEP(xidePathResolve(root,"native-toolchain-real.png"));
 CHECK(root && tools && other && compiler && mode && count && xrtDirCreateAll(tools) && xrtDirCreateAll(other) && xrtFileCopy(helper,compiler,true) && writeText(mode,"normal"));
 uint32_t commands=123,options=456;
 const char* scoped="This compiler does not support build -o.\nxl run <file> --link lib --res config\nxl build <file> -o out --linking bad\nxl check <file> --json\nxl abi <file>\n";
 CHECK(xideToolchainParseHelp(scoped,strlen(scoped),&commands,&options,&error) && commands==(XIDE_TOOL_BUILD|XIDE_TOOL_RUN|XIDE_TOOL_CHECK|XIDE_TOOL_ABI) && options==(XIDE_TOOL_OUTPUT|XIDE_TOOL_CHECK_JSON));
 CHECK(!xideToolchainParseHelp("a\0b",3,&commands,&options,&error) && commands==(XIDE_TOOL_BUILD|XIDE_TOOL_RUN|XIDE_TOOL_CHECK|XIDE_TOOL_ABI));
 CHECK(!xideToolchainParseHelp("\xff",1,&commands,&options,&error) && !xideToolchainParseHelp("",0,&commands,&options,&error));
 cache=xideToolchainsCreate(); CHECK(cache && request(cache,compiler,root,0,&p,&error) && !p.cached && !strcmp(p.fileVersion,"1.2.3.4") && p.commands==511 && p.options==2047 && p.stamp.present[0] && !p.stamp.present[1] && !p.stamp.present[2] && counter(count)==1);
 CHECK(xideToolchainProfileCopy(&saved,&p) && saved.compiler!=p.compiler && saved.help!=p.help && request(cache,compiler,root,0,&p,&error) && p.cached && counter(count)==1 && request(cache,compiler,root,1,&p,&error) && !p.cached && counter(count)==2);
 CHECK(request(cache,compiler,other,0,&p,&error) && !p.cached && xidePathEqual(p.directory,other));
 CHECK(writeText(frontend,"frontend") && request(cache,compiler,root,0,&p,&error) && !p.cached && p.stamp.present[1] && counter(count)==3);
 CHECK(writeText(runtime,"same bytes") && request(cache,compiler,root,0,&p,&error) && !p.cached && p.stamp.present[2] && counter(count)==4); xide_toolchain_stamp oldStamp=p.stamp;
 CHECK(writeText(runtime,"same bytes") && !xideToolchainValidate(compiler,&oldStamp,&error) && request(cache,compiler,root,0,&p,&error) && !p.cached && counter(count)==5);
 CHECK(removeFile(runtime) && request(cache,compiler,root,0,&p,&error) && !p.cached && !p.stamp.present[2] && counter(count)==6);
 CHECK(attributes(compiler,FILE_ATTRIBUTE_READONLY) && request(cache,compiler,root,0,&p,&error) && !p.cached && !strcmp(p.fileVersion,"1.2.3.4")); xide_file_snapshot readOnly={0}; CHECK(xideFileSnapshotInspect(compiler,&readOnly,&error) && !xideFileSnapshotRead(compiler,&readOnly,&error) && attributes(compiler,FILE_ATTRIBUTE_NORMAL));
 CHECK(xideToolchainProfileCopy(&saved,&p) && writeText(mode,"nonzero") && !request(cache,compiler,root,1,&p,&error) && !strcmp(p.help,saved.help));
 const char* bad[]={"oversize","invalid","nul","empty"}; for(unsigned i=0;i<4;++i) CHECK(writeText(mode,bad[i]) && !request(cache,compiler,root,1,&p,&error));
 CHECK(writeText(mode,"change") && !request(cache,compiler,root,1,&p,&error) && strstr(error.message,"changed"));
 CHECK(writeText(mode,"slow") && removeFile(signal)); pending=xideToolchainRequest(cache,compiler,root,1,&error); CHECK(pending);
 uint64_t until=GetTickCount64()+5000; while(!xidePathExists(signal) && GetTickCount64()<until) Sleep(5); CHECK(xidePathExists(signal)); uint64_t cancelled=GetTickCount64(); xideToolchainRequestCancel(pending); xideToolchainRequestDestroy(pending); pending=NULL; CHECK(GetTickCount64()-cancelled<2000);
 CHECK(!request(cache,compiler,root,1,&p,&error) && strstr(error.message,"15 second"));
 CHECK(writeText(mode,"normal") && writeText(entry,"extern \"native_answer\" func nativeAnswer() as c_int\nfunc main(...) as int\n    return nativeAnswer()-42\nend\n") && writeText(native,"int native_answer(void) { return 42; }\n") && xideProjectCreate(&project,manifest,"工具链与混编验收😀",entry,&error) && xideProjectAddFile(&project,native,"c",&error) && xideProjectSave(&project,NULL,&error));
 tasks=xideTasksCreate(); CHECK(tasks); char* output=KEEP(copy(project.configurations[0].output)); CHECK(output && xrtDirCreateAll(KEEP(xidePathDirectory(output))) && writeText(output,"previous artifact"));
 CHECK(request(xideTasksToolchains(tasks),compiler,root,1,&p,&error) && writeText(mode,"nul")); int before=counter(count);
 xide_task* t=xideTasksBuild(tasks,&project,NULL,compiler,1,&error); CHECK(t && waitTask(tasks,t,&info) && info.state==XIDE_TASK_FAILED && !info.built && !info.ran && strstr(info.error,"NUL") && counter(count)==before+1 && counter(buildCount)==0 && sameBytes(output,"previous artifact"));
 project.configurations[0].linkLibraries.items=calloc(1,sizeof(char*)); CHECK(project.configurations[0].linkLibraries.items); project.configurations[0].linkLibraries.items[0]=copy("user32"); project.configurations[0].linkLibraries.count=1; CHECK(xideProjectSave(&project,NULL,&error) && writeText(mode,"misleading"));
 t=xideTasksBuild(tasks,&project,NULL,compiler,1,&error); CHECK(t && waitTask(tasks,t,&info) && info.state==XIDE_TASK_FAILED && !info.built && !info.ran && strstr(info.error,"options") && counter(buildCount)==0 && sameBytes(output,"previous artifact"));
 CHECK(writeText(mode,"build-change")); t=xideTasksBuild(tasks,&project,NULL,compiler,1,&error); CHECK(t && waitTask(tasks,t,&info) && info.state==XIDE_TASK_FAILED && !info.built && !info.ran && strstr(info.error,"changed") && counter(buildCount)==1 && sameBytes(output,"previous artifact"));
 CHECK(writeText(mode,"normal")); app.hidden=1; app.highDpi=1; app.dpiOverride=1.25f; app.maxFrames=1; app.compilerPath=compiler; app.projectPath=manifest; app.settingsPath=settings; app.sessionRoot=sessions;
 CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1; CHECK(xideActionCount()==75 && xideActionNamed("task.toolchainInfo")->id==65 && xideActionQuery(&app,XIDE_ACTION_TOOLCHAIN_INFO).enabled);
 CHECK(xideEditorOpen(&app,entry,&error) && xideEditorOpen(&app,native,&error)); xide_document *xlDoc=xideDocumentsFind(&app.documents,entry),*cDoc=app.activeDocument; CHECK(xlDoc && cDoc && token(xlDoc->editor,"extern",XUI_CODE_TOKEN_KEYWORD) && token(cDoc->editor,"native_answer",XUI_CODE_TOKEN_IDENTIFIER));
 uint32_t xlVersion=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xlDoc->editor)),cVersion=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(cDoc->editor)); baseline=xideProjectExport(&app.workspaceModel.projects[0],&error); CHECK(baseline);
 CHECK(menuOpen(&app) && xideToolchainViewIsOpen(&app) && waitView(&app) && strstr(xuiTextEditGetText(xideToolchainViewControl(&app,XIDE_TOOL_INFO)),"1.2.3.4") && xuiTextEditIsReadonly(xideToolchainViewControl(&app,XIDE_TOOL_HELP)) && capture(&app,image));
 CHECK(xideActionsHasModal(&app) && xideActionExecute(&app,XIDE_ACTION_NEW_FILE)==XIDE_ACTION_DISABLED && !xideReferencesViewOpen(&app) && !xideResourceViewOpen(&app) && !xideConfigurationViewOpen(&app) && !xideFontViewOpen(&app) && !xideImportViewOpen(&app) && !xideTemplateViewOpen(&app) && !xideRecoveryViewOpen(&app) && !xideFileChangesViewOpen(&app));
 char* help=KEEP(copy(xuiTextEditGetText(xideToolchainViewControl(&app,XIDE_TOOL_HELP)))); CHECK(xuiSetFocusWidget(app.context,xideToolchainViewControl(&app,XIDE_TOOL_HELP))==XUI_OK && xuiInputText(app.context,'X')==XUI_OK && settle(&app) && !strcmp(help,xuiTextEditGetText(xideToolchainViewControl(&app,XIDE_TOOL_HELP))));
 CHECK(writeText(mode,"nonzero") && click(&app,XIDE_TOOL_REFRESH) && waitView(&app) && strstr(xideToolchainViewMessage(&app),"上次有效") && !strcmp(help,xuiTextEditGetText(xideToolchainViewControl(&app,XIDE_TOOL_HELP))) && xuiWidgetGetEnabled(xideToolchainViewControl(&app,XIDE_TOOL_REFRESH)) && !(xuiWidgetGetStateId(xideToolchainViewControl(&app,XIDE_TOOL_REFRESH))&XUI_WIDGET_STATE_DISABLED) && !xuiWidgetGetEnabled(xideToolchainViewControl(&app,XIDE_TOOL_CANCEL)) && capture(&app,imageFailed));
 CHECK(writeText(mode,"slow") && click(&app,XIDE_TOOL_REFRESH) && xideToolchainViewBusy(&app) && click(&app,XIDE_TOOL_CANCEL) && waitView(&app) && strstr(xideToolchainViewMessage(&app),"取消"));
 CHECK(xideUiFontApply(&app,app.uiFontPath,40,&error) && resize(&app,700,460) && xuiTextEditGetFont(xideToolchainViewControl(&app,XIDE_TOOL_HELP))==app.uiFont && xuiScrollViewEnsureChildVisible(xideDialogGetScrollViewWidget(xideToolchainViewWindow(&app)),xideToolchainViewControl(&app,XIDE_TOOL_CLOSE))==XUI_OK && capture(&app,imageLarge));
 CHECK(click(&app,XIDE_TOOL_CLOSE) && !xideToolchainViewIsOpen(&app) && xideUiFontApply(&app,app.uiFontPath,16,&error) && resize(&app,1280,800));
 CHECK(xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xlDoc->editor))==xlVersion && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(cDoc->editor))==cVersion && !xlDoc->dirty && !cDoc->dirty); xvalue* after=xideProjectExport(&app.workspaceModel.projects[0],&error); CHECK(after && xrtValueEqual(after,baseline)); xrtValueRelease(after);
 CHECK(writeText(mode,"normal") && xideToolchainViewOpen(&app) && waitView(&app) && writeText(mode,"slow") && click(&app,XIDE_TOOL_REFRESH) && xideToolchainViewBusy(&app)); uint64_t closing=GetTickCount64(); CHECK(click(&app,XIDE_TOOL_CLOSE) && GetTickCount64()-closing<2000 && !xideToolchainViewIsOpen(&app) && waitView(&app));
 CHECK(writeText(mode,"normal") && xideToolchainViewOpen(&app) && waitView(&app) && writeText(mode,"pause") && click(&app,XIDE_TOOL_REFRESH) && xideToolchainViewBusy(&app)); xideAppDestroy(&app); memset(&app,0,sizeof(app));
 if(f->compiler) {
  CHECK(request(cache,f->compiler,root,1,&p,&error) && (p.commands&XIDE_TOOL_BUILD) && (p.commands&XIDE_TOOL_CHECK) && (p.options&XIDE_TOOL_OUTPUT) && (p.options&XIDE_TOOL_LINK));
  t=xideTasksBuild(tasks,&project,NULL,f->compiler,1,&error); CHECK(t && waitTask(tasks,t,&info)); if(info.state!=XIDE_TASK_SUCCEEDED) fprintf(stderr,"real toolchain: %s\n%s\n",info.error,xideTaskOutput(t)); CHECK(info.state==XIDE_TASK_SUCCEEDED && info.built && info.ran && xideTaskToolchain(t,&saved) && !saved.cached && xidePathEqual(saved.compiler,f->compiler) && saved.commands==p.commands && saved.options==p.options);
  app.hidden=1; app.highDpi=1; app.maxFrames=1; app.compilerPath=f->compiler; app.projectPath=manifest; app.settingsPath=settings; app.sessionRoot=sessions; CHECK(xideAppCreate(&app)==XUI_OK && xideToolchainViewOpen(&app) && waitView(&app) && capture(&app,imageReal));
  puts("toolchain_test compiler passed: actual pinned DEMO6 --help and fingerprints; fresh task observation, real C/xlang project built and ran, native real toolchain information.");
 }
 puts("toolchain_test passed: exact command/option scope; true PE file version; directory-specific cache, force query, sibling presence/replacement/read-only and post-query identity; output/UTF-8/NUL/exit/timeout/cancel; fresh build probe, unsupported link rejection and compiler change prevents publication/run; native menu, readonly summaries/help, failed refresh retains observation, modal guards, close/destruction, UIFont/125% DPI/two-axis scroll; C/xlang document state preserved; 4 native screenshots with real compiler.");
cleanup:
 if(began) { xgeEnd(); } xideAppDestroy(&app); xideToolchainRequestDestroy(pending); xideTasksDestroy(tasks); xideToolchainProfileClear(&p); xideToolchainProfileClear(&saved); xideToolchainsDestroy(cache); xideProjectClear(&project); xrtValueRelease(baseline); for(size_t i=0;i<ownedCount;++i) free(owned[i]); xgeQuit(); return XGE_OK;
}
static DWORD WINAPI worker(void* data)
{ fixture* f=data; if(!SetThreadDesktop(f->desktop)) return 1; xge_desc_t d={0}; d.iWidth=1600; d.iHeight=1000; d.sTitle="xlang toolchain profile verification"; d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; d.iRunMode=XGE_RUN_GAME_LOOP; int result=xgeInit(&d); if(result==XGE_OK) result=xgeRun(verify,f); xgeUnit(); return result==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv)
{
 if(argc<2 || argc>3 || !xrtDirCreateAll(argv[1])) return 2;
 fixture f={.directory=argv[1],.compiler=argc==3 ? argv[2] : NULL}; wchar_t name[100]; swprintf(name,100,L"XlangToolchainVerification-%lu",GetCurrentProcessId()); f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) return 1;
 HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1; if(thread) { if(WaitForSingleObject(thread,150000)!=WAIT_OBJECT_0) { fputs("toolchain_test timed out\n",stderr); ExitProcess(1); } GetExitCodeThread(thread,&code); CloseHandle(thread); } CloseDesktop(f.desktop); return (int)code;
}
