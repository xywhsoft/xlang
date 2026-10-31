#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/resource_view.h"
#include "ui/workspace_view.h"
#include "ui/editor_view.h"
#include "ui/references_view.h"
#include "ui/configuration_view.h"
#include "ui/import_view.h"
#include "ui/template_view.h"
#include "ui/font_view.h"
#include "ui/recovery_view.h"
#include "ui/file_changes_view.h"
#include "ui/task_view.h"
#include "services/file_sync.h"
#include "services/ui_font.h"
#include "services/resource_creation.h"
#include "model/resources.h"
#include "platform/path.h"
#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct fixture { const char *directory,*compiler; HDESK desktop; int started,failed; } fixture;
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"resource_creation_test:%d: %s; %s; %s\n",__LINE__,#v,error.message,xideResourceViewMessage(&app)); f->failed=1; goto cleanup; } } while(0)
static const char* xml="<?xml version=\"1.0\" encoding=\"UTF-8\"?><assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\"><assemblyIdentity version=\"1.0.0.0\" processorArchitecture=\"*\" name=\"Xlang.IDE.Test\" type=\"win32\"/></assembly>";
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* v,const char* key) { return v ? xrtValueObjectGet(v,sv(key)) : NULL; }
static int writeText(const char* p,const char* s) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static int settle(xide_app* a) { if(xuiDispatchPendingEvents(a->context)!=XUI_OK) return 0; xideEditorPoll(a); xideFileSyncPoll(a); xideTaskViewPoll(a); xideWorkbenchLayout(a); return xuiLayout(a->context)==XUI_OK && !a->frameError; }
static int pointer(xide_app* a,xui_rect_t r) { float x=r.fX+r.fW/2,y=r.fY+r.fH/2; return xuiInputPointerMove(a->context,x,y,0)==XUI_OK && xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(a); }
static int click(xide_app* a,int id)
{
 xui_widget w=xideResourceViewControl(a,id),scroll=xideDialogGetScrollViewWidget(xideResourceViewWindow(a)); if(xuiScrollViewEnsureChildVisible(scroll,w)!=XUI_OK || !settle(a)) return 0;
 xui_rect_t r=xuiWidgetGetWorldRect(w),visible=xuiWidgetGetWorldRect(scroll); float right=r.fX+r.fW,bottom=r.fY+r.fH;
 if(r.fX<visible.fX) r.fX=visible.fX;
 if(r.fY<visible.fY) r.fY=visible.fY;
 if(right>visible.fX+visible.fW) right=visible.fX+visible.fW;
 if(bottom>visible.fY+visible.fH) bottom=visible.fY+visible.fH;
 r.fW=right-r.fX; r.fH=bottom-r.fY; return r.fW>0 && r.fH>0 && pointer(a,r);
}
static int choose(xide_app* a,int id,int index)
{
 xui_widget w=xideResourceViewControl(a,id); if(xuiScrollViewEnsureChildVisible(xideDialogGetScrollViewWidget(xideResourceViewWindow(a)),w)!=XUI_OK || !settle(a) || xuiComboBoxOpen(w)!=XUI_OK || !settle(a)) return 0;
 xui_widget m=xuiComboBoxGetMenuWidget(w); xui_rect_t r=xuiMenuGetItemRect(m,index),world=xuiWidgetGetWorldRect(m); r.fX+=world.fX; r.fY+=world.fY; return pointer(a,r) && xuiComboBoxGetSelected(w)==index;
}
static xui_widget findButton(xui_context c,xui_widget w,const char* name)
{
 if(xuiWidgetGetType(w)==xuiWidgetFindType(c,"button") && !strcmp(xuiButtonGetText(w),name)) return w;
 for(xui_widget child=xuiWidgetGetFirstChild(w);child;child=xuiWidgetGetNextSibling(child)) { xui_widget found=findButton(c,child,name); if(found) return found; }
 return NULL;
}
static int menuOpen(xide_app* a,int action)
{
 int i=-1; for(int j=0;j<xuiMenuGetItemCount(a->projectMenu);++j) if(xuiMenuGetItem(a->projectMenu,j)->iValue==action) i=j;
 if(i<0 || xuiMenuOpenAt(a->projectMenu,a->root,80,60)!=XUI_OK || !settle(a)) return 0;
 xui_rect_t r=xuiMenuGetItemRect(a->projectMenu,i),world=xuiWidgetGetWorldRect(a->projectMenu); r.fX+=world.fX; r.fY+=world.fY; return pointer(a,r);
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
static int pe(const char* path,int resources)
{
 wchar_t w[32768]; if(!MultiByteToWideChar(CP_UTF8,0,path,-1,w,32768)) return 0;
 DWORD ignored=0,size=GetFileVersionInfoSizeW(w,&ignored); void* data=size ? malloc(size) : NULL; int okay=0;
 if(data && GetFileVersionInfoW(w,0,size,data)) { VS_FIXEDFILEINFO* v=NULL; UINT length=0; okay=VerQueryValueW(data,L"\\",(void**)&v,&length) && length>=sizeof(*v) && v->dwFileVersionMS==MAKELONG(8,9) && v->dwFileVersionLS==MAKELONG(6,7); }
 free(data); if(!okay || !resources) return okay;
 HMODULE module=LoadLibraryExW(w,NULL,LOAD_LIBRARY_AS_DATAFILE|LOAD_LIBRARY_AS_IMAGE_RESOURCE); if(!module) return 0;
 HRSRC icon=FindResourceExW(module,MAKEINTRESOURCEW(3),MAKEINTRESOURCEW(1),1033),group=FindResourceExW(module,MAKEINTRESOURCEW(14),MAKEINTRESOURCEW(1),1033),manifest=FindResourceExW(module,MAKEINTRESOURCEW(24),MAKEINTRESOURCEW(1),1033);
 HGLOBAL body=manifest ? LoadResource(module,manifest) : NULL; void* bytes=body ? LockResource(body) : NULL; okay=icon && group && manifest && bytes && SizeofResource(module,icon)==48 && SizeofResource(module,manifest)==strlen(xml) && !memcmp(bytes,xml,strlen(xml)); FreeLibrary(module); return okay;
}
static int taskWait(xide_app* a,xide_task* task,xide_task_info* info)
{ uint64_t end=GetTickCount64()+45000; do { if(!settle(a)) return 0; xideTaskInfo(task,info); if(info->state>=XIDE_TASK_SUCCEEDED) return 1; Sleep(5); } while(GetTickCount64()<end); return 0; }
static int tJunction(const char* path,const char* target)
{
    wchar_t p[4096],t[4096],sub[4100]; if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,p,4096) || !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,target,-1,t,4096)) return 0;
    for(wchar_t* c=t;*c;++c) if(*c=='/') *c='\\';
    swprintf(sub,4100,L"\\??\\%ls",t); size_t a=wcslen(sub),b=wcslen(t);
    struct { DWORD tag; WORD bytes,reserved,subOffset,subLength,printOffset,printLength; wchar_t buffer[8200]; } data={0};
    data.tag=IO_REPARSE_TAG_MOUNT_POINT; data.subLength=(WORD)(a*2); data.printOffset=(WORD)((a+1)*2); data.printLength=(WORD)(b*2); data.bytes=(WORD)(8+(a+b+2)*2);
    memcpy(data.buffer,sub,(a+1)*2); memcpy(data.buffer+a+1,t,(b+1)*2);
    if(!CreateDirectoryW(p,NULL)) return 0;
    HANDLE h=CreateFileW(p,GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL); DWORD transferred=0;
    int okay=h!=INVALID_HANDLE_VALUE && DeviceIoControl(h,FSCTL_SET_REPARSE_POINT,&data,data.bytes+8,NULL,0,&transferred,NULL);
    if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
    if(!okay) RemoveDirectoryW(p);
    return okay;
}
static HANDLE lockPath(const char* path,DWORD access,int directory)
{ wchar_t w[32768]; if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,w,32768)) return INVALID_HANDLE_VALUE; return CreateFileW(w,access,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,directory ? FILE_FLAG_BACKUP_SEMANTICS : 0,NULL); }
static int move(const char* old,const char* next)
{ wchar_t a[32768],b[32768]; return MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,old,-1,a,32768) && MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,next,-1,b,32768) && MoveFileW(a,b); }
static int exists(const char* path) { xfileinfo info={0}; return xrtPathStat(path,false,&info); }
static int verify(void* user)
{
 fixture* f=user; if(f->started) return XGE_OK; f->started=1;
 xide_app app={0}; xide_model_error error={0}; xide_project project={0},compiled={0}; int began=0;
 xide_resource_plan* plan=NULL; xide_resource_config parsed={0}; xvalue *saved=NULL,*now=NULL,*references=NULL; char *manifestBytes=NULL,*previewBytes=NULL; size_t manifestLength=0;
 HANDLE lock=INVALID_HANDLE_VALUE; char* owned[40]={0}; size_t ownedCount=0;
#define KEEP(s) (owned[ownedCount++]=(s))
 char* id=xideCreateId("resource-create"); char* root=KEEP(id ? xidePathResolve(f->directory,id) : NULL); free(id); CHECK(root && xrtDirCreateAll(root));
 char *entry=KEEP(xidePathResolve(root,"main.xl")),*native=KEEP(xidePathResolve(root,"native.c")),*manifest=KEEP(xidePathResolve(root,"app.xlp")),*beta=KEEP(xidePathResolve(root,"beta.xlp")),*workspace=KEEP(xidePathResolve(root,"workspace.xlw")),*settings=KEEP(xidePathResolve(root,"settings.json"));
 char *resDir=KEEP(xidePathResolve(root,"资源😀")),*resource=KEEP(xidePathResolve(resDir,"新建.res.json")),*typed=KEEP(xidePathResolve(resDir,"新建.res.xlon")),*icon=KEEP(xidePathResolve(resDir,"app.ico")),*race=KEEP(xidePathResolve(resDir,"竞争.json")),*guard=KEEP(xidePathResolve(root,"guard")),*moved=KEEP(xidePathResolve(root,"guard-moved")),*guardFile=KEEP(xidePathResolve(guard,"new.json"));
 char *image=KEEP(xidePathResolve(root,"native-resource-create.png")),*imagePreview=KEEP(xidePathResolve(root,"native-resource-create-preview.png")),*imageLarge=KEEP(xidePathResolve(root,"native-resource-create-large-font.png")),*outManifest=KEEP(xidePathResolve(root,"typed-build.xlp"));
 CHECK(entry && native && manifest && beta && workspace && settings && resDir && resource && typed && icon && race && guard && moved && guardFile && image && imagePreview && imageLarge && outManifest && xrtDirCreateAll(resDir) && xrtDirCreateAll(guard));
 CHECK(writeText(entry,"extern \"native_answer\" func nativeAnswer() as c_int\nfunc main(...) as int\n    return nativeAnswer()-42\nend\n") && writeText(native,"int native_answer(void) { return 42; }\n"));
 unsigned char ico[70]={0}; ico[2]=1; ico[4]=1; ico[6]=1; ico[7]=1; ico[10]=1; ico[12]=32; ico[14]=48; ico[18]=22; ico[22]=40; ico[26]=1; ico[30]=2; ico[34]=1; ico[36]=32; ico[42]=4; ico[62]=0x44; ico[63]=0x88; ico[64]=0xcc; ico[65]=0xff;
 CHECK(xrtFileWriteAtomic(icon,(xbytesview){ico,sizeof(ico)}));
 app.hidden=1; app.noSession=1; app.highDpi=1; app.dpiOverride=1.25f; app.settingsPath=settings;
 CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1;
 CHECK(!!xideActionFind(XIDE_ACTION_C_SERVER)==XIDE_ENABLE_C_LSP && xideActionNamed("project.newResource")->id==39 && !xideActionQuery(&app,XIDE_ACTION_NEW_RESOURCE).enabled && xideWorkspaceCreate(&app.workspaceModel,"资源创建验收",&error));
 CHECK(xideProjectCreate(&project,manifest,"C 与 xlang 新资源😀",entry,&error) && xideProjectAddFile(&project,native,"c",&error) && xideProjectSave(&project,NULL,&error) && xideWorkspaceAddProject(&app.workspaceModel,manifest,"alpha",&error)); xideProjectClear(&project);
 CHECK(xideProjectCreate(&project,beta,"独立启动项目",entry,&error) && xideProjectSave(&project,NULL,&error) && xideWorkspaceAddProject(&app.workspaceModel,beta,"beta",&error) && xideWorkspaceSetStartup(&app.workspaceModel,"beta",&error) && xideWorkspaceSaveAll(&app,workspace,&error) && xideWorkspaceViewSelectProject(&app,"alpha")); xideProjectClear(&project);
 xide_project* p=xideWorkspaceFindProject(&app.workspaceModel,"alpha"); CHECK(p && !p->resources.count && xideActionQuery(&app,XIDE_ACTION_NEW_RESOURCE).enabled && !xideActionQuery(&app,XIDE_ACTION_RESOURCES).enabled);
 CHECK(xideEditorOpen(&app,entry,&error)); xide_document* xl=app.activeDocument; CHECK(xideEditorOpen(&app,native,&error)); xide_document* c=app.activeDocument;
 CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(c->editor),0,"// unsaved C\n")==XUI_OK && xuiCodeDocumentInsert(xuiCodeEditGetDocument(xl->editor),0,"// unsaved xlang\n")==XUI_OK && token(xl->editor,"extern",XUI_CODE_TOKEN_KEYWORD) && token(c->editor,"native_answer",XUI_CODE_TOKEN_IDENTIFIER));
 CHECK(xuiCodeSelectionSetRange(xuiCodeEditGetSelection(c->editor),xuiCodeEditGetDocument(c->editor),1,8)==XUI_OK);
 uint32_t cv=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(c->editor)),xv=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xl->editor));
 saved=xideProjectExport(p,&error); manifestBytes=(char*)xrtFileReadAll(manifest,&manifestLength); CHECK(saved && manifestBytes);
 const char* bad[]={"","bad.txt","CON.json","NUL.xlon","COM1.json","LPT².json","bad:stream.json","bad?.json","bad.json "};
 for(size_t i=0;i<sizeof(bad)/sizeof(*bad);++i) { plan=xideResourcePlanPrepare(p,bad[i],"{\"version\":{\"fileVersion\":\"1.0.0.0\"}}",NULL,&error); if(plan) fprintf(stderr,"invalid path accepted: %s -> %s\n",bad[i],xideResourcePlanPath(plan)); CHECK(!plan); }
 CHECK(!xideResourcePlanPrepare(p,resource,"{",NULL,&error) && !xideResourcePlanPrepare(p,resource,"{\"icon\":{\"path\":\"missing.ico\"}}",NULL,&error) && !exists(resource));
 plan=xideResourcePlanPrepare(p,race,"{\"version\":{\"fileVersion\":\"1.0.0.0\"},\"extension\":{\"keep\":true}}",NULL,&error); CHECK(plan && !exists(race));
 CHECK(writeText(race,"sentinel") && !xideResourcePlanCommit(p,plan,&error) && error.code==XIDE_MODEL_CONFLICT && !p->dirty && !p->resources.count); xideResourcePlanDestroy(plan); plan=NULL;
 size_t size=0; char* text=(char*)xrtFileReadAll(race,&size); CHECK(text && size==8 && !memcmp(text,"sentinel",8)); xrtFree(text);
 plan=xideResourcePlanPrepare(p,resource,"{\"version\":{\"fileVersion\":\"1.0.0.0\"}}",NULL,&error); CHECK(plan && xideProjectSetMetadata(p,p->name,"stale",p->kind,&error) && !xideResourcePlanCommit(p,plan,&error) && error.code==XIDE_MODEL_CONFLICT && !exists(resource)); xideResourcePlanDestroy(plan); plan=NULL;
 CHECK(xideProjectSetMetadata(p,p->name,"",p->kind,&error)); p->dirty=0;
 plan=xideResourcePlanPrepare(p,resource,"{\"version\":{\"fileVersion\":\"1.0.0.0\"}}",NULL,&error); CHECK(plan); lock=lockPath(manifest,GENERIC_WRITE,0); CHECK(lock!=INVALID_HANDLE_VALUE && !xideResourcePlanCommit(p,plan,&error) && !exists(resource) && !p->dirty); CloseHandle(lock); lock=INVALID_HANDLE_VALUE; xideResourcePlanDestroy(plan); plan=NULL;
 plan=xideResourcePlanPrepare(p,resource,"{\"version\":{\"fileVersion\":\"1.0.0.0\"}}",NULL,&error); CHECK(plan && xrtFileWriteAtomic(manifest,(xbytesview){(const unsigned char*)manifestBytes,manifestLength}) && !xideResourcePlanCommit(p,plan,&error) && error.code==XIDE_MODEL_CONFLICT && !exists(resource)); xideResourcePlanDestroy(plan); plan=NULL;
 plan=xideResourcePlanPrepare(p,guardFile,"{\"version\":{\"fileVersion\":\"1.0.0.0\"}}",NULL,&error); CHECK(plan); lock=lockPath(guard,DELETE,1); CHECK(lock!=INVALID_HANDLE_VALUE && !xideResourcePlanCommit(p,plan,&error) && !exists(guardFile)); CloseHandle(lock); lock=INVALID_HANDLE_VALUE; xideResourcePlanDestroy(plan); plan=NULL;
 plan=xideResourcePlanPrepare(p,guardFile,"{\"version\":{\"fileVersion\":\"1.0.0.0\"}}",NULL,&error); CHECK(plan && move(guard,moved) && xrtDirCreateAll(guard) && !xideResourcePlanCommit(p,plan,&error) && error.code==XIDE_MODEL_CONFLICT && !exists(guardFile)); xideResourcePlanDestroy(plan); plan=NULL;
 char *link=KEEP(xidePathResolve(root,"junction")),*child=KEEP(xidePathResolve(resDir,"inside")),*throughLink=KEEP(xidePathResolveLexical(link,"inside/new.json"));
 CHECK(link && child && throughLink && xrtDirCreateAll(child) && tJunction(link,resDir));
 plan=xideResourcePlanPrepare(p,throughLink,"{\"version\":{\"fileVersion\":\"1.0.0.0\"}}",NULL,&error);
 CHECK(plan && !xideResourcePlanCommit(p,plan,&error) && !exists(throughLink) && !p->dirty); xideResourcePlanDestroy(plan); plan=NULL;
 wchar_t linkName[32768]; CHECK(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,link,-1,linkName,32768) && RemoveDirectoryW(linkName));
 now=xideProjectExport(p,&error); CHECK(now && xrtValueEqual(now,saved) && !p->dirty && !p->resources.count); xrtValueRelease(now); now=NULL;
 CHECK(!xideResourceViewWindow(&app) && !xideResourceViewControl(&app,XIDE_RES_NEW_PATH) && xideUiFontApply(&app,app.uiFontPath,40,&error));
 CHECK(xideActionExecute(&app,XIDE_ACTION_NEW_RESOURCE)==XIDE_ACTION_DISPATCHED && settle(&app));
 CHECK(xideResourceViewIsOpen(&app) && xuiInputGetFont(xideResourceViewControl(&app,XIDE_RES_NEW_PATH))==app.uiFont);
 CHECK(xuiWidgetGetRect(xideResourceViewControl(&app,XIDE_RES_NEW_PATH)).fH==xideUiHeight(&app,32) && click(&app,XIDE_RES_CANCEL) && xideUiFontApply(&app,app.uiFontPath,16,&error));
 CHECK(menuOpen(&app,XIDE_ACTION_NEW_RESOURCE) && xideResourceViewIsOpen(&app) && xuiWidgetGetVisible(xideResourceViewControl(&app,XIDE_RES_NEW_PATH)) && capture(&app,image));
 CHECK(xideActionExecute(&app,XIDE_ACTION_NEW_FILE)==XIDE_ACTION_DISABLED && !xideResourceViewOpenNew(&app) && !xideReferencesViewOpen(&app) && click(&app,XIDE_RES_CANCEL) && app.activeDocument==c && !exists(resource) && !p->dirty);
 xui_widget property=findButton(app.context,app.properties,"新建 Windows 资源配置…");
 CHECK(property && xuiScrollViewEnsureChildVisible(app.properties,property)==XUI_OK && settle(&app) && pointer(&app,xuiWidgetGetWorldRect(property)) && xideResourceViewIsOpen(&app));
 CHECK(xuiEditSetText(xideResourceViewControl(&app,XIDE_RES_NEW_PATH),resource)==XUI_OK && choose(&app,XIDE_RES_CONFIG,1) && !strcmp(xuiEditGetText(xideResourceViewControl(&app,XIDE_RES_NEW_PATH)),typed) && choose(&app,XIDE_RES_CONFIG,0));
 CHECK(xuiEditSetText(xideResourceViewControl(&app,XIDE_RES_FILE_VERSION),"9.8.7.6")==XUI_OK && xuiEditSetText(xideResourceViewControl(&app,XIDE_RES_PRODUCT_VERSION),"9.8.7.6")==XUI_OK && choose(&app,XIDE_RES_ICON_MODE,1) && xuiTextEditSetText(xideResourceViewControl(&app,XIDE_RES_ICON),"app.ico")==XUI_OK && choose(&app,XIDE_RES_MANIFEST_MODE,3) && xuiTextEditSetText(xideResourceViewControl(&app,XIDE_RES_MANIFEST),xml)==XUI_OK);
 CHECK(click(&app,XIDE_RES_VALIDATE) && !exists(resource) && !p->resources.count && strstr(xuiTextEditGetText(xideResourceViewControl(&app,XIDE_RES_PREVIEW)),"9.8.7.6") && xuiScrollViewEnsureChildVisible(xideDialogGetScrollViewWidget(xideResourceViewWindow(&app)),xideResourceViewControl(&app,XIDE_RES_PREVIEW))==XUI_OK && capture(&app,imagePreview));
 previewBytes=_strdup(xuiTextEditGetText(xideResourceViewControl(&app,XIDE_RES_PREVIEW))); CHECK(previewBytes && click(&app,XIDE_RES_APPLY) && !xideResourceViewIsOpen(&app) && p->resources.count==1 && p->dirty && xidePathEqual(p->resources.items[0],resource) && app.activeDocument==c && c->dirty && xl->dirty && !strcmp(app.workspaceModel.startupProject,"beta"));
 text=(char*)xrtFileReadAll(resource,&size); CHECK(text && size==strlen(previewBytes) && !memcmp(text,previewBytes,size) && xideResourceParse(&parsed,text,&error) && get(parsed.root,"format")); xrtFree(text); free(previewBytes); previewBytes=NULL;
 int first=0,last=0; CHECK(xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(c->editor))==cv && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xl->editor))==xv && xuiCodeSelectionGetRange(xuiCodeEditGetSelection(c->editor),&first,&last)==XUI_OK && first==1 && last==8);
 text=(char*)xrtFileReadAll(manifest,&size); CHECK(text && size==manifestLength && !memcmp(text,manifestBytes,size) && xideActionQuery(&app,XIDE_ACTION_RESOURCES).enabled); xrtFree(text);
 CHECK(xideResourceViewOpen(&app) && !xuiWidgetGetVisible(xideResourceViewControl(&app,XIDE_RES_NEW_PATH)) && !strcmp(xuiEditGetText(xideResourceViewControl(&app,XIDE_RES_FILE_VERSION)),"9.8.7.6") && click(&app,XIDE_RES_CANCEL));
 CHECK(xideResourceViewOpenNew(&app) && xuiEditSetText(xideResourceViewControl(&app,XIDE_RES_NEW_PATH),race)==XUI_OK && !xideResourceViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && xideResourceViewIsOpen(&app) && !strcmp(xuiEditGetText(xideResourceViewControl(&app,XIDE_RES_NEW_PATH)),race) && click(&app,XIDE_RES_CANCEL));
 CHECK(xideResourceViewOpenNew(&app) && xuiEditSetText(xideResourceViewControl(&app,XIDE_RES_NEW_PATH),typed)==XUI_OK && xuiCodeDocumentInsert(xuiCodeEditGetDocument(c->editor),0,"// later\n")==XUI_OK && !xideResourceViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && !exists(typed) && click(&app,XIDE_RES_CANCEL) && xuiCodeDocumentUndo(xuiCodeEditGetDocument(c->editor))==XUI_OK);
 CHECK(xideResourceViewOpenNew(&app) && xideProjectSetMetadata(p,p->name,"stale UI",p->kind,&error) && !xideResourceViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && click(&app,XIDE_RES_CANCEL) && xideProjectSetMetadata(p,p->name,"",p->kind,&error));
 CHECK(xideResourceViewOpenNew(&app) && xrtFileWriteAtomic(manifest,(xbytesview){(const unsigned char*)manifestBytes,manifestLength}) && !xideResourceViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && click(&app,XIDE_RES_CANCEL));
 CHECK(xideResourceViewOpenNew(&app) && choose(&app,XIDE_RES_CONFIG,1) && xuiEditSetText(xideResourceViewControl(&app,XIDE_RES_NEW_PATH),typed)==XUI_OK && xuiEditSetText(xideResourceViewControl(&app,XIDE_RES_FILE_VERSION),"9.8.7.6")==XUI_OK && choose(&app,XIDE_RES_ICON_MODE,1) && xuiTextEditSetText(xideResourceViewControl(&app,XIDE_RES_ICON),"app.ico")==XUI_OK && choose(&app,XIDE_RES_MANIFEST_MODE,3) && xuiTextEditSetText(xideResourceViewControl(&app,XIDE_RES_MANIFEST),xml)==XUI_OK);
 CHECK(xideUiFontApply(&app,app.uiFontPath,40,&error) && resize(&app,700,460) && xuiInputGetFont(xideResourceViewControl(&app,XIDE_RES_NEW_PATH))==app.uiFont && xuiScrollViewEnsureChildVisible(xideDialogGetScrollViewWidget(xideResourceViewWindow(&app)),xideResourceViewControl(&app,XIDE_RES_APPLY))==XUI_OK && capture(&app,imageLarge) && click(&app,XIDE_RES_APPLY) && p->resources.count==2 && exists(typed) && xideUiFontApply(&app,app.uiFontPath,16,&error) && resize(&app,1280,800));
 CHECK(xuiCodeDocumentUndo(xuiCodeEditGetDocument(c->editor))==XUI_OK && !c->dirty && xuiCodeDocumentUndo(xuiCodeEditGetDocument(xl->editor))==XUI_OK && !xl->dirty && xideWorkspaceSaveAll(&app,NULL,&error));
 if(f->compiler) {
  /* Compile each newly generated format in a separate real C/xlang project. */
  const char* paths[]={resource,typed};
  for(int i=0;i<2;++i) {
   CHECK(xideProjectCreate(&compiled,outManifest,"Created resource integration",entry,&error) && xideProjectAddFile(&compiled,native,"c",&error) && xideProjectAddFile(&compiled,paths[i],"resource-config",&error) && xideProjectSave(&compiled,NULL,&error));
   xide_task* task=xideTasksBuild(app.tasks,&compiled,compiled.activeConfiguration,f->compiler,1,&error); xide_task_info info={0}; CHECK(task && taskWait(&app,task,&info));
   if(info.state!=XIDE_TASK_SUCCEEDED) fprintf(stderr,"resource creation compile: %s\n%s\n",info.error,xideTaskOutput(task));
   CHECK(info.state==XIDE_TASK_SUCCEEDED && info.built && info.ran && pe(xideTaskArtifact(task),1)); xideProjectClear(&compiled);
  }
  puts("resource_creation_test compiler passed: created JSON and XLON real C/xlang build/run, PE version 9.8.7.6, exact RT_ICON/RT_GROUP_ICON/RT_MANIFEST.");
 }
 puts("resource_creation_test passed: action 39, zero-resource project menu/property button; preview/cancel no writes; exact generated bytes; JSON/XLON mode; new ordinary Unicode path; invalid names/extensions/assets; target competition; stale project/manifest/parent, real ancestor junction and sharing conflicts; no-allocation reference transfer; existing dirty C/xlang undo/selection preserved; independent startup; 125% DPI/40 DIP two-axis scroll; 3 native screenshots.");
