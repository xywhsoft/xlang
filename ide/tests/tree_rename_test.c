#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/rename_view.h"
#include "ui/workspace_view.h"
#include "ui/editor_view.h"
#include "ui/task_view.h"
#include "services/project_templates.h"
#include "services/file_sync.h"
#include "services/session.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct fixture {
 const char *directory,*compiler; HDESK desktop; int started,failed,trace;
 PVOID volatile phase; volatile LONG checkLine;
} fixture;
static void phase(fixture* f,const char* name)
{
 InterlockedExchangePointer(&f->phase,(PVOID)name);
 if(f->trace) fprintf(stderr,"tree_rename_test phase=%s thread=%lu tick=%llu\n",name,(unsigned long)GetCurrentThreadId(),(unsigned long long)GetTickCount64());
}
static const char* lastPhase(fixture* f)
{ const char* p=InterlockedCompareExchangePointer(&f->phase,NULL,NULL); return p ? p : "not-started"; }
#define CHECK(v) do { InterlockedExchange(&f->checkLine,__LINE__); if(f->trace) fprintf(stderr,"tree_rename_test check=%d phase=%s\n",__LINE__,lastPhase(f)); if(!(v)) { fprintf(stderr,"tree_rename_test:%d: %s; phase=%s; %s\n",__LINE__,#v,lastPhase(f),error.message); f->failed=1; goto cleanup; } } while(0)
static char* copy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static char* keep(char** list,size_t* count,char* value) { if(value) list[(*count)++]=value; return value; }
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* v,const char* s) { return xrtValueObjectGet(v,sv(s)); }
static int put(xvalue* v,const char* k,const char* s) { return xrtValueObjectSetNew(v,sv(k),xrtValueString(sv(s))); }
static int writeText(const char* p,const char* s) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static xide_file* file(xide_files* files,const char* path) { for(size_t i=0;i<files->count;++i) if(xidePathEqual(files->items[i].path,path)) return &files->items[i]; return NULL; }
static int settle(xide_app* a) { if(xuiDispatchPendingEvents(a->context)!=XUI_OK) return 0; xideEditorPoll(a); xideFileSyncPoll(a); xideTaskViewPoll(a); xideWorkbenchLayout(a); return xuiLayout(a->context)==XUI_OK; }
static int idle(xide_app* a) { uint64_t end=GetTickCount64()+10000; do { if(!settle(a)) return 0; if(xideFileSyncSettled(a)) return 1; Sleep(5); } while(GetTickCount64()<end); return 0; }
static int taskWait(xide_app* a,xide_task* t,xide_task_info* info,int phase)
{
 uint64_t end=GetTickCount64()+30000; int settled=1;
 do {
  settled=settle(a); xideTaskInfo(t,info); if(!settled) break;
  if(phase>=0 ? (int)info->phase==phase : info->state>=XIDE_TASK_SUCCEEDED) return 1;
  if(phase>=0 && info->state>=XIDE_TASK_SUCCEEDED) break;
  Sleep(5);
 } while(GetTickCount64()<end);
 fprintf(stderr,"tree_rename_test task wait failed: settled=%d wantedPhase=%d state=%d phase=%d invalidated=%d built=%d ran=%d error=%s buildLog=%s\n",settled,phase,info->state,info->phase,info->invalidated,info->built,info->ran,info->error,xideTaskBuildLog(t));
 return 0;
}
static size_t opened(xide_app* a) { size_t n=0; for(size_t i=0;i<a->documents.count;++i) if(a->documents.items[i]->open) ++n; return n; }
static int key(xide_app* a,int k) { return xuiInputKeyDownEx(a->context,k,0,NULL)==XUI_OK && xuiInputKeyUpEx(a->context,k,0,NULL)==XUI_OK && settle(a); }
static int pointer(xide_app* a,xui_rect_t r) { float x=r.fX+r.fW/2,y=r.fY+r.fH/2; return xuiInputPointerMove(a->context,x,y,0)==XUI_OK && xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(a); }
static int click(xide_app* a,int id) { xui_widget w=xideRenameViewControl(a,id),scroll=xideDialogGetScrollViewWidget(xideRenameViewWindow(a)); if(xuiScrollViewEnsureChildVisible(scroll,w)!=XUI_OK || !settle(a)) return 0; return pointer(a,xuiWidgetGetWorldRect(w)); }
static int capture(xide_app* a,const char* path) { if(!settle(a) || xuiUpdate(a->context,0)!=XUI_OK) return 0; xui_rect_i_t r={0,0,a->viewportWidth,a->viewportHeight}; return a->proxy.surfaceClear(&a->proxy,a->target,0xffeeeeeeu)==XUI_OK && xuiRender(a->context,a->target,&r,1)==XUI_OK && xideAppCapture(a,path)==XGE_OK && !a->frameError; }
static int treeOpen(xide_app* a,const char* project,const char* id) { return xideWorkspaceViewSelectFile(a,project,id) && xuiSetFocusWidget(a->context,a->workspace)==XUI_OK && key(a,XUI_KEY_F2) && xideRenameViewIsOpen(a); }
static int name(xide_app* a,const char* value) { return xuiEditSetText(xideRenameViewControl(a,XIDE_RENAME_NAME),value)==XUI_OK && xideRenameViewRefresh(a); }
static int menuRename(xide_app* a) { int index=-1; for(int i=0;i<xuiMenuGetItemCount(a->projectMenu);++i) if(xuiMenuGetItem(a->projectMenu,i)->iValue==XIDE_ACTION_RENAME_SELECTED_FILE) index=i; if(index<0 || xuiMenuOpenAt(a->projectMenu,a->root,80,60)!=XUI_OK || !settle(a)) return 0; xui_rect_t r=xuiMenuGetItemRect(a->projectMenu,index),world=xuiWidgetGetWorldRect(a->projectMenu); r.fX+=world.fX; r.fY+=world.fY; return xuiInputPointerMove(a->context,r.fX+r.fW/2,r.fY+r.fH/2,0)==XUI_OK && settle(a) && xuiMenuGetHoverIndex(a->projectMenu)==index && pointer(a,r); }
static xui_widget firstInput(xui_context c,xui_widget w) { if(xuiWidgetGetType(w)==xuiWidgetFindType(c,"input")) return w; for(xui_widget child=xuiWidgetGetFirstChild(w);child;child=xuiWidgetGetNextSibling(child)) { xui_widget result=firstInput(c,child); if(result) return result; } return NULL; }
static HANDLE handle(const char* p,DWORD access,DWORD share) { wchar_t w[32768]; if(!MultiByteToWideChar(CP_UTF8,0,p,-1,w,32768)) return INVALID_HANDLE_VALUE; return CreateFileW(w,access,share,NULL,OPEN_EXISTING,0,NULL); }
static int removeFile(const char* p) { wchar_t w[32768]; return MultiByteToWideChar(CP_UTF8,0,p,-1,w,32768) && DeleteFileW(w); }
static int linkFile(const char* p,const char* target) { wchar_t w[32768],t[32768]; return MultiByteToWideChar(CP_UTF8,0,p,-1,w,32768) && MultiByteToWideChar(CP_UTF8,0,target,-1,t,32768) && CreateHardLinkW(w,t,NULL); }
static int pattern(const char* p,uint64_t length) { wchar_t w[32768]; if(!MultiByteToWideChar(CP_UTF8,0,p,-1,w,32768)) return 0; HANDLE h=CreateFileW(w,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL); if(h==INVALID_HANDLE_VALUE) return 0; unsigned char block[65536]; for(size_t i=0;i<sizeof(block);++i) block[i]=(unsigned char)i; int okay=1; for(uint64_t at=0;okay && at<length;) { DWORD size=length-at>sizeof(block) ? sizeof(block) : (DWORD)(length-at),written=0; okay=WriteFile(h,block,size,&written,NULL) && written==size; at+=written; } CloseHandle(h); return okay; }
static int mutateKeepTime(const char* p,unsigned char byte) { HANDLE h=handle(p,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE); if(h==INVALID_HANDLE_VALUE) return 0; FILETIME time={0}; DWORD written=0; int okay=GetFileTime(h,NULL,NULL,&time) && WriteFile(h,&byte,1,&written,NULL) && written==1 && FlushFileBuffers(h) && SetFileTime(h,NULL,NULL,&time); CloseHandle(h); return okay; }
static char* fakeCompiler(void) { wchar_t w[32768]; if(!GetModuleFileNameW(NULL,w,32768)) return NULL; wchar_t* slash=wcsrchr(w,L'\\'); if(!slash) return NULL; wcscpy(slash+1,L"task_test.exe"); int n=WideCharToMultiByte(CP_UTF8,0,w,-1,NULL,0,NULL,NULL); char* p=n ? malloc((size_t)n) : NULL; if(p && !WideCharToMultiByte(CP_UTF8,0,w,-1,p,n,NULL,NULL)) { free(p); p=NULL; } return p; }
static int verify(void* user)
{
 fixture* f=user; if(f->started) return XGE_OK; f->started=1; phase(f,"fixture-setup");
 xide_app app={0}; xide_model_error error={0}; xide_template_plan* plan=NULL; HANDLE lock=INVALID_HANDLE_VALUE; int began=0; char* owned[64]={0}; size_t count=0; xide_project round={0}; xvalue* state=NULL;
#define KEEP(v) keep(owned,&count,(v))
 char* unique=xideCreateId("tree-rename"); char* root=KEEP(unique ? xidePathResolve(f->directory,unique) : NULL); free(unique); CHECK(root && xrtDirCreateAll(root));
 char *settings=KEEP(xidePathResolve(root,"settings.json")),*sessions=KEEP(xidePathResolve(root,"sessions")),*workspace=KEEP(xidePathResolve(root,"工程树验证.xlw")),*binary=KEEP(xidePathResolve(root,"大型二进制😀.bin")),*newBinary=KEEP(xidePathResolve(root,"大型改名😀.bin")),*small=KEEP(xidePathResolve(root,"共享二进制.bin")),*newSmall=KEEP(xidePathResolve(root,"共享新名.bin")),*alias=KEEP(xidePathResolve(root,"所选硬链接.c")),*newAlias=KEEP(xidePathResolve(root,"硬链接新名😀.c")),*independent=KEEP(xidePathResolve(root,"保持编辑.xl")),*imageBinary=KEEP(xidePathResolve(root,"native-tree-rename-binary.png")),*imageAlias=KEEP(xidePathResolve(root,"native-tree-rename-alias.png")),*resource=KEEP(xidePathResolve(root,"资源.json")),*newResource=KEEP(xidePathResolve(root,"树资源.xlon"));
 CHECK(settings && sessions && workspace && binary && newBinary && small && newSmall && alias && newAlias && independent && imageBinary && imageAlias && resource && newResource);
 app.hidden=1; app.maxFrames=1; app.highDpi=1; app.dpiOverride=1.25f; app.settingsPath=settings; app.sessionRoot=sessions; CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1;
 CHECK(xideWorkspaceCreate(&app.workspaceModel,"工程树重命名",&error)); char *pId=NULL,*qId=NULL;
 plan=xideTemplatePlanPrepare(root,"共享原项目","mixed-c",&error); CHECK(plan && xideTemplatePlanCommit(&app.workspaceModel,plan,1,&pId,&error)); KEEP(pId); xideTemplatePlanDestroy(plan); plan=NULL;
 plan=xideTemplatePlanPrepare(root,"共享别名项目","mixed-c",&error); CHECK(plan && xideTemplatePlanCommit(&app.workspaceModel,plan,0,&qId,&error)); KEEP(qId); xideTemplatePlanDestroy(plan); plan=NULL;
 xide_project *p=xideWorkspaceFindProject(&app.workspaceModel,pId),*q=xideWorkspaceFindProject(&app.workspaceModel,qId); CHECK(p && q);
 CHECK(put(p->source,"futureRoot","保留根字段")); size_t jsonLength=0; char* json=xrtJsonStringify(p->source,1,&jsonLength); CHECK(json && xrtFileWriteAtomic(p->manifest,(xbytesview){(const unsigned char*)json,jsonLength})); xrtFree(json);
 char *entry=KEEP(copy(p->entry)),*native=KEEP(copy(p->cSources.items[0])),*directory=KEEP(xidePathDirectory(p->entry)),*newEntry=KEEP(xidePathResolve(directory,"树入口😀.xl")),*header=KEEP(xidePathResolve(directory,"native.h")),*newHeader=KEEP(xidePathResolve(directory,"树头文件.h")),*oldQEntry=KEEP(copy(file(&q->files,q->entry)->id)),*oldQC=KEEP(copy(file(&q->files,q->cSources.items[0])->id)); CHECK(entry && native && directory && newEntry && header && newHeader && oldQEntry && oldQC);
 CHECK(writeText(entry,"// FAKE_SLOW\nextern \"native_answer\" func nativeAnswer() as c_int\nfunc main(...) as int\n    if nativeAnswer() != 42 then\n        return 1\n    end\n    return 0\nend\n") && writeText(native,"int native_answer(void) { return 42; }\n") && linkFile(alias,native));
 CHECK(xideProjectSetEntry(q,entry,&error) && xideProjectRemoveFile(q,oldQEntry,&error) && xideProjectRemoveFile(q,oldQC,&error) && xideProjectAddFile(q,alias,"c",&error));
 const uint64_t bigLength=20u*1024u*1024u+73u; const unsigned char expected[32]={0xfc,0x3d,0x4b,0x44,0xbe,0x19,0xf6,0x53,0x64,0x79,0x67,0xe5,0x0f,0x07,0x86,0xf5,0x48,0x2e,0xda,0x6a,0x46,0x1d,0xa5,0x3a,0x55,0x20,0x55,0x73,0x1a,0x78,0x98,0xd9};
 CHECK(pattern(binary,bigLength) && pattern(small,512) && xideWorkspaceAddFile(&app.workspaceModel,binary,&error) && xideWorkspaceAddFile(&app.workspaceModel,small,&error) && xideProjectAddFile(p,small,"resource",&error) && xideProjectAddFile(q,small,"resource",&error));
 CHECK(writeText(resource,"{\"format\":\"xlang-windows-resource-1\",\"version\":{\"fileVersion\":\"2.3.4.5\",\"productVersion\":\"2.3.4.5\"}}") && xideProjectAddFile(p,resource,"resource-config",&error) && xideProjectAddFile(q,resource,"resource-config",&error));
 char *binaryId=KEEP(copy(file(&app.workspaceModel.files,binary)->id)),*smallId=KEEP(copy(file(&p->files,small)->id)),*pCId=KEEP(copy(file(&p->files,native)->id)),*qCId=KEEP(copy(file(&q->files,alias)->id)),*entryId=KEEP(copy(file(&p->files,entry)->id)),*headerId=KEEP(copy(file(&p->files,header)->id)),*resourceId=KEEP(copy(file(&p->files,resource)->id)); CHECK(binaryId && smallId && pCId && qCId && entryId && headerId && resourceId);
 CHECK(put(file(&p->files,small)->source,"future","保留二进制字段") && put(file(&q->files,alias)->source,"future","保留别名字段") && xideWorkspaceViewRefresh(&app)==XUI_OK && xideWorkspaceSaveAll(&app,workspace,&error));
 CHECK(writeText(independent,"func main(...) as int\n    return 0\nend\n") && xideEditorOpen(&app,independent,&error)); xide_document* b=app.activeDocument; xui_code_document bcore=xuiCodeEditGetDocument(b->editor); CHECK(xuiCodeDocumentInsert(bcore,0,"// 保持未保存😀\n")==XUI_OK); uint32_t bVersion=xuiCodeDocumentGetVersion(bcore); uint64_t bId=b->id; size_t slots=app.documents.count;
 CHECK(xideWorkspaceViewSelectProject(&app,pId) && xuiSetFocusWidget(app.context,app.workspace)==XUI_OK && key(&app,XUI_KEY_F2) && !xideRenameViewIsOpen(&app) && !xideActionQuery(&app,XIDE_ACTION_RENAME_SELECTED_FILE).enabled && app.activeDocument==b);
 xui_widget property=firstInput(app.context,xuiScrollViewGetContentWidget(app.properties)); CHECK(property && xuiSetFocusWidget(app.context,property)==XUI_OK && key(&app,XUI_KEY_F2) && !xideRenameViewIsOpen(&app));
 CHECK(xideWorkspaceViewSelectFile(&app,pId,pCId)); q->available=0; CHECK(xideWorkspaceViewSelectFile(&app,qId,qCId) && !xideWorkspaceViewSelectedFile(&app) && !xideActionQuery(&app,XIDE_ACTION_RENAME_SELECTED_FILE).enabled); q->available=1;
 phase(f,"binary-guards");
 xide_file_snapshot initial={0},changed={0}; CHECK(xideFileSnapshotRead(binary,&initial,&error) && initial.length==bigLength && !memcmp(initial.sha256,expected,32));
 CHECK(treeOpen(&app,"",binaryId) && !strcmp(xuiEditGetText(xideRenameViewControl(&app,XIDE_RENAME_ORIGINAL)),binary) && app.activeDocument==b && app.documents.count==slots && opened(&app)==1 && name(&app,"大型改名😀.bin") && click(&app,XIDE_RENAME_CANCEL) && xidePathExists(binary) && !xidePathExists(newBinary));
 CHECK(treeOpen(&app,"",binaryId) && name(&app,"大型改名😀.bin") && mutateKeepTime(binary,255) && xideFileSnapshotRead(binary,&changed,&error) && changed.length==initial.length && changed.index==initial.index && changed.volume==initial.volume && changed.lastWrite==initial.lastWrite && changed.attributes==initial.attributes && memcmp(changed.sha256,initial.sha256,32));
 CHECK(!xideRenameViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && xideRenameViewIsOpen(&app) && !strcmp(xuiEditGetText(xideRenameViewControl(&app,XIDE_RENAME_NAME)),"大型改名😀.bin") && xidePathExists(binary) && !xidePathExists(newBinary) && !app.fileRebind && !app.fileRebindReading && click(&app,XIDE_RENAME_CANCEL) && mutateKeepTime(binary,0));
 CHECK(treeOpen(&app,"",binaryId) && name(&app,"大型改名😀.bin")); lock=handle(binary,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE); CHECK(lock!=INVALID_HANDLE_VALUE); CHECK(!xideRenameViewApply(&app,&error) && error.code==XIDE_MODEL_IO && !app.fileRebind); CloseHandle(lock); lock=INVALID_HANDLE_VALUE; CHECK(click(&app,XIDE_RENAME_CANCEL));
 wchar_t w[32768]; CHECK(MultiByteToWideChar(CP_UTF8,0,binary,-1,w,32768)); DWORD attr=GetFileAttributesW(w); CHECK(SetFileAttributesW(w,attr|FILE_ATTRIBUTE_READONLY) && !xideActionQuery(&app,XIDE_ACTION_RENAME_SELECTED_FILE).enabled && !xideRenameViewOpenFile(&app,binary,&error) && SetFileAttributesW(w,attr));
 CHECK(treeOpen(&app,"",binaryId) && name(&app,"大型改名😀.bin") && writeText(newBinary,"occupied") && !xideRenameViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && xidePathExists(binary) && click(&app,XIDE_RENAME_CANCEL) && removeFile(newBinary));
 CHECK(treeOpen(&app,"",binaryId) && name(&app,"大型改名😀.bin") && xideWorkspaceViewSelectProject(&app,pId) && !xideRenameViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && click(&app,XIDE_RENAME_CANCEL));
 phase(f,"binary-commit");
 CHECK(treeOpen(&app,"",binaryId) && name(&app,"大型改名😀.bin") && capture(&app,imageBinary) && click(&app,XIDE_RENAME_APPLY));
 CHECK(!xidePathExists(binary) && xideFileSnapshotRead(newBinary,&changed,&error) && xideFileSnapshotEqual(&initial,&changed) && !strcmp(file(&app.workspaceModel.files,newBinary)->id,binaryId) && app.activeDocument==b && b->id==bId && b->dirty && xuiCodeDocumentGetVersion(bcore)==bVersion && app.documents.count==slots && opened(&app)==1 && xuiGetFocusWidget(app.context)==app.workspace);
 /* Same bytes replaced atomically still have a different physical identity. */
 CHECK(treeOpen(&app,pId,smallId) && name(&app,"共享新名.bin") && xideFileSnapshotRead(small,&initial,&error)); unsigned char bytes[512]; for(size_t i=0;i<sizeof(bytes);++i) bytes[i]=(unsigned char)i;
 CHECK(xrtFileWriteAtomic(small,(xbytesview){bytes,sizeof(bytes)}) && xideFileSnapshotRead(small,&changed,&error) && !memcmp(initial.sha256,changed.sha256,32) && initial.index!=changed.index && !xideRenameViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && click(&app,XIDE_RENAME_CANCEL));
 CHECK(treeOpen(&app,pId,smallId) && name(&app,"共享新名.bin") && put(file(&p->files,small)->source,"temporary","更改后过期") && !xideRenameViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && click(&app,XIDE_RENAME_CANCEL) && xrtValueObjectRemove(file(&p->files,small)->source,sv("temporary")));
 CHECK(xideEditorOpen(&app,p->manifest,&error)); xide_document* md=app.activeDocument; CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(md->editor),0," ")==XUI_OK && xideEditorShow(&app,b)==XUI_OK && treeOpen(&app,pId,smallId) && name(&app,"共享新名.bin") && !xideRenameViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && !app.fileRebind && click(&app,XIDE_RENAME_CANCEL) && xideEditorClose(&app,md,1,&error) && xideEditorShow(&app,b)==XUI_OK);
 CHECK(treeOpen(&app,pId,smallId) && name(&app,"共享新名.bin") && click(&app,XIDE_RENAME_APPLY) && !xidePathExists(small) && file(&p->files,newSmall) && file(&q->files,newSmall) && file(&app.workspaceModel.files,newSmall) && !strcmp(file(&p->files,newSmall)->id,smallId) && get(file(&p->files,newSmall)->source,"future") && p->dirty && q->dirty && app.workspaceModel.dirty && app.activeDocument==b && opened(&app)==1);
 /* An out-of-band document open invalidates the closed-file dialog baseline. */
 CHECK(treeOpen(&app,pId,entryId) && name(&app,"树入口😀.xl")); xide_document* transient=xideDocumentOpen(&app.documents,entry,&error); CHECK(transient && !xideRenameViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && click(&app,XIDE_RENAME_CANCEL) && xideEditorClose(&app,transient,1,&error) && xideEditorShow(&app,b)==XUI_OK && !xideDocumentsFind(&app.documents,entry));
 CHECK(xideEditorOpen(&app,native,&error)); xide_document* c=app.activeDocument; xui_code_document ccore=xuiCodeEditGetDocument(c->editor); CHECK(xuiCodeDocumentInsert(ccore,0,"// 未保存 C 😀\n")==XUI_OK && xuiCodeSelectionSetRange(xuiCodeEditGetSelection(c->editor),ccore,2,6)==XUI_OK); uint32_t cVersion=xuiCodeDocumentGetVersion(ccore); uint64_t cId=c->id; xui_widget cEditor=c->editor; int cWindow=c->window; unsigned char* raw=c->baseline.raw; char* cSession=KEEP(copy(c->sessionId)); CHECK(cSession && xideEditorShow(&app,b)==XUI_OK && xideWorkspaceViewSelectFile(&app,qId,qCId));
 phase(f,"hardlink-commit");
 CHECK(menuRename(&app) && xideRenameViewIsOpen(&app) && !strcmp(xuiEditGetText(xideRenameViewControl(&app,XIDE_RENAME_ORIGINAL)),alias) && name(&app,"硬链接新名😀.c") && capture(&app,imageAlias) && click(&app,XIDE_RENAME_APPLY));
 int start=0,end=0;
 CHECK(!xidePathExists(alias));
 CHECK(xidePathExists(native));
 CHECK(xidePathExists(newAlias));
 CHECK(xidePathEqual(native,newAlias));
 CHECK(!strcmp(c->path,newAlias));
 CHECK(c->id==cId);
 CHECK(c->editor==cEditor);
 CHECK(c->window==cWindow);
 CHECK(c->baseline.raw==raw);
 CHECK(!strcmp(c->sessionId,cSession));
 CHECK(c->dirty);
 CHECK(xuiCodeDocumentGetVersion(ccore)==cVersion);
 CHECK(xuiCodeSelectionGetRange(xuiCodeEditGetSelection(c->editor),&start,&end)==XUI_OK);
 CHECK(start==2);
 CHECK(end==6);
 CHECK(!strcmp(p->cSources.items[0],newAlias));
 CHECK(!strcmp(q->cSources.items[0],newAlias));
 CHECK(!strcmp(file(&p->files,newAlias)->id,pCId));
 CHECK(!strcmp(file(&q->files,newAlias)->id,qCId));
 CHECK(get(file(&q->files,newAlias)->source,"future"));
 CHECK(app.activeDocument==b);
 CHECK(xuiCodeDocumentGetVersion(bcore)==bVersion);
 CHECK(opened(&app)==2);
 CHECK(xuiCodeDocumentUndo(ccore)==XUI_OK && !c->dirty && xuiCodeDocumentRedo(ccore)==XUI_OK && c->dirty && xideDocumentSave(c,NULL,0,&error) && !c->dirty && xidePathExists(native) && !xidePathEqual(native,newAlias));
 phase(f,"stale-task-prepare");
 CHECK(xideWorkspaceSaveAll(&app,NULL,&error)); char* fake=KEEP(fakeCompiler()); CHECK(fake);
 xide_task_info info={0}; xide_task* stale=xideTasksBuild(app.tasks,p,NULL,fake,1,&error); CHECK(stale);
 phase(f,"stale-task-build-wait"); CHECK(taskWait(&app,stale,&info,XIDE_TASK_BUILD));
 phase(f,"entry-dialog"); CHECK(treeOpen(&app,pId,entryId) && name(&app,"树入口😀.xl"));
 phase(f,"entry-commit"); CHECK(click(&app,XIDE_RENAME_APPLY));
 CHECK(!xidePathExists(entry) && !strcmp(p->entry,newEntry) && !strcmp(q->entry,newEntry) && !xideDocumentsFind(&app.documents,newEntry));
 phase(f,"stale-task-invalidated-wait"); CHECK(taskWait(&app,stale,&info,-1));
 CHECK(info.state==XIDE_TASK_FAILED && info.invalidated && !info.ran && app.activeDocument==b);
 phase(f,"header-resource-commit");
 CHECK(treeOpen(&app,pId,headerId) && name(&app,"树头文件.h") && click(&app,XIDE_RENAME_APPLY) && !xidePathExists(header) && !strcmp(file(&p->files,newHeader)->id,headerId) && !xideDocumentsFind(&app.documents,newHeader));
 CHECK(treeOpen(&app,pId,resourceId) && name(&app,"树资源.xlon") && click(&app,XIDE_RENAME_APPLY) && !xidePathExists(resource) && !strcmp(p->resources.items[0],newResource) && !strcmp(q->resources.items[0],newResource) && !xideDocumentsFind(&app.documents,newResource));
 CHECK(treeOpen(&app,pId,resourceId) && click(&app,XIDE_RENAME_APPLY) && !app.fileRebind && !app.fileRebindReading && xidePathExists(newResource));
 /* Resolve stable file IDs against current model paths, not cached tree paths. */
 CHECK(xideWorkspaceViewSelectFile(&app,pId,resourceId)); xide_file* selected=file(&p->files,newResource); char* cached=selected->path; selected->path=newSmall; CHECK(xideWorkspaceViewSelectedFile(&app)==selected && !strcmp(xideWorkspaceViewSelectedFile(&app)->path,newSmall)); selected->path=cached;
 CHECK(xideWorkspaceSaveAll(&app,NULL,&error) && xideProjectLoad(&round,p->manifest,&error) && get(round.source,"futureRoot") && get(file(&round.files,newSmall)->source,"future") && !strcmp(round.entry,newEntry)); xideProjectClear(&round);
 CHECK(xideSessionSave(&app,0,&error)); char* session=KEEP(xidePathResolve(xideSessionDirectory(&app),"session.json")); state=xrtJsonParseFile(session); CHECK(state && xrtValueCount(get(state,"openDocuments"))==2); xrtValueRelease(state); state=NULL;
 if(f->compiler) { phase(f,"real-build-run"); CHECK(idle(&app)); xide_task* t=xideTasksBuild(app.tasks,p,NULL,f->compiler,1,&error); CHECK(t && taskWait(&app,t,&info,-1)); if(info.state!=XIDE_TASK_SUCCEEDED) fprintf(stderr,"tree real task state=%d invalidated=%d error=%s build=%s\n",info.state,info.invalidated,info.error,xideTaskBuildLog(t)); CHECK(info.state==XIDE_TASK_SUCCEEDED && info.built && info.ran); puts("tree_rename_test real DEMO6 passed: closed entry/header/resource and selected C hardlink references built and ran."); }
 CHECK(!app.frameError && !app.fileRebind && !app.fileRebindReading && app.activeDocument==b && xuiCodeDocumentGetVersion(bcore)==bVersion && b->dirty);
 puts("tree_rename_test passed: workspace F2/project menu scope, closed 20 MiB binary stream/hash identity and preserved timestamp mutation, unchanged-byte atomic replacement, target/source lock/readonly/stale model/selection/document/dirty manifest guards; shared binary and C hardlink exact selected spelling, stable IDs/unknown fields/selection/undo/session and other active dirty editor; closed entry/header/resource rename invalidates old task and preserves document count; two native screenshots.");
