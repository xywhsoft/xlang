#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/rename_view.h"
#include "ui/editor_view.h"
#include "ui/workspace_view.h"
#include "ui/task_view.h"
#include "services/file_sync.h"
#include "model/resources.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Exercise the real service and real rename view, with one deterministic public
   text-edit failure after the physical commit. Production has no fault hook. */
static int failEdit;
static int replace(xui_code_document d,int start,int end,const char* body) { if(failEdit && --failEdit==0) return XUI_ERROR_OUT_OF_MEMORY; return xuiCodeDocumentReplace(d,start,end,body); }
#define xuiCodeDocumentReplace replace
#include "services/resource_rebind.c"
#undef xuiCodeDocumentReplace
typedef struct fixture { const char *directory,*compiler; HDESK desktop; int started,failed,checks; } fixture;
#define CHECK(v) do { ++f->checks; if(!(v)) { fprintf(stderr,"resource_rebind_test:%d: %s; %s; %s\n",__LINE__,#v,error.message,xideRenameViewMessage(&app)); f->failed=1; goto cleanup; } } while(0)
static const char* xml="<?xml version=\"1.0\" encoding=\"UTF-8\"?><assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\"><assemblyIdentity version=\"1.0.0.0\" processorArchitecture=\"*\" name=\"Xlang.IDE.Rename\" type=\"win32\"/></assembly>";
static int writeText(const char* p,const char* s) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static int settle(xide_app* a) { if(xuiDispatchPendingEvents(a->context)!=XUI_OK) return 0; xideEditorPoll(a); xideFileSyncPoll(a); xideTaskViewPoll(a); xideWorkbenchLayout(a); return xuiLayout(a->context)==XUI_OK && !a->frameError; }
static int capture(xide_app* a,const char* path) { if(!settle(a) || xuiUpdate(a->context,0)!=XUI_OK) return 0; xui_rect_i_t r={0,0,a->viewportWidth,a->viewportHeight}; return a->proxy.surfaceClear(&a->proxy,a->target,0xffeeeeeeu)==XUI_OK && xuiRender(a->context,a->target,&r,1)==XUI_OK && xideAppCapture(a,path)==XGE_OK; }
static int linkFile(const char* p,const char* target) { wchar_t w[32768],t[32768]; return MultiByteToWideChar(CP_UTF8,0,p,-1,w,32768) && MultiByteToWideChar(CP_UTF8,0,target,-1,t,32768) && CreateHardLinkW(w,t,NULL); }
static HANDLE lockFile(const char* p) { wchar_t w[32768]; if(!MultiByteToWideChar(CP_UTF8,0,p,-1,w,32768)) return INVALID_HANDLE_VALUE; return CreateFileW(w,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL); }
static int attributes(const char* p,DWORD a) { wchar_t w[32768]; return MultiByteToWideChar(CP_UTF8,0,p,-1,w,32768) && SetFileAttributesW(w,a); }
static int name(xide_app* a,const char* leaf) { return xuiEditSetText(xideRenameViewControl(a,XIDE_RENAME_NAME),leaf)==XUI_OK && xideRenameViewRefresh(a); }
static int openName(xide_app* a,const char* path,const char* leaf,xide_model_error* e) {
 const xide_file* selected=NULL;
 for(size_t i=0;i<a->workspaceModel.files.count;++i) if(xidePathEqual(a->workspaceModel.files.items[i].path,path)) selected=&a->workspaceModel.files.items[i];
 return selected && xideWorkspaceViewSelectFile(a,NULL,selected->id) && xideRenameViewOpenFile(a,path,e) && name(a,leaf);
}
static void cancel(xide_app* a) { xui_widget w=xideRenameViewControl(a,XIDE_RENAME_CANCEL); xuiScrollViewEnsureChildVisible(xideDialogGetScrollViewWidget(xideRenameViewWindow(a)),w); settle(a); xui_rect_t r=xuiWidgetGetWorldRect(w); float x=r.fX+r.fW/2,y=r.fY+r.fH/2; xuiInputPointerMove(a->context,x,y,0); xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT); xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0); settle(a); }
static int rawSame(const char* path,const char* text) { xide_text_file file={0}; int okay=xideTextFileRead(path,&file,NULL) && !strcmp(file.text,text); xideTextFileClear(&file); return okay; }
static int pe(const char* path) {
 wchar_t w[32768]; if(!MultiByteToWideChar(CP_UTF8,0,path,-1,w,32768)) return 0;
 HMODULE m=LoadLibraryExW(w,NULL,LOAD_LIBRARY_AS_DATAFILE|LOAD_LIBRARY_AS_IMAGE_RESOURCE); if(!m) return 0;
 HRSRC icon=FindResourceExW(m,MAKEINTRESOURCEW(3),MAKEINTRESOURCEW(1),1033),manifest=FindResourceExW(m,MAKEINTRESOURCEW(24),MAKEINTRESOURCEW(1),1033); HGLOBAL body=manifest ? LoadResource(m,manifest) : NULL; const void* bytes=body ? LockResource(body) : NULL;
 int okay=icon && bytes && SizeofResource(m,icon)==48 && SizeofResource(m,manifest)==strlen(xml) && !memcmp(bytes,xml,strlen(xml)); FreeLibrary(m); return okay;
}
static int verify(void* user) {
 fixture* f=user; if(f->started) return XGE_OK; f->started=1; xide_app app={0}; xide_model_error error={0}; xide_project project={0}; int began=0; HANDLE lock=INVALID_HANDLE_VALUE;
 xide_resource_config parsed={0}; xide_resource_rebind* probe=NULL; char* owned[32]={0}; int count=0;
#define KEEP(s) (owned[count++]=(s))
 char* id=xideCreateId("resource-rename"); char* root=KEEP(id ? xidePathResolve(f->directory,id) : NULL); free(id); CHECK(root && xrtDirCreateAll(root));
 char *entry=KEEP(xidePathResolve(root,"main.xl")),*native=KEEP(xidePathResolve(root,"native.c")),*manifest=KEEP(xidePathResolve(root,"app.xlp")),*workspace=KEEP(xidePathResolve(root,"work.xlw")),*settings=KEEP(xidePathResolve(root,"settings.json"));
 char *config=KEEP(xidePathResolve(root,"资源.res.xlon")),*other=KEEP(xidePathResolve(root,"other.res.json")),*icon=KEEP(xidePathResolve(root,"图标.ico")),*alias=KEEP(xidePathResolve(root,"alias.ico")),*nextIcon=KEEP(xidePathResolveLexical(root,"更新😀.ico")),*upperIcon=KEEP(xidePathResolveLexical(root,"更新😀.ICO")),*asset=KEEP(xidePathResolve(root,"app.xml")),*nextAsset=KEEP(xidePathResolveLexical(root,"清单😀.xml"));
 char *image=KEEP(xidePathResolve(root,"native-resource-rename.png")),*pendingImage=KEEP(xidePathResolve(root,"native-resource-rename-pending.png"));
 CHECK(entry && native && manifest && workspace && settings && config && other && icon && alias && nextIcon && upperIcon && asset && nextAsset && image && pendingImage);
 const char* typed="{\n \"format\":\"xlang-windows-resource-1\",\n \"icon\":{\"path\":\"./图标.ico\",\"base64\":\"inactive\",\"keep\":set[char(\"😀\")]},\n \"manifest\":{\"path\":\"app.xml\",\"text\":\"inactive\"},\n \"version\":{\"fileVersion\":\"9.8.7.6\",\"keep\":float(\"nan\")},\n \"extension\":intmap{-1:bytes(\"AQID\")}\n}";
 char json[32768]; snprintf(json,sizeof(json),"{\"icon\":\"%s\",\"manifest\":{\"path\":1,\"text\":\"<assembly/>\"},\"future\":true}",alias);
 CHECK(writeText(config,typed) && writeText(other,json) && writeText(asset,xml) && writeText(entry,"extern \"native_answer\" func nativeAnswer() as c_int\nfunc main(...) as int\n    return nativeAnswer()-42\nend\n") && writeText(native,"int native_answer(void) { return 42; }\n"));
 unsigned char ico[70]={0}; ico[2]=1; ico[4]=1; ico[6]=1; ico[7]=1; ico[10]=1; ico[12]=32; ico[14]=48; ico[18]=22; ico[22]=40; ico[26]=1; ico[30]=2; ico[34]=1; ico[36]=32; ico[42]=4; ico[62]=0x44; ico[63]=0x88; ico[64]=0xcc; ico[65]=0xff;
 CHECK(xrtFileWriteAtomic(icon,(xbytesview){ico,sizeof(ico)}) && linkFile(alias,icon));
 app.hidden=1; app.noSession=1; app.highDpi=1; app.dpiOverride=1.25f; app.settingsPath=settings;
 CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1;
 CHECK(xideWorkspaceCreate(&app.workspaceModel,"资源素材改名",&error) && xideProjectCreate(&project,manifest,"C / xlang",entry,&error) && xideProjectAddFile(&project,native,"c",&error) && xideProjectAddFile(&project,config,"resource-config",&error) && xideProjectSave(&project,NULL,&error) && xideWorkspaceAddProject(&app.workspaceModel,manifest,"alpha",&error)); xideProjectClear(&project);
 xide_project* p=xideWorkspaceFindProject(&app.workspaceModel,"alpha"); CHECK(p && xideProjectAddFile(p,other,"resource-config",&error) && xideWorkspaceAddFile(&app.workspaceModel,icon,&error) && xideWorkspaceAddFile(&app.workspaceModel,asset,&error) && xideWorkspaceSaveAll(&app,workspace,&error));
 char* beta=KEEP(xidePathResolve(root,"beta.xlp")); CHECK(beta && xideProjectCreate(&project,beta,"共享资源项目",entry,&error) && xideProjectAddFile(&project,config,"resource-config",&error) && xideProjectSave(&project,NULL,&error) && xideWorkspaceAddProject(&app.workspaceModel,beta,"beta",&error) && xideWorkspaceSaveAll(&app,NULL,&error)); xideProjectClear(&project); p=xideWorkspaceFindProject(&app.workspaceModel,"alpha"); CHECK(p);
 CHECK(xideEditorOpen(&app,entry,&error)); xide_document* xl=app.activeDocument;
 CHECK(xideEditorOpen(&app,native,&error)); xide_document* c=app.activeDocument; xui_code_document ccore=xuiCodeEditGetDocument(c->editor);
 CHECK(xuiCodeDocumentInsert(ccore,0,"// 未保存 C 😀\n")==XUI_OK && settle(&app)); uint32_t cv=xuiCodeDocumentGetVersion(ccore); CHECK(xuiCodeSelectionSetRange(xuiCodeEditGetSelection(c->editor),ccore,2,6)==XUI_OK);
 char* configAlias=KEEP(xidePathResolve(root,"config-alias.res.xlon")); xide_project* q=xideWorkspaceFindProject(&app.workspaceModel,"beta");
 CHECK(configAlias && q && linkFile(configAlias,config) && xideProjectRemoveFile(q,q->files.items[q->files.count-1].id,&error) && xideProjectAddFile(q,configAlias,"resource-config",&error) && xideWorkspaceViewRefresh(&app)==XUI_OK);
 CHECK(!openName(&app,icon,"更新😀.ico",&error) && strstr(error.message,"硬链接路径") && xidePathExists(icon) && !xideDocumentsFind(&app.documents,config));
 CHECK(xideProjectRemoveFile(q,q->files.items[q->files.count-1].id,&error) && xideProjectAddFile(q,config,"resource-config",&error) && xideWorkspaceSaveAll(&app,NULL,&error) && xideWorkspaceViewRefresh(&app)==XUI_OK);
 CHECK(openName(&app,icon,"更新😀.ico",&error) && strstr(xideRenameViewMessage(&app),"2 份"));
 CHECK(writeText(other,"{\"icon\":null}") && !xideRenameViewApply(&app,&error) && xidePathExists(icon) && !xideDocumentsFind(&app.documents,config)); cancel(&app); CHECK(writeText(other,json));
 CHECK(openName(&app,icon,"更新😀.ico",&error) && writeText(config,typed) && !xideRenameViewApply(&app,&error) && xidePathExists(icon)); cancel(&app);
 CHECK(openName(&app,icon,"更新😀.ico",&error)); lock=lockFile(icon); CHECK(lock!=INVALID_HANDLE_VALUE && !xideRenameViewApply(&app,&error) && !xideDocumentsFind(&app.documents,config) && !xideDocumentsFind(&app.documents,other) && app.activeDocument==c); CloseHandle(lock); lock=INVALID_HANDLE_VALUE; cancel(&app);
 CHECK(attributes(config,FILE_ATTRIBUTE_READONLY) && !openName(&app,icon,"更新😀.ico",&error) && xidePathExists(icon)); CHECK(attributes(config,FILE_ATTRIBUTE_NORMAL));
 CHECK(xideEditorOpen(&app,config,&error)); xide_document* d=app.activeDocument; xui_code_document core=xuiCodeEditGetDocument(d->editor);
 CHECK(xuiCodeDocumentInsert(core,2," \"dirty\":\"原有未保存扩展😀\",\n")==XUI_OK && settle(&app) && xideEditorShow(&app,c)==XUI_OK);
 CHECK(openName(&app,icon,"更新😀.ico",&error)); CHECK(xuiCodeDocumentInsert(core,0,"// 模态外部编辑\n")==XUI_OK && !xideRenameViewApply(&app,&error) && xidePathExists(icon)); cancel(&app); CHECK(xuiCodeDocumentUndo(core)==XUI_OK && settle(&app));
 char* before=KEEP(copy(xuiCodeEditGetText(d->editor))); CHECK(before && xuiCodeSelectionSetRange(xuiCodeEditGetSelection(d->editor),core,2,6)==XUI_OK);
 uint32_t beforeVersion=xuiCodeDocumentGetVersion(core); CHECK(openName(&app,icon,"图标.ico",&error) && xideRenameViewApply(&app,&error) && settle(&app) && xuiCodeDocumentGetVersion(core)==beforeVersion && !xideDocumentsFind(&app.documents,other) && rawSame(other,json));
 CHECK(openName(&app,icon,"更新😀.ico",&error) && capture(&app,image)); failEdit=2;
 CHECK(!xideRenameViewApply(&app,&error) && xideRenameViewHasPending(&app) && !xidePathExists(icon) && xidePathExists(nextIcon) && xidePathExists(alias) && strstr(error.message,"已改名") && !xideActionQuery(&app,XIDE_ACTION_BUILD).enabled);
 CHECK(rawSame(config,typed) && rawSame(other,json) && capture(&app,pendingImage));
 cancel(&app); CHECK(xideRenameViewHasPending(&app)); xideDialogSetOpen(xideRenameViewWindow(&app),0); CHECK(xideActionsHasModal(&app)); xideWorkbenchLayout(&app); CHECK(xideRenameViewIsOpen(&app));
 CHECK(xideRenameViewApply(&app,&error) && !xideRenameViewHasPending(&app) && settle(&app) && app.activeDocument==c && c->dirty && xuiCodeDocumentGetVersion(ccore)==cv);
 int a=0,b=0; CHECK(xuiCodeSelectionGetRange(xuiCodeEditGetSelection(c->editor),&a,&b)==XUI_OK && a==2 && b==6);
 xide_document* second=xideDocumentsFind(&app.documents,other); CHECK(second && second->dirty && d->dirty && strstr(xuiCodeEditGetText(d->editor),"./更新😀.ico") && strstr(xuiCodeEditGetText(d->editor),"set[char(\"😀\")]"));
 CHECK(strstr(xuiCodeEditGetText(d->editor),"\"base64\":\"inactive\"") && strstr(xuiCodeEditGetText(second->editor),nextIcon) && strstr(xuiCodeEditGetText(second->editor),"\"path\":1"));
 CHECK(xuiCodeSelectionGetRange(xuiCodeEditGetSelection(d->editor),&a,&b)==XUI_OK && a==2 && b==6);
 CHECK(xuiCodeDocumentUndo(core)==XUI_OK && !strcmp(xuiCodeEditGetText(d->editor),before) && xuiCodeDocumentRedo(core)==XUI_OK && settle(&app));
 CHECK(openName(&app,nextIcon,"更新😀.ICO",&error) && xideRenameViewApply(&app,&error) && settle(&app) && strstr(xuiCodeEditGetText(d->editor),"./更新😀.ICO") && strstr(xuiCodeEditGetText(second->editor),upperIcon));
 CHECK(xideEditorOpen(&app,asset,&error)); xide_document* xmlDoc=app.activeDocument; uint32_t xmlVersion=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xmlDoc->editor));
 CHECK(xideRenameViewOpen(&app) && name(&app,"清单😀.xml") && xideRenameViewApply(&app,&error) && settle(&app) && !strcmp(xmlDoc->path,nextAsset) && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xmlDoc->editor))==xmlVersion && strstr(xuiCodeEditGetText(d->editor),"清单😀.xml") && strstr(xuiCodeEditGetText(d->editor),"\"text\":\"inactive\""));
 CHECK(rawSame(config,typed) && xideDocumentSave(d,NULL,0,&error) && xideDocumentSave(second,NULL,0,&error) && xideDocumentSave(c,NULL,0,&error) && xideWorkspaceSaveAll(&app,NULL,&error));
 /* The second config deliberately tests path/type precedence. The PE build uses
    the first config so two resource groups do not compete for the same IDs. */
 CHECK(xideProjectRemoveFile(p,p->files.items[p->files.count-1].id,&error) && xideWorkspaceSaveAll(&app,NULL,&error));
 if(f->compiler) { xide_task* task=xideTasksBuild(app.tasks,p,NULL,f->compiler,1,&error); xide_task_info info={0}; CHECK(task); uint64_t end=GetTickCount64()+45000;
  do { if(!settle(&app)) { CHECK(0); } xideTaskInfo(task,&info); if(info.state>=XIDE_TASK_SUCCEEDED) break; Sleep(5); } while(GetTickCount64()<end);
  if(info.state!=XIDE_TASK_SUCCEEDED) fprintf(stderr,"build log: %s; %s\n",xideTaskBuildLog(task),info.error);
  CHECK(info.state==XIDE_TASK_SUCCEEDED && info.built && info.ran && pe(xideTaskArtifact(task))); puts("resource_rebind_test real C/xlang PE icon/manifest build and run passed.");
 }
 CHECK(xl->open && !strcmp(xl->language,"xlang") && !strcmp(c->language,"c") && !app.frameError);
 printf("resource_rebind_test passed: %d checks; shared/dirty/unopened modern XLON and JSON references, identity/version/readonly/source-lock guards, hardlink aliases, case-only/Unicode spelling, preserved inactive fields/extensions/selection/C undo, one-step resource undo/redo, no implicit disk save, physical-commit edit failure/retry/modal barrier; two native screenshots.\n",f->checks);