cleanup:
 if(lock!=INVALID_HANDLE_VALUE) CloseHandle(lock);
 if(began) { xgeEnd(); } xideAppDestroy(&app); xideProjectClear(&project); xideProjectClear(&compiled); xideResourcePlanDestroy(plan); xideResourceClear(&parsed); xrtValueRelease(saved); xrtValueRelease(now); xrtValueRelease(references); xrtFree(manifestBytes); free(previewBytes);
 for(size_t i=0;i<ownedCount;++i) { free(owned[i]); } xgeQuit(); return XGE_OK;
}
static DWORD WINAPI worker(void* data)
{ fixture* f=data; if(!SetThreadDesktop(f->desktop)) return 1; xge_desc_t d={0}; d.iWidth=1600; d.iHeight=1000; d.sTitle="xlang Windows resource creation verification"; d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; d.iRunMode=XGE_RUN_GAME_LOOP; int result=xgeInit(&d); if(result==XGE_OK) result=xgeRun(verify,f); xgeUnit(); return result==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv)
{
 if(argc<2 || argc>3 || !xrtDirCreateAll(argv[1])) return 2;
 fixture f={.directory=argv[1],.compiler=argc==3 ? argv[2] : NULL}; wchar_t name[100]; swprintf(name,100,L"XlangResourceCreation-%lu",GetCurrentProcessId()); f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) return 1;
 HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1; if(thread) { if(WaitForSingleObject(thread,120000)!=WAIT_OBJECT_0) { fputs("resource_creation_test timed out\n",stderr); ExitProcess(1); } GetExitCodeThread(thread,&code); CloseHandle(thread); } CloseDesktop(f.desktop); return (int)code;
}