cleanup:
 phase(f,"fixture-cleanup");
 xgeQuit(); if(lock!=INVALID_HANDLE_VALUE) CloseHandle(lock); xideTemplatePlanDestroy(plan); xideProjectClear(&round); xrtValueRelease(state); if(began) xgeEnd();
 phase(f,"app-destroy"); xideAppDestroy(&app); for(size_t i=0;i<count;++i) free(owned[i]); phase(f,"fixture-finished"); return XGE_OK;
#undef KEEP
}
static DWORD WINAPI worker(void* user)
{
 fixture* f=user; phase(f,"worker-start");
 if(!SetThreadDesktop(f->desktop)) { fprintf(stderr,"tree_rename_test SetThreadDesktop failed: %lu\n",(unsigned long)GetLastError()); return 1; }
 xge_desc_t d={0}; d.iWidth=1600; d.iHeight=1000; d.sTitle="xlang workspace tree rename verification"; d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; d.iRunMode=XGE_RUN_GAME_LOOP;
 phase(f,"xge-init"); int result=xgeInit(&d);
 if(result==XGE_OK) { phase(f,"xge-run"); result=xgeRun(verify,f); }
 if(result!=XGE_OK || !f->started || f->failed) fprintf(stderr,"tree_rename_test worker result=%d started=%d failed=%d phase=%s check=%ld\n",result,f->started,f->failed,lastPhase(f),(long)InterlockedCompareExchange(&f->checkLine,0,0));
 phase(f,"xge-unit"); xgeUnit(); phase(f,"worker-finished");
 return result==XGE_OK && f->started && !f->failed ? 0 : 1;
}
int main(int argc,char** argv)
{
 setvbuf(stderr,NULL,_IONBF,0);
 if(argc!=2 && argc!=3) { fprintf(stderr,"tree_rename_test requires a fixture directory and optional compiler\n"); return 2; }
 if(!xrtDirCreateAll(argv[1])) { fprintf(stderr,"tree_rename_test cannot create fixture directory: %s\n",argv[1]); return 2; }
 const char* trace=getenv("XIDE_TEST_TRACE");
 fixture f={.directory=argv[1],.compiler=argc==3 ? argv[2] : NULL,.trace=trace && !strcmp(trace,"1")};
 wchar_t name[100]; swprintf(name,100,L"XlangTreeRenameVerification-%lu",GetCurrentProcessId());
 f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);
 if(!f.desktop) { fprintf(stderr,"tree_rename_test CreateDesktop failed: %lu\n",(unsigned long)GetLastError()); return 1; }
 HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1;
 if(!thread) fprintf(stderr,"tree_rename_test CreateThread failed: %lu\n",(unsigned long)GetLastError());
 else {
  DWORD waited=WaitForSingleObject(thread,90000);
  if(waited!=WAIT_OBJECT_0) { fprintf(stderr,"tree_rename_test worker wait=0x%08lx error=%lu phase=%s check=%ld\n",(unsigned long)waited,(unsigned long)GetLastError(),lastPhase(&f),(long)InterlockedCompareExchange(&f.checkLine,0,0)); ExitProcess(1); }
  if(!GetExitCodeThread(thread,&code)) { fprintf(stderr,"tree_rename_test GetExitCodeThread failed: %lu\n",(unsigned long)GetLastError()); code=1; }
  if(code) fprintf(stderr,"tree_rename_test worker exit=0x%08lx phase=%s check=%ld\n",(unsigned long)code,lastPhase(&f),(long)InterlockedCompareExchange(&f.checkLine,0,0));
  CloseHandle(thread);
 }
 CloseDesktop(f.desktop); return (int)code;
}
