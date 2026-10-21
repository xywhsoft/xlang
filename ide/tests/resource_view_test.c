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
#include "model/resources.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct fixture { const char *directory,*compiler; HDESK desktop; int started,failed; } fixture;
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"resource_view_test:%d: %s; %s; %s\n",__LINE__,#v,error.message,xideResourceViewMessage(&app)); f->failed=1; goto cleanup; } } while(0)
static const char* json="{\"format\":\"xlang-windows-resource-1\",\"language\":1033,\"codepage\":\"1200\",\"version\":{\"fileVersion\":\"2.3.4.5\",\"FileVersion\":\"1.2.3.4\",\"ProductName\":\"资源属性😀\",\"extension\":{\"keep\":true}},\"extension\":{\"keep\":[1,2,3]}}";
static const char* xlon="{\"version\":{\"fileVersion\":\"3.4.5.6\",\"keep\":char(\"😀\")},\"icon\":{\"base64\":\"AQID\",\"keep\":time(\"2000-01-02T03:04:05Z\")},\"manifest\":{\"text\":\"<assembly/>\",\"keep\":float(\"nan\")},\"extension\":{\"integerKeys\":intmap{1:\"保留\"},\"unique\":set[1,2],\"binary\":bytes(\"AQID\")}}";
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
static int menuOpen(xide_app* a)
{
 int i=-1; for(int j=0;j<xuiMenuGetItemCount(a->projectMenu);++j) if(xuiMenuGetItem(a->projectMenu,j)->iValue==XIDE_ACTION_RESOURCES) i=j;
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
static int verify(void* user)
{
 fixture* f=user; if(f->started) return XGE_OK; f->started=1; xide_app app={0}; xide_model_error error={0}; int began=0;
 xide_resource_config config={0},round={0}; xvalue *draft=NULL,*saved=NULL,*refs=NULL; xide_project typed={0};
 char* owned[36]={0}; size_t ownedCount=0; char *body=NULL,*original=NULL,*b64Icon=NULL,*b64Xml=NULL; size_t originalLength=0;
#define KEEP(s) (owned[ownedCount++]=(s))
 char* id=xideCreateId("resource-form"); char* root=KEEP(id ? xidePathResolve(f->directory,id) : NULL); free(id); CHECK(root && xrtDirCreateAll(root));
 char *entry=KEEP(xidePathResolve(root,"main.xl")),*native=KEEP(xidePathResolve(root,"native.c")),*manifest=KEEP(xidePathResolve(root,"app.xlp")),*workspace=KEEP(xidePathResolve(root,"workspace.xlw")),*settings=KEEP(xidePathResolve(root,"settings.json")),*resDir=KEEP(xidePathResolve(root,"资源😀")); CHECK(resDir && xrtDirCreateAll(resDir));
 char *resource=KEEP(xidePathResolve(resDir,"app.res.json")),*typedResource=KEEP(xidePathResolve(resDir,"app.res.xlon")),*icon=KEEP(xidePathResolve(resDir,"app.ico")),*xmlPath=KEEP(xidePathResolve(resDir,"app.manifest")),*typedManifest=KEEP(xidePathResolve(root,"typed.xlp")),*image=KEEP(xidePathResolve(root,"native-resource-version.png")),*imageAssets=KEEP(xidePathResolve(root,"native-resource-assets.png")),*imageLarge=KEEP(xidePathResolve(root,"native-resource-large-font.png"));
 CHECK(entry && native && manifest && workspace && settings && resource && typedResource && icon && xmlPath && typedManifest && image && imageAssets && imageLarge);
 unsigned char ico[70]={0}; ico[2]=1; ico[4]=1; ico[6]=1; ico[7]=1; ico[10]=1; ico[12]=32; ico[14]=48; ico[18]=22; ico[22]=40; ico[26]=1; ico[30]=2; ico[34]=1; ico[36]=32; ico[42]=4; ico[62]=0x44; ico[63]=0x88; ico[64]=0xcc; ico[65]=0xff;
 CHECK(xrtFileWriteAtomic(icon,(xbytesview){ico,sizeof(ico)}) && writeText(xmlPath,xml));
 b64Icon=xrtBase64EncodeNew(ico,sizeof(ico),NULL); b64Xml=xrtBase64EncodeNew(xml,strlen(xml),NULL); CHECK(b64Icon && b64Xml);
 /* Known field checks, aliases, defaults, typed unknown values and exact DOM roundtrip. */
 CHECK(!xideResourceSetVersion(NULL,0,NULL,&error) && !xideResourceSetNumber(NULL,"language","1033",&error) && !xideResourceSetSource(NULL,0,XIDE_RESOURCE_NONE,"",&error));
 CHECK(xideResourceParse(&config,json,&error) && !config.xson && !strcmp(xideResourceVersion(config.root,3),"2.3.4.5"));
 draft=xrtValueDeepClone(config.root); const char* fields[9]; for(int i=0;i<9;++i) fields[i]=xideResourceVersion(config.root,i);
 CHECK(draft && xideResourceSetVersion(draft,1,fields,&error) && xideResourceSetNumber(draft,"codepage","1200",&error) && xrtValueEqual(draft,config.root));
 fields[3]="9.8.7.6"; fields[8]="9.8.7.6"; CHECK(xideResourceSetVersion(draft,1,fields,&error) && xideResourceSetNumber(draft,"codepage","65001",&error) && xideResourceSetSource(draft,0,XIDE_RESOURCE_PATH,"app.ico",&error) && xideResourceSetSource(draft,1,XIDE_RESOURCE_TEXT,xml,&error) && xideResourceCheckAssets(draft,resource,&error));
 body=xideResourceSerialize(&config,draft,&error); CHECK(body && strstr(body,"1.2.3.4") && strstr(body,"extension") && xideResourceParse(&round,body,&error) && xrtValueEqual(round.root,draft)); xrtFree(body); body=NULL;
 CHECK(!xideResourceSetNumber(draft,"language","4294967296",&error) && !xideResourceSetNumber(draft,"codepage","-1",&error));
 const char* invalid[]={"[]","{\"version\":3}","{\"version\":{\"fileVersion\":\"1.2.65536\"}}","{\"version\":{\"fileVersion\":\"4294967296000000000000\"}}","{\"version\":{\"fileVersion\":\"1..2\"}}","{\"version\":{\"productName\":\"nul\\u0000name\"}}","{\"icon\":{\"text\":\"bad\"}}","{\"manifest\":{\"path\":2}}","{\"language\":true}"};
 for(size_t i=0;i<sizeof(invalid)/sizeof(*invalid);++i) CHECK(!xideResourceParse(&round,invalid[i],&error) && xrtValueEqual(round.root,draft));
 CHECK(xideResourceSetSource(draft,0,XIDE_RESOURCE_PATH,"missing.ico",&error) && !xideResourceCheckAssets(draft,resource,&error) && xideResourceSetSource(draft,0,XIDE_RESOURCE_BASE64,"AQID",&error) && !xideResourceCheckAssets(draft,resource,&error));
 CHECK(xideResourceSetSource(draft,0,XIDE_RESOURCE_NONE,"",&error) && xideResourceSetSource(draft,1,XIDE_RESOURCE_BASE64,"%%invalid%%",&error) && !xideResourceCheckAssets(draft,resource,&error));
 xrtValueRelease(draft); draft=NULL;
 CHECK(xideResourceParse(&round,"{\"version\":{},\"icon\":{\"path\":\"app.ico\",\"base64\":\"hidden\",\"extension\":{\"keep\":true}}}",&error)); draft=xrtValueDeepClone(round.root);
 CHECK(draft && xideResourceSetSource(draft,0,XIDE_RESOURCE_PATH,"app.ico",&error) && xrtValueEqual(draft,round.root) && xideResourceSetSource(draft,0,XIDE_RESOURCE_BASE64,b64Icon,&error) && get(get(draft,"icon"),"extension") && !get(get(draft,"icon"),"path") && xideResourceSetSource(draft,1,XIDE_RESOURCE_BASE64,b64Xml,&error) && xideResourceCheckAssets(draft,resource,&error));
 xrtValueRelease(draft); draft=NULL; CHECK(xideResourceParse(&round,"{\"version\":null,\"icon\":null}",&error)); draft=xrtValueDeepClone(round.root); CHECK(draft && xideResourceSetVersion(draft,0,fields,&error) && xrtValueEqual(draft,round.root) && !xideResourceCheckAssets(draft,resource,&error)); xrtValueRelease(draft); draft=NULL;
 CHECK(xideResourceParse(&round,xlon,&error) && round.xson); draft=xrtValueDeepClone(round.root); CHECK(draft && xideResourceSetVersion(draft,1,fields,&error)); body=xideResourceSerialize(&round,draft,&error); CHECK(body && strstr(body,"intmap{") && strstr(body,"set[") && strstr(body,"bytes(") && strstr(body,"char(\"😀\")") && strstr(body,"time(\"2000-01-02T03:04:05Z\")") && strstr(body,"float(\"nan\")")); xrtFree(body); body=NULL; xrtValueRelease(draft); draft=NULL;
 body=malloc(strlen(json)+2); CHECK(body); sprintf(body,"%s\n",json); xide_text_file encoded={0}; CHECK(xideTextFileEncode(body,(int)strlen(body),1,XUI_CODE_EOL_CRLF,&encoded,&error) && xrtFileWriteAtomic(resource,(xbytesview){encoded.raw,encoded.rawLength})); xideTextFileClear(&encoded); free(body); body=NULL;
 CHECK(writeText(typedResource,xlon) && writeText(entry,"extern \"native_answer\" func nativeAnswer() as c_int\nfunc main(...) as int\n    return nativeAnswer()-42\nend\n") && writeText(native,"int native_answer(void) { return 42; }\n")); original=(char*)xrtFileReadAll(resource,&originalLength); CHECK(original);
 app.hidden=1; app.maxFrames=1; app.noSession=1; app.highDpi=1; app.dpiOverride=1.25f; app.settingsPath=settings; CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1;
 CHECK(xideActionCount()==71 && xideActionNamed("project.resources")->id==38 && !xideActionQuery(&app,XIDE_ACTION_RESOURCES).enabled && xideWorkspaceCreate(&app.workspaceModel,"资源表单验收😀",&error));
 CHECK(xideProjectCreate(&typed,manifest,"C 与 xlang 资源验收",entry,&error) && xideProjectAddFile(&typed,native,"c",&error) && xideProjectAddFile(&typed,resource,"resource-config",&error) && xideProjectAddFile(&typed,typedResource,"resource-config",&error) && xideProjectSave(&typed,NULL,&error) && xideWorkspaceAddProject(&app.workspaceModel,manifest,"alpha",&error) && xideWorkspaceSetStartup(&app.workspaceModel,"alpha",&error) && xideWorkspaceSaveAll(&app,workspace,&error) && xideWorkspaceViewRefresh(&app)==XUI_OK && xideWorkspaceViewSelectProject(&app,"alpha")); xideProjectClear(&typed);
 xide_project* p=xideWorkspaceFindProject(&app.workspaceModel,"alpha"); CHECK(p && xideEditorOpen(&app,entry,&error)); xide_document* xlDoc=app.activeDocument; CHECK(xideEditorOpen(&app,native,&error)); xide_document* cDoc=app.activeDocument;
 CHECK(token(xlDoc->editor,"extern",XUI_CODE_TOKEN_KEYWORD)); CHECK(token(cDoc->editor,"native_answer",XUI_CODE_TOKEN_IDENTIFIER)); CHECK(xideActionQuery(&app,XIDE_ACTION_RESOURCES).enabled); CHECK(menuOpen(&app)); CHECK(xideResourceViewIsOpen(&app) && capture(&app,image));
 CHECK(xideActionExecute(&app,XIDE_ACTION_NEW_FILE)==XIDE_ACTION_DISABLED && !xideReferencesViewOpen(&app) && !xideConfigurationViewOpen(&app) && !xideImportViewOpen(&app) && !xideTemplateViewOpen(&app) && !xideFontViewOpen(&app) && !xideRecoveryViewOpen(&app) && !xideFileChangesViewOpen(&app));
 CHECK(click(&app,XIDE_RES_APPLY) && !xideResourceViewIsOpen(&app) && !xideDocumentsFind(&app.documents,resource) && !p->dirty);
 xui_widget propertyButton=findButton(app.context,app.properties,"Windows 资源属性…");
 CHECK(propertyButton && xuiScrollViewEnsureChildVisible(app.properties,propertyButton)==XUI_OK && settle(&app) && pointer(&app,xuiWidgetGetWorldRect(propertyButton)) && xideResourceViewIsOpen(&app) && click(&app,XIDE_RES_CANCEL));
 CHECK(xideResourceViewOpen(&app) && xuiEditSetText(xideResourceViewControl(&app,XIDE_RES_FILE_VERSION),"9.8.7.6")==XUI_OK && !choose(&app,XIDE_RES_CONFIG,1) && xuiComboBoxGetSelected(xideResourceViewControl(&app,XIDE_RES_CONFIG))==0 && !strcmp(xuiEditGetText(xideResourceViewControl(&app,XIDE_RES_FILE_VERSION)),"9.8.7.6") && click(&app,XIDE_RES_CANCEL));
 CHECK(xideResourceViewOpen(&app) && choose(&app,XIDE_RES_ICON_MODE,1)); xui_widget emptyInput=xideResourceViewControl(&app,XIDE_RES_ICON);
 CHECK(xuiScrollViewEnsureChildVisible(xideDialogGetScrollViewWidget(xideResourceViewWindow(&app)),emptyInput)==XUI_OK && settle(&app) && pointer(&app,xuiWidgetGetWorldRect(emptyInput)) && xuiGetFocusWidget(app.context)==emptyInput && xuiInputText(app.context,'x')==XUI_OK && settle(&app) && !strcmp(xuiTextEditGetText(emptyInput),"x") && xuiWidgetGetCachePolicy(emptyInput).iPolicy==XUI_CACHE_POLICY_AUTO && click(&app,XIDE_RES_CANCEL));
 CHECK(!xideDocumentsFind(&app.documents,resource) && xideResourceViewOpen(&app) && xuiEditSetText(xideResourceViewControl(&app,XIDE_RES_FILE_VERSION),"1.2.70000")==XUI_OK && !xideResourceViewApply(&app,&error) && !xideDocumentsFind(&app.documents,resource) && click(&app,XIDE_RES_CANCEL));
 CHECK(xideResourceViewOpen(&app) && xuiEditSetText(xideResourceViewControl(&app,XIDE_RES_FILE_VERSION),"9.8.7.6")==XUI_OK && xuiEditSetText(xideResourceViewControl(&app,XIDE_RES_PRODUCT_VERSION),"9.8.7.6")==XUI_OK && choose(&app,XIDE_RES_ICON_MODE,1) && xuiTextEditSetText(xideResourceViewControl(&app,XIDE_RES_ICON),"app.ico")==XUI_OK && choose(&app,XIDE_RES_MANIFEST_MODE,3) && xuiTextEditSetText(xideResourceViewControl(&app,XIDE_RES_MANIFEST),xml)==XUI_OK && click(&app,XIDE_RES_VALIDATE) && xuiTextEditIsReadonly(xideResourceViewControl(&app,XIDE_RES_PREVIEW)) && strstr(xuiTextEditGetText(xideResourceViewControl(&app,XIDE_RES_PREVIEW)),"9.8.7.6") && capture(&app,imageAssets));
 CHECK(click(&app,XIDE_RES_APPLY) && !xideResourceViewIsOpen(&app)); xide_document* d=app.activeDocument; CHECK(d && xidePathEqual(d->path,resource) && d->dirty && d->baseline.bom && d->baseline.eol==XUI_CODE_EOL_CRLF && !p->dirty);
 size_t n=0; char* raw=(char*)xrtFileReadAll(resource,&n); CHECK(raw && n==originalLength && !memcmp(raw,original,n)); xrtFree(raw);
 CHECK(xuiCodeDocumentUndo(xuiCodeEditGetDocument(d->editor))==XUI_OK && !d->dirty && !strcmp(xuiCodeEditGetText(d->editor),d->baseline.text) && xuiCodeDocumentRedo(xuiCodeEditGetDocument(d->editor))==XUI_OK && d->dirty && xideDocumentSave(d,NULL,0,&error));
 raw=(char*)xrtFileReadAll(resource,&n); CHECK(raw && n>3 && !memcmp(raw,"\xef\xbb\xbf",3) && strstr(raw,"\r\n") && strstr(raw,"1.2.3.4")); xrtFree(raw);
 /* Current unsaved body is the form source and remains the preceding undo step. */
 CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(d->editor),0," \n")==XUI_OK && d->dirty); uint32_t version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor));
 CHECK(xideResourceViewOpen(&app) && strstr(xideResourceViewMessage(&app),"未保存") && xuiEditSetText(xideResourceViewControl(&app,XIDE_RES_PRODUCT),"未保存产品😀")==XUI_OK && click(&app,XIDE_RES_APPLY) && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor))!=version && xuiCodeDocumentUndo(xuiCodeEditGetDocument(d->editor))==XUI_OK && d->dirty && !strncmp(xuiCodeEditGetText(d->editor)," \n",2) && xuiCodeDocumentUndo(xuiCodeEditGetDocument(d->editor))==XUI_OK && !d->dirty);
 CHECK(xideResourceViewOpen(&app) && xuiCodeDocumentInsert(xuiCodeEditGetDocument(cDoc->editor),0,"// changed\n")==XUI_OK && !xideResourceViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && click(&app,XIDE_RES_CANCEL) && xuiCodeDocumentUndo(xuiCodeEditGetDocument(cDoc->editor))==XUI_OK);
 CHECK(xideResourceViewOpen(&app) && xideProjectSetMetadata(p,p->name,"模型变化",p->kind,&error) && !xideResourceViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && click(&app,XIDE_RES_CANCEL) && xideWorkspaceSaveAll(&app,NULL,&error));
 CHECK(xideResourceViewOpen(&app)); raw=(char*)xrtFileReadAll(resource,&n); CHECK(raw && xrtFileWriteAtomic(resource,(xbytesview){(const unsigned char*)raw,n}) && !xideResourceViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && click(&app,XIDE_RES_CANCEL)); xrtFree(raw);
 CHECK(xideResourceViewOpen(&app) && writeText(resource,json) && !xideResourceViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && click(&app,XIDE_RES_CANCEL) && xideDocumentReload(d,0,&error));
 CHECK(xideResourceViewOpen(&app) && xuiEditSetText(xideResourceViewControl(&app,XIDE_RES_FILE_VERSION),"9.8.7.6")==XUI_OK && xuiEditSetText(xideResourceViewControl(&app,XIDE_RES_PRODUCT_VERSION),"9.8.7.6")==XUI_OK && choose(&app,XIDE_RES_ICON_MODE,1) && xuiTextEditSetText(xideResourceViewControl(&app,XIDE_RES_ICON),"app.ico")==XUI_OK && choose(&app,XIDE_RES_MANIFEST_MODE,1) && xuiTextEditSetText(xideResourceViewControl(&app,XIDE_RES_MANIFEST),"app.manifest")==XUI_OK && click(&app,XIDE_RES_APPLY) && xideDocumentSave(d,NULL,0,&error));
 CHECK(xideResourceViewOpen(&app) && choose(&app,XIDE_RES_CONFIG,1) && xuiEditSetText(xideResourceViewControl(&app,XIDE_RES_FILE_VERSION),"9.8.7.6")==XUI_OK && xuiEditSetText(xideResourceViewControl(&app,XIDE_RES_PRODUCT_VERSION),"9.8.7.6")==XUI_OK && choose(&app,XIDE_RES_ICON_MODE,2) && xuiTextEditSetText(xideResourceViewControl(&app,XIDE_RES_ICON),b64Icon)==XUI_OK && choose(&app,XIDE_RES_MANIFEST_MODE,2) && xuiTextEditSetText(xideResourceViewControl(&app,XIDE_RES_MANIFEST),b64Xml)==XUI_OK && click(&app,XIDE_RES_VALIDATE) && strstr(xuiTextEditGetText(xideResourceViewControl(&app,XIDE_RES_PREVIEW)),"intmap{"));
 char* modernImage=KEEP(xidePathResolve(root,"native-resource-modern-xlon.png")); CHECK(modernImage && xuiScrollViewEnsureChildVisible(xideDialogGetScrollViewWidget(xideResourceViewWindow(&app)),xideResourceViewControl(&app,XIDE_RES_PREVIEW))==XUI_OK && capture(&app,modernImage) && click(&app,XIDE_RES_APPLY));
 xide_document* td=app.activeDocument; CHECK(td && xidePathEqual(td->path,typedResource) && td->dirty && strstr(xuiCodeEditGetText(td->editor),"char(\"😀\")") && strstr(xuiCodeEditGetText(td->editor),"time(\"2000-01-02T03:04:05Z\")") && strstr(xuiCodeEditGetText(td->editor),"float(\"nan\")") && xuiCodeDocumentUndo(xuiCodeEditGetDocument(td->editor))==XUI_OK && !td->dirty && !strcmp(xuiCodeEditGetText(td->editor),xlon) && xuiCodeDocumentRedo(xuiCodeEditGetDocument(td->editor))==XUI_OK && td->dirty && xideDocumentSave(td,NULL,0,&error));
 CHECK(xideResourceViewOpen(&app) && xideUiFontApply(&app,app.uiFontPath,40,&error) && resize(&app,700,460) && xuiTextEditGetFont(xideResourceViewControl(&app,XIDE_RES_MANIFEST))==app.uiFont && xuiInputGetFont(xideResourceViewControl(&app,XIDE_RES_FILE_VERSION))==app.uiFont && xuiScrollViewEnsureChildVisible(xideDialogGetScrollViewWidget(xideResourceViewWindow(&app)),xideResourceViewControl(&app,XIDE_RES_APPLY))==XUI_OK && capture(&app,imageLarge) && click(&app,XIDE_RES_CANCEL) && xideUiFontApply(&app,app.uiFontPath,16,&error) && resize(&app,1280,800));
 if(f->compiler) {
  refs=xideProjectReferencesJson(p); CHECK(refs); xvalue* files=get(refs,"files"); int removed=0;
  for(size_t i=0;i<xrtValueCount(files);++i) { xvalue* item=xrtValueArrayGet(files,i); xstrview s={0}; if(xrtValueGetString(get(item,"path"),&s) && strstr(s.Data,".xlon")) { CHECK(xrtValueArrayRemove(files,i,1)); removed=1; break; } }
  CHECK(removed && xideProjectApplyReferences(p,refs,&error) && xideWorkspaceSaveAll(&app,NULL,&error));
  xide_task* task=xideTasksBuild(app.tasks,p,p->activeConfiguration,f->compiler,1,&error); xide_task_info info={0}; CHECK(task && taskWait(&app,task,&info)); if(info.state!=XIDE_TASK_SUCCEEDED) fprintf(stderr,"resource compiler: %s\n%s\n",info.error,xideTaskOutput(task)); CHECK(info.state==XIDE_TASK_SUCCEEDED && info.built && info.ran && pe(xideTaskArtifact(task),1));
  CHECK(xideProjectCreate(&typed,typedManifest,"XLON typed resource integration",entry,&error) && xideProjectAddFile(&typed,native,"c",&error) && xideProjectAddFile(&typed,typedResource,"resource-config",&error) && xideProjectSave(&typed,NULL,&error));
  task=xideTasksBuild(app.tasks,&typed,typed.activeConfiguration,f->compiler,1,&error); CHECK(task && taskWait(&app,task,&info)); if(info.state!=XIDE_TASK_SUCCEEDED) fprintf(stderr,"XLON compiler: %s\n%s\n",info.error,xideTaskOutput(task)); CHECK(info.state==XIDE_TASK_SUCCEEDED && info.built && info.ran && pe(xideTaskArtifact(task),1));
  puts("resource_view_test compiler passed: real C/xlang JSON build/run, PE version 9.8.7.6, RT_ICON/RT_GROUP_ICON/RT_MANIFEST bytes, modern XLON char/time/float/intmap/set/bytes in root/version/assets preserved, single undo/redo and compiled.");
 }
 xui_code_document resourceCore=xuiCodeEditGetDocument(d->editor); CHECK(xuiCodeDocumentReplace(resourceCore,0,xuiCodeDocumentGetLength(resourceCore),"{")==XUI_OK); uint32_t badVersion=xuiCodeDocumentGetVersion(resourceCore);
 CHECK(!xideResourceViewOpen(&app) && !xideResourceViewIsOpen(&app) && strstr(xideResourceViewMessage(&app),"无法按") && d->dirty && xuiCodeDocumentGetVersion(resourceCore)==badVersion && !strcmp(xuiCodeEditGetText(d->editor),"{") && xuiCodeDocumentUndo(resourceCore)==XUI_OK && !d->dirty);
 CHECK(xlDoc->open && cDoc->open && token(xlDoc->editor,"extern",XUI_CODE_TOKEN_KEYWORD) && token(cDoc->editor,"native_answer",XUI_CODE_TOKEN_IDENTIFIER));
 puts("resource_view_test passed: aliases/defaults/uint32/NUL/version and asset guards; unknown JSON/modern XLON text preservation; actual menu/property button/buttons/preview and unreadable-form diagnostic preservation; no-op/cancel/pending switch; single undo/redo; BOM/CRLF; unsaved source; model/doc/disk conflicts; modal/font/125% DPI/two-axis scroll; 4 native screenshots.");
