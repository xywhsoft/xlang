#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/rename_view.h"
#include "ui/template_view.h"
#include "ui/configuration_view.h"
#include "ui/font_view.h"
#include "ui/recovery_view.h"
#include "ui/file_changes_view.h"
#include "services/ui_font.h"
#include "services/file_rebind.h"
#include "services/project_templates.h"
#include "services/file_sync.h"
#include "services/session.h"
#include "ui/workspace_view.h"
#include "ui/editor_view.h"
#include "ui/references_view.h"
#include "ui/task_view.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct fixture { const char *directory,*compiler; HDESK desktop; int started,failed; } fixture;
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"rename_test:%d: %s; %s\n",__LINE__,#v,error.message); f->failed=1; goto cleanup; } } while(0)
static char* copy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* v,const char* s) { return xrtValueObjectGet(v,sv(s)); }
static int put(xvalue* v,const char* k,const char* s) { return xrtValueObjectSetNew(v,sv(k),xrtValueString(sv(s))); }
static int writeText(const char* p,const char* s) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static xide_file* file(xide_project* p,const char* path) { for(size_t i=0;i<p->files.count;++i) if(xidePathEqual(p->files.items[i].path,path)) return &p->files.items[i]; return NULL; }

static int settle(xide_app* a)
{ if(xuiDispatchPendingEvents(a->context)!=XUI_OK) return 0; xideEditorPoll(a); xideFileSyncPoll(a); xideTaskViewPoll(a); xideWorkbenchLayout(a); return xuiLayout(a->context)==XUI_OK; }
static int idle(xide_app* a)
{ uint64_t end=GetTickCount64()+10000; do { if(!settle(a)) return 0; if(xideFileSyncSettled(a)) return 1; Sleep(5); } while(GetTickCount64()<end); return 0; }
static int taskWait(xide_app* a,xide_task* t,xide_task_info* info,int phase)
{ uint64_t end=GetTickCount64()+30000; do { if(!settle(a)) return 0; xideTaskInfo(t,info); if(phase>=0 ? (int)info->phase==phase : info->state>=XIDE_TASK_SUCCEEDED) return 1; if(phase>=0 && info->state>=XIDE_TASK_SUCCEEDED) return 0; Sleep(5); } while(GetTickCount64()<end); return 0; }
static int capture(xide_app* a,const char* path)
{ if(!settle(a) || xuiUpdate(a->context,0)!=XUI_OK) return 0; xui_rect_i_t r={0,0,a->viewportWidth,a->viewportHeight}; return a->proxy.surfaceClear(&a->proxy,a->target,0xffeeeeeeu)==XUI_OK && xuiRender(a->context,a->target,&r,1)==XUI_OK && xideAppCapture(a,path)==XGE_OK && !a->frameError; }
static int pointer(xide_app* a,xui_rect_t r)
{ float x=r.fX+r.fW/2,y=r.fY+r.fH/2; return xuiInputPointerMove(a->context,x,y,0)==XUI_OK && xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(a); }
static int click(xide_app* a,int id)
{
 xui_widget w=xideRenameViewControl(a,id),scroll=xideDialogGetScrollViewWidget(xideRenameViewWindow(a));
 if(xuiScrollViewEnsureChildVisible(scroll,w)!=XUI_OK || !settle(a)) return 0;
 xui_rect_t r=xuiWidgetGetWorldRect(w),visible=xuiWidgetGetWorldRect(scroll); float right=r.fX+r.fW,bottom=r.fY+r.fH;
 if(r.fX<visible.fX) { r.fX=visible.fX; } if(r.fY<visible.fY) { r.fY=visible.fY; }
 if(right>visible.fX+visible.fW) { right=visible.fX+visible.fW; } if(bottom>visible.fY+visible.fH) { bottom=visible.fY+visible.fH; }
 r.fW=right-r.fX; r.fH=bottom-r.fY; return r.fW>0 && r.fH>0 && pointer(a,r);
}
static int resize(xide_app* a,int width,int height)
{ xui_surface_desc_t d={0}; d.iKind=XUI_SURFACE_KIND_TEXTURE; d.iWidth=width; d.iHeight=height; d.iFormat=XUI_SURFACE_FORMAT_RGBA8; d.iFlags=XUI_SURFACE_USAGE_TARGET|XUI_SURFACE_ALPHA_PREMULTIPLIED; xui_surface next=NULL; if(a->proxy.surfaceCreate(&a->proxy,&next,&d)!=XUI_OK) return 0; a->proxy.surfaceDestroy(&a->proxy,a->target); a->target=next; a->viewportWidth=width; a->viewportHeight=height; return xuiInputViewport(a->context,(float)width,(float)height)==XUI_OK && settle(a); }
static xui_widget firstInput(xui_context c,xui_widget w)
{ if(xuiWidgetGetType(w)==xuiWidgetFindType(c,"input")) return w; for(xui_widget child=xuiWidgetGetFirstChild(w);child;child=xuiWidgetGetNextSibling(child)) { xui_widget found=firstInput(c,child); if(found) return found; } return NULL; }
static int key(xide_app* a,int k,uint32_t modifiers)
{ return xuiInputKeyDownEx(a->context,k,modifiers,NULL)==XUI_OK && xuiInputKeyUpEx(a->context,k,0,NULL)==XUI_OK && settle(a); }
static int identity(const char* path,BY_HANDLE_FILE_INFORMATION* info)
{ wchar_t w[32768]; if(!MultiByteToWideChar(CP_UTF8,0,path,-1,w,32768)) return 0; HANDLE h=CreateFileW(w,0,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL); if(h==INVALID_HANDLE_VALUE) return 0; int okay=GetFileInformationByHandle(h,info); CloseHandle(h); return okay; }
static int sameIdentity(const BY_HANDLE_FILE_INFORMATION* a,const BY_HANDLE_FILE_INFORMATION* b)
{ return a->dwVolumeSerialNumber==b->dwVolumeSerialNumber && a->nFileIndexHigh==b->nFileIndexHigh && a->nFileIndexLow==b->nFileIndexLow; }
static int baselineBytes(const char* path,const xide_text_file* baseline)
{ size_t n=0; unsigned char* raw=xrtFileReadAll(path,&n); int okay=raw && n==baseline->rawLength && !memcmp(raw,baseline->raw,n); xrtFree(raw); return okay; }
static int menuRename(xide_app* a)
{
 int index=-1; for(int i=0;i<xuiMenuGetItemCount(a->fileMenu);++i) if(xuiMenuGetItem(a->fileMenu,i)->iValue==XIDE_ACTION_RENAME_FILE) index=i;
 if(index<0 || xuiMenuOpenAt(a->fileMenu,a->root,80,60)!=XUI_OK || !settle(a)) return 0;
 xui_rect_t r=xuiMenuGetItemRect(a->fileMenu,index),world=xuiWidgetGetWorldRect(a->fileMenu); r.fX+=world.fX; r.fY+=world.fY;
 return xuiInputPointerMove(a->context,r.fX+r.fW/2,r.fY+r.fH/2,0)==XUI_OK && settle(a) && xuiMenuGetHoverIndex(a->fileMenu)==index && pointer(a,r);
}
static int (*savedBefore)(xide_document*,const char*,xide_model_error*,void*);
static int targetRace(xide_document* d,const char* target,xide_model_error* e,void* user)
{ int okay=savedBefore(d,target,e,user); return okay && writeText(target,"int race_occupied = 9;\n"); }
static int verify(void* user)
{
 fixture* f=user; if(f->started) return XGE_OK; f->started=1;
 xide_app app={0}; xide_model_error error={0}; xide_project round={0}; xide_template_plan* plan=NULL; HANDLE lock=INVALID_HANDLE_VALUE; int began=0; xvalue* before=NULL; xide_text_file encoded={0};
 char *root=NULL,*settings=NULL,*sessions=NULL,*workspace=NULL,*first=NULL,*second=NULL,*entry=NULL,*native=NULL,*folder=NULL,*alias=NULL,*newEntry=NULL,*newC=NULL,*caseC=NULL,*blocked=NULL,*wrong=NULL,*resource=NULL,*newResource=NULL,*header=NULL,*newHeader=NULL,*imageXl=NULL,*imageC=NULL,*imageLarge=NULL,*fake=NULL,*sourceId=NULL,*sharedId=NULL,*entryId=NULL,*resourceId=NULL,*headerId=NULL,*sessionId=NULL,*settingsBytes=NULL,*independent=NULL,*changedSyntax=NULL;
 char* unique=xideCreateId("rename"); root=unique ? xidePathResolve(f->directory,unique) : NULL; free(unique); CHECK(root && xrtDirCreateAll(root));
 settings=xidePathResolve(root,"settings.json"); sessions=xidePathResolve(root,"session"); workspace=xidePathResolve(root,"中文工作空间.xlw");
 imageXl=xidePathResolve(root,"native-rename-xlang.png"); imageC=xidePathResolve(root,"native-rename-c.png"); imageLarge=xidePathResolve(root,"native-rename-large-font.png"); blocked=xidePathResolve(root,"已存在.c"); wrong=xidePathResolve(root,"错误.txt"); alias=xidePathResolve(root,"共享硬链接.c"); resource=xidePathResolve(root,"资源.json"); newResource=xidePathResolve(root,"新资源.xlon"); independent=xidePathResolve(root,"独立.c"); changedSyntax=xidePathResolve(root,"独立.xl");
 CHECK(settings && sessions && workspace && imageXl && imageC && imageLarge && blocked && wrong && alias && resource && newResource && independent && changedSyntax);
 app.hidden=1; app.maxFrames=1; app.highDpi=1; app.dpiOverride=1.25f; app.settingsPath=settings; app.sessionRoot=sessions;
 CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1; CHECK(xideActionCount()==74 && !xideActionQuery(&app,XIDE_ACTION_RENAME_FILE).enabled);
 CHECK(xideWorkspaceCreate(&app.workspaceModel,"重命名验证",&error));
 plan=xideTemplatePlanPrepare(root,"原项目😀","mixed-c",&error); CHECK(plan && xideTemplatePlanCommit(&app.workspaceModel,plan,1,&first,&error)); xideTemplatePlanDestroy(plan); plan=NULL;
 plan=xideTemplatePlanPrepare(root,"共享项目","mixed-c",&error); CHECK(plan && xideTemplatePlanCommit(&app.workspaceModel,plan,0,&second,&error)); xideTemplatePlanDestroy(plan); plan=NULL;
 xide_project* p=xideWorkspaceFindProject(&app.workspaceModel,first); xide_project* q=xideWorkspaceFindProject(&app.workspaceModel,second); CHECK(p && q);
 xvalue* seed=xrtJsonParseFile(p->manifest); CHECK(seed && put(seed,"futureRoot","保留根字段")); size_t n=0; char* serialized=xrtJsonStringify(seed,1,&n); CHECK(serialized && xrtFileWriteAtomic(p->manifest,(xbytesview){(const unsigned char*)serialized,n})); xrtFree(serialized);
 xide_project snapshot={0}; CHECK(xideProjectDecode(&snapshot,p->manifest,seed,&error)); xrtValueRelease(seed); snapshot.id=p->id; p->id=NULL; snapshot.referenceSource=p->referenceSource; p->referenceSource=NULL; xideProjectClear(p); *p=snapshot;
 entry=copy(p->entry); native=copy(p->cSources.items[0]); folder=native ? xidePathDirectory(native) : NULL; newC=folder ? xidePathResolveLexical(folder,"MixedRenamed😀.c") : NULL; caseC=folder ? xidePathResolveLexical(folder,"MIXEDrenamed😀.c") : NULL; newEntry=folder ? xidePathResolve(folder,"新入口😀.xl") : NULL; header=folder ? xidePathResolve(folder,"native.h") : NULL; newHeader=folder ? xidePathResolve(folder,"新头文件.h") : NULL; CHECK(entry && native && newC && caseC && newEntry && header && newHeader);
 const char* cBody="#include <stdint.h>\nint native_answer(void)\n{\n    return 42;\n}\n";
 CHECK(xideTextFileEncode(cBody,(int)strlen(cBody),1,XUI_CODE_EOL_CRLF,&encoded,&error) && xrtFileWriteAtomic(native,(xbytesview){encoded.raw,encoded.rawLength})); xideTextFileClear(&encoded);
 wchar_t oldW[32768],aliasW[32768]; CHECK(MultiByteToWideChar(CP_UTF8,0,native,-1,oldW,32768) && MultiByteToWideChar(CP_UTF8,0,alias,-1,aliasW,32768) && CreateHardLinkW(aliasW,oldW,NULL));
 char* oldEntryId=copy(file(q,q->entry)->id); char* oldCId=copy(file(q,q->cSources.items[0])->id); CHECK(oldEntryId && oldCId && xideProjectSetEntry(q,entry,&error) && xideProjectRemoveFile(q,oldEntryId,&error) && xideProjectRemoveFile(q,oldCId,&error) && xideProjectAddFile(q,alias,"c",&error)); free(oldEntryId); free(oldCId);
 CHECK(xideWorkspaceAddFile(&app.workspaceModel,alias,&error) && put(file(p,native)->source,"future","保留😀") && put(file(q,alias)->source,"future","共享字段"));
 sourceId=copy(file(p,native)->id); sharedId=copy(file(q,alias)->id); entryId=copy(file(p,entry)->id); headerId=copy(file(p,header)->id); CHECK(sourceId && sharedId && entryId && headerId);
 CHECK(writeText(resource,"{\"format\":\"xlang-windows-resource-1\",\"version\":{\"fileVersion\":\"2.3.4.5\",\"productVersion\":\"2.3.4.5\"}}") && xideProjectAddFile(p,resource,"resource-config",&error) && xideProjectAddFile(q,resource,"resource-config",&error)); resourceId=copy(file(p,resource)->id); CHECK(resourceId);
 CHECK(writeText(entry,"// FAKE_SLOW\nextern \"native_answer\" func nativeAnswer() as c_int\nfunc main(...) as int\n    if nativeAnswer() != 42 then\n        return 1\n    end\n    return 0\nend\n") && xideWorkspaceViewRefresh(&app)==XUI_OK && xideWorkspaceViewSelectProject(&app,first) && xideWorkspaceSaveAll(&app,workspace,&error) && idle(&app));
 CHECK(xideEditorOpen(&app,native,&error) && settle(&app)); xide_document* c=app.activeDocument; xui_code_document core=xuiCodeEditGetDocument(c->editor); CHECK(c->baseline.bom && c->baseline.eol==XUI_CODE_EOL_CRLF && xuiCodeDocumentInsert(core,0,"// 本地未保存😀\n")==XUI_OK && c->dirty);
 CHECK(xuiCodeSelectionSetRange(xuiCodeEditGetSelection(c->editor),core,2,6)==XUI_OK); uint32_t version=xuiCodeDocumentGetVersion(core); uint64_t id=c->id; xui_widget editor=c->editor; int window=c->window; sessionId=copy(c->sessionId); CHECK(sessionId);
 CHECK(xuiSetFocusWidget(app.context,editor)==XUI_OK && key(&app,XUI_KEY_F2,0) && xideRenameViewIsOpen(&app) && !xideActionQuery(&app,XIDE_ACTION_SAVE_FILE).enabled);
 CHECK(!xideTemplateViewOpen(&app) && !xideReferencesViewOpen(&app) && !xideConfigurationViewOpen(&app) && !xideFontViewOpen(&app) && !xideRecoveryViewOpen(&app) && !xideFileChangesViewOpen(&app));
 const char* invalid[]={"CON.c","CON .c","LPT1.c","a?.c","a/b.c","a\\b.c","trailing.c.",". ","NUL","wrong.xl"};
 for(size_t i=0;i<sizeof(invalid)/sizeof(*invalid);++i) CHECK(xuiEditSetText(xideRenameViewControl(&app,XIDE_RENAME_NAME),invalid[i])==XUI_OK && !xideRenameViewRefresh(&app) && !xuiWidgetGetEnabled(xideRenameViewControl(&app,XIDE_RENAME_APPLY)) && xidePathExists(native));
 CHECK(xuiEditSetText(xideRenameViewControl(&app,XIDE_RENAME_NAME),"取消的源码.c")==XUI_OK && xuiSetFocusWidget(app.context,xideRenameViewControl(&app,XIDE_RENAME_NAME))==XUI_OK && key(&app,'Z',XUI_MOD_CTRL) && key(&app,XUI_KEY_F2,0) && xuiCodeDocumentGetVersion(core)==version && xideRenameViewIsOpen(&app) && click(&app,XIDE_RENAME_CANCEL) && !xideRenameViewIsOpen(&app));
 before=xideProjectExport(p,&error); CHECK(before && writeText(blocked,"int occupied = 8;\n"));
 CHECK(!xideDocumentRename(c,blocked,&error) && error.code==XIDE_MODEL_CONFLICT && xidePathExists(native) && !strcmp(c->path,native) && c->dirty && !app.fileRebind && !app.fileRebindReading);
 CHECK(!xideDocumentRename(c,wrong,&error) && error.code==XIDE_MODEL_INVALID && !xidePathExists(wrong));
 CHECK(MultiByteToWideChar(CP_UTF8,0,native,-1,oldW,32768)); lock=CreateFileW(oldW,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL); CHECK(lock!=INVALID_HANDLE_VALUE);
 CHECK(!xideDocumentRename(c,newC,&error) && error.code==XIDE_MODEL_IO && !xidePathExists(newC) && c->dirty && !app.fileRebind); CloseHandle(lock); lock=INVALID_HANDLE_VALUE;
 DWORD attributes=GetFileAttributesW(oldW); CHECK(attributes!=INVALID_FILE_ATTRIBUTES && SetFileAttributesW(oldW,attributes|FILE_ATTRIBUTE_READONLY)); CHECK(!xideDocumentRename(c,newC,&error) && error.code==XIDE_MODEL_IO && xidePathExists(native) && !xidePathExists(newC)); CHECK(SetFileAttributesW(oldW,attributes));
 CHECK(writeText(native,"int externally_changed = 1;\n") && !xideDocumentRename(c,newC,&error) && error.code==XIDE_MODEL_CONFLICT && !xidePathExists(newC) && strstr(xuiCodeEditGetText(editor),"本地未保存"));
 CHECK(xrtFileWriteAtomic(native,(xbytesview){c->baseline.raw,c->baseline.rawLength}));
 /* Atomic replacement broke the hard link; recreate it before the shared rename. */
 CHECK(MultiByteToWideChar(CP_UTF8,0,alias,-1,aliasW,32768) && DeleteFileW(aliasW) && CreateHardLinkW(aliasW,oldW,NULL));
 savedBefore=app.documents.beforeSave; app.documents.beforeSave=targetRace; CHECK(!xideDocumentRename(c,newC,&error) && error.code==XIDE_MODEL_CONFLICT && xidePathExists(native) && xidePathExists(newC)); app.documents.beforeSave=savedBefore;
 CHECK(MultiByteToWideChar(CP_UTF8,0,newC,-1,aliasW,32768) && DeleteFileW(aliasW));
 xvalue* after=xideProjectExport(p,&error); CHECK(after && xrtValueEqual(before,after)); xrtValueRelease(after); xrtValueRelease(before); before=NULL;
 CHECK(xideEditorOpen(&app,blocked,&error) && !xideDocumentRename(c,blocked,&error) && error.code==XIDE_MODEL_CONFLICT);
 CHECK(xideWorkspaceViewSelectProject(&app,first) && xideReferencesViewOpen(&app) && !xideDocumentRename(c,newC,&error) && error.code==XIDE_MODEL_CONFLICT); CHECK(xideDialogSetOpen(xideReferencesViewWindow(&app),0)==XUI_OK);
 CHECK(xideEditorOpen(&app,p->manifest,&error)); xide_document* md=app.activeDocument; CHECK(!xideActionQuery(&app,XIDE_ACTION_RENAME_FILE).enabled && !xideRenameViewOpen(&app) && xuiCodeDocumentInsert(xuiCodeEditGetDocument(md->editor),0," ")==XUI_OK && !xideDocumentRename(c,newC,&error) && error.code==XIDE_MODEL_CONFLICT); CHECK(xideDocumentReload(md,1,&error));
 CHECK(xideEditorShow(&app,c)==XUI_OK && xideRenameViewOpen(&app) && xuiEditSetText(xideRenameViewControl(&app,XIDE_RENAME_NAME),"MixedRenamed😀.c")==XUI_OK && writeText(newC,"int existing = 3;\n") && !xideRenameViewApply(&app,&error) && xideRenameViewIsOpen(&app) && !strcmp(xuiEditGetText(xideRenameViewControl(&app,XIDE_RENAME_NAME)),"MixedRenamed😀.c") && click(&app,XIDE_RENAME_CANCEL) && DeleteFileW(aliasW));
 CHECK(xideRenameViewOpen(&app) && xrtValueObjectSetNew(app.workspaceModel.source,sv("temporaryFuture"),xrtValueBool(1)) && !xideRenameViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && click(&app,XIDE_RENAME_CANCEL) && xrtValueObjectRemove(app.workspaceModel.source,sv("temporaryFuture")));
 CHECK(xideRenameViewOpen(&app) && xuiCodeDocumentInsert(core,0,"// 过期版本\n")==XUI_OK && !xideRenameViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && click(&app,XIDE_RENAME_CANCEL) && xuiCodeDocumentUndo(core)==XUI_OK);
 xui_widget property=firstInput(app.context,xuiScrollViewGetContentWidget(app.properties)); CHECK(property && xuiEditSetText(property,"未应用属性")==XUI_OK && !xideRenameViewOpen(&app) && !xideActionQuery(&app,XIDE_ACTION_RENAME_FILE).enabled); CHECK(xuiEditSetText(property,p->name)==XUI_OK);
 CHECK(xideWorkspaceViewSelectFile(&app,first,sourceId)); int selected=xuiTreeViewGetSelected(app.workspace); version=xuiCodeDocumentGetVersion(core);
 xui_code_fold_range_t fold[16]; int foldCount=0; CHECK(xuiCodeFoldStateGetRanges(xuiCodeEditGetFoldState(editor),fold,16,&foldCount)==XUI_OK && foldCount>0); fold[0].iFlags|=XUI_CODE_FOLD_COLLAPSED; CHECK(xuiCodeFoldStateSetRanges(xuiCodeEditGetFoldState(editor),fold,foldCount)==XUI_OK);
 BY_HANDLE_FILE_INFORMATION oldIdentity={0},newIdentity={0}; CHECK(identity(native,&oldIdentity)); unsigned char* baselineRaw=c->baseline.raw;
 CHECK(menuRename(&app) && xideRenameViewIsOpen(&app) && xuiEditSetText(xideRenameViewControl(&app,XIDE_RENAME_NAME),"MixedRenamed😀.c")==XUI_OK && capture(&app,imageC) && click(&app,XIDE_RENAME_APPLY));
 int a=0,b=0; CHECK(!xideRenameViewIsOpen(&app) && !xidePathExists(native) && baselineBytes(newC,&c->baseline) && identity(newC,&newIdentity) && sameIdentity(&oldIdentity,&newIdentity) && c->id==id && c->editor==editor && c->window==window && !strcmp(c->sessionId,sessionId) && c->dirty && c->baseline.raw==baselineRaw && xuiCodeDocumentGetVersion(core)==version && xuiCodeSelectionGetRange(xuiCodeEditGetSelection(editor),&a,&b)==XUI_OK && a==2 && b==6 && !strcmp(c->language,"c"));
 CHECK(xuiCodeFoldStateGetRanges(xuiCodeEditGetFoldState(editor),fold,16,&foldCount)==XUI_OK && foldCount>0 && (fold[0].iFlags&XUI_CODE_FOLD_COLLAPSED));
 CHECK(xidePathEqual(alias,newC) && xidePathEqual(p->cSources.items[0],newC) && xidePathEqual(q->cSources.items[0],newC) && xidePathEqual(app.workspaceModel.files.items[0].path,newC) && !strcmp(file(p,newC)->id,sourceId) && !strcmp(file(q,newC)->id,sharedId) && get(file(p,newC)->source,"future") && p->dirty && q->dirty && app.workspaceModel.dirty && xuiTreeViewGetSelected(app.workspace)==selected);
 CHECK(xuiCodeDocumentUndo(core)==XUI_OK && !c->dirty && xuiCodeDocumentRedo(core)==XUI_OK && c->dirty && xideDocumentSave(c,NULL,0,&error) && !c->dirty && c->baseline.bom && c->baseline.eol==XUI_CODE_EOL_CRLF);
 CHECK(xideDocumentRename(c,caseC,&error) && !strcmp(c->path,caseC) && !strcmp(p->cSources.items[0],caseC) && !strcmp(q->cSources.items[0],caseC)); char* canonical=xidePathResolve(NULL,caseC); CHECK(canonical && !strcmp(xidePathFilename(canonical),"MIXEDrenamed😀.c")); free(canonical);
 CHECK(xideDocumentRename(c,caseC,&error) && !app.fileRebind && !app.fileRebindReading);
 CHECK(xideEditorOpen(&app,entry,&error) && settle(&app)); xide_document* xl=app.activeDocument; version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xl->editor)); CHECK(identity(entry,&oldIdentity));
 wchar_t executable[32768]; CHECK(GetModuleFileNameW(NULL,executable,32768)); wchar_t* slash=wcsrchr(executable,L'\\'); CHECK(slash); wcscpy(slash+1,L"task_test.exe"); int count=WideCharToMultiByte(CP_UTF8,0,executable,-1,NULL,0,NULL,NULL); fake=count ? malloc((size_t)count) : NULL; CHECK(fake && WideCharToMultiByte(CP_UTF8,0,executable,-1,fake,count,NULL,NULL));
 CHECK(xideWorkspaceSaveAll(&app,NULL,&error)); xide_task* stale=xideTasksBuild(app.tasks,p,NULL,fake,1,&error); xide_task_info info={0}; CHECK(stale && taskWait(&app,stale,&info,XIDE_TASK_BUILD));
 CHECK(xuiSetFocusWidget(app.context,xl->editor)==XUI_OK && key(&app,XUI_KEY_F2,0) && xuiEditSetText(xideRenameViewControl(&app,XIDE_RENAME_NAME),"新入口😀.xl")==XUI_OK && capture(&app,imageXl) && click(&app,XIDE_RENAME_APPLY));
 CHECK(!xidePathExists(entry) && baselineBytes(newEntry,&xl->baseline) && identity(newEntry,&newIdentity) && sameIdentity(&oldIdentity,&newIdentity) && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xl->editor))==version && !xl->dirty && !strcmp(xl->language,"xlang") && !strcmp(file(p,newEntry)->id,entryId) && xidePathEqual(p->entry,newEntry) && xidePathEqual(q->entry,newEntry) && taskWait(&app,stale,&info,-1) && info.state==XIDE_TASK_FAILED && info.invalidated && !info.ran);
 CHECK(xideEditorOpen(&app,resource,&error) && xideDocumentRename(app.activeDocument,newResource,&error) && !xidePathExists(resource) && xidePathEqual(p->resources.items[0],newResource) && xidePathEqual(q->resources.items[0],newResource) && !strcmp(file(p,newResource)->id,resourceId));
 CHECK(xideEditorOpen(&app,header,&error) && xideDocumentRename(app.activeDocument,newHeader,&error) && !xidePathExists(header) && !strcmp(file(p,newHeader)->id,headerId) && !strcmp(app.activeDocument->language,"c") && p->cSources.count==1);
 CHECK(writeText(independent,"func main(...) as int\n    return 0\nend\n") && xideEditorOpen(&app,independent,&error) && xideDocumentRename(app.activeDocument,changedSyntax,&error) && !xidePathExists(independent) && !strcmp(app.activeDocument->language,"xlang"));
 CHECK(xideEditorShow(&app,c)==XUI_OK && xideRenameViewOpen(&app) && xuiEditSetText(xideRenameViewControl(&app,XIDE_RENAME_NAME),"最后源码😀.c")==XUI_OK && xideUiFontApply(&app,app.uiFontPath,40,&error) && resize(&app,700,460));
 CHECK(xuiInputGetFont(xideRenameViewControl(&app,XIDE_RENAME_NAME))==app.uiFont);
 settingsBytes=xrtJsonStringify(app.preferences.source,1,&n); CHECK(settingsBytes && writeText(settings,"{\"format\":\"xlang-ide-settings-1\",\"external\":true}")); xui_font oldFont=app.uiFont;
 CHECK(!xideUiFontApply(&app,app.uiFontPath,18,&error) && app.uiFont==oldFont && app.uiFontSize==40 && !strcmp(xuiEditGetText(xideRenameViewControl(&app,XIDE_RENAME_NAME)),"最后源码😀.c")); CHECK(writeText(settings,settingsBytes)); xrtFree(settingsBytes); settingsBytes=NULL;
 CHECK(xuiScrollViewEnsureChildVisible(xideDialogGetScrollViewWidget(xideRenameViewWindow(&app)),xideRenameViewControl(&app,XIDE_RENAME_CANCEL))==XUI_OK && settle(&app) && xuiScrollViewEnsureChildVisible(xideDialogGetScrollViewWidget(xideRenameViewWindow(&app)),xideRenameViewControl(&app,XIDE_RENAME_APPLY))==XUI_OK && capture(&app,imageLarge) && click(&app,XIDE_RENAME_APPLY) && !xideRenameViewIsOpen(&app));
 free(caseC); caseC=copy(c->path); CHECK(caseC && xideUiFontApply(&app,app.uiFontPath,16,&error) && resize(&app,1280,800) && xideWorkspaceSaveAll(&app,NULL,&error) && xideProjectLoad(&round,p->manifest,&error) && !strcmp(file(&round,caseC)->id,sourceId) && get(round.source,"futureRoot")); xideProjectClear(&round);
 CHECK(idle(&app) && writeText(caseC,"int native_answer(void) { return 42; } // 外部😀\n")); uint64_t until=GetTickCount64()+10000; do { CHECK(settle(&app)); if(strstr(xuiCodeEditGetText(c->editor),"外部😀")) break; Sleep(5); } while(GetTickCount64()<until); CHECK(strstr(xuiCodeEditGetText(c->editor),"外部😀") && !c->dirty);
 CHECK(xideSessionSave(&app,0,&error)); char* session=xidePathResolve(xideSessionDirectory(&app),"session.json"); CHECK(session); xvalue* saved=xrtJsonParseFile(session); free(session); CHECK(saved); int found=0; for(size_t i=0;i<xrtValueCount(get(saved,"openDocuments"));++i) { xvalue* item=xrtValueArrayGet(get(saved,"openDocuments"),i); xstrview path={0}; if(xrtValueGetString(get(item,"path"),&path)) { char* absolute=xidePathResolve(root,path.Data); if(absolute && xidePathEqual(absolute,caseC)) found=1; free(absolute); } } xrtValueRelease(saved); CHECK(found);
 if(f->compiler) { xide_task* task=xideTasksBuild(app.tasks,p,NULL,f->compiler,1,&error); CHECK(task && taskWait(&app,task,&info,-1) && info.state==XIDE_TASK_SUCCEEDED && info.built && info.ran); puts("rename_test real DEMO6 passed: renamed entry/C/header/resource references built and ran."); }
 CHECK(!app.frameError && !app.fileRebind && !app.fileRebindReading);
 puts("rename_test passed: dirty/clean C/xlang rename preserves file bytes/identity, BOM/CRLF, document/version/selection/folds/undo; shared entry/C/header/resource/hardlink/workspace references and unknown fields; target race/existence/source lock/readonly/disk changes, stale model/document/property/modal/manifest guards; real F2/menu/cancel/failure/apply and plain-field isolation; case-only spelling, syntax extension, font rollback and 125%/40 DIP two-axis controls; actual watcher/session/stale task and three native screenshots.");