cleanup:
 if(lock!=INVALID_HANDLE_VALUE) { CloseHandle(lock); } xideResourceRebindDestroy(probe); xideResourceClear(&parsed); xideProjectClear(&project); xgeQuit(); if(began) xgeEnd(); xideAppDestroy(&app); for(int i=0;i<count;++i) free(owned[i]); return XGE_OK;
#undef KEEP
}
static DWORD WINAPI worker(void* user) { fixture* f=user; if(!SetThreadDesktop(f->desktop)) return 1; xge_desc_t d={0}; d.iWidth=1600; d.iHeight=1000; d.sTitle="xlang resource rename verification"; d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; d.iRunMode=XGE_RUN_GAME_LOOP; int result=xgeInit(&d); if(result==XGE_OK) result=xgeRun(verify,f); xgeUnit(); return result==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv) { if((argc!=2 && argc!=3) || !xrtDirCreateAll(argv[1])) return 2; fixture f={.directory=argv[1],.compiler=argc==3 ? argv[2] : NULL}; wchar_t name[100]; swprintf(name,100,L"XlangResourceRenameVerification-%lu",GetCurrentProcessId()); f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) return 1; HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1; if(thread) { if(WaitForSingleObject(thread,90000)!=WAIT_OBJECT_0) ExitProcess(1); GetExitCodeThread(thread,&code); CloseHandle(thread); } CloseDesktop(f.desktop); return (int)code; }