cleanup:
 if(began) { xgeEnd(); } xideAppDestroy(&app); xideResourceClear(&config); xideResourceClear(&round); xrtValueRelease(draft); xrtValueRelease(saved); xrtValueRelease(refs); xideProjectClear(&typed); xrtFree(body); xrtFree(original); xrtFree(b64Icon); xrtFree(b64Xml); for(size_t i=0;i<ownedCount;++i) free(owned[i]); xgeQuit(); return XGE_OK;
}
static DWORD WINAPI worker(void* data)
{ fixture* f=data; if(!SetThreadDesktop(f->desktop)) return 1; xge_desc_t d={0}; d.iWidth=1600; d.iHeight=1000; d.sTitle="xlang Windows resource properties verification"; d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; d.iRunMode=XGE_RUN_GAME_LOOP; int result=xgeInit(&d); if(result==XGE_OK) result=xgeRun(verify,f); xgeUnit(); return result==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv)
{
 if(argc<2 || argc>3 || !xrtDirCreateAll(argv[1])) return 2;
 fixture f={.directory=argv[1],.compiler=argc==3 ? argv[2] : NULL}; wchar_t name[100]; swprintf(name,100,L"XlangResourceVerification-%lu",GetCurrentProcessId()); f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) return 1;
 HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1; if(thread) { if(WaitForSingleObject(thread,120000)!=WAIT_OBJECT_0) { fputs("resource_view_test timed out\n",stderr); ExitProcess(1); } GetExitCodeThread(thread,&code); CloseHandle(thread); } CloseDesktop(f.desktop); return (int)code;
}