cleanup:
 xgeQuit(); if(lock!=INVALID_HANDLE_VALUE) CloseHandle(lock); xideTemplatePlanDestroy(plan); xrtValueRelease(before); xideTextFileClear(&encoded); xideProjectClear(&round); if(began) xgeEnd(); xideAppDestroy(&app);
 free(root); free(settings); free(sessions); free(workspace); free(first); free(second); free(entry); free(native); free(folder); free(alias); free(newEntry); free(newC); free(caseC); free(blocked); free(wrong); free(resource); free(newResource); free(header); free(newHeader); free(imageXl); free(imageC); free(imageLarge); free(fake); free(sourceId); free(sharedId); free(entryId); free(resourceId); free(headerId); free(sessionId); xrtFree(settingsBytes); free(independent); free(changedSyntax); return XGE_OK;
}
static DWORD WINAPI worker(void* user)
{ fixture* f=user; if(!SetThreadDesktop(f->desktop)) return 1; xge_desc_t d={0}; d.iWidth=1600; d.iHeight=1000; d.sTitle="xlang physical rename verification"; d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; d.iRunMode=XGE_RUN_GAME_LOOP; int r=xgeInit(&d); if(r==XGE_OK) r=xgeRun(verify,f); xgeUnit(); return r==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv)
{ if((argc!=2 && argc!=3) || !xrtDirCreateAll(argv[1])) return 2; fixture f={.directory=argv[1],.compiler=argc==3 ? argv[2] : NULL}; wchar_t name[100]; swprintf(name,100,L"XlangRenameVerification-%lu",GetCurrentProcessId()); f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) return 1; HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1; if(thread) { if(WaitForSingleObject(thread,90000)!=WAIT_OBJECT_0) ExitProcess(1); GetExitCodeThread(thread,&code); CloseHandle(thread); } CloseDesktop(f.desktop); return (int)code; }
