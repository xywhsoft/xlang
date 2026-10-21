#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/references_view.h"
#include "ui/workspace_view.h"
#include "ui/configuration_view.h"
#include "ui/font_view.h"
#include "ui/editor_view.h"
#include "ui/task_view.h"
#include "ui/catalog_view.h"
#include "services/library_info.h"
#include "services/ui_font.h"
#include "services/file_sync.h"
#include "platform/path.h"
#include <windows.h>
#include <winver.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct fixture { const char *directory,*compiler; HDESK desktop; int started,failed; } fixture;
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"references_test:%d: %s; %s\n",__LINE__,#v,error.message); f->failed=1; goto cleanup; } } while(0)
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* v,const char* key) { return xrtValueObjectGet(v,sv(key)); }
static int put(xvalue* v,const char* key,const char* s) { return xrtValueObjectSetNew(v,sv(key),xrtValueString(sv(s))); }
static const char* text(const xvalue* v) { xstrview s={0}; return v && xrtValueGetString(v,&s) ? s.Data : ""; }
static int writeText(const char* p,const char* s) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static int addFile(xvalue* refs,const char* id,const char* path,const char* kind)
{
    xvalue* item=xrtValueObject(); int okay=item && put(item,"id",id) && put(item,"path",path) && put(item,"kind",kind) && put(item,"future","保留😀");
    if(okay) return xrtValueArrayAppendNew(get(refs,"files"),item);
    xrtValueRelease(item); return 0;
}
static int addLibrary(xvalue* refs,const char* id,const char* name,const char* kind)
{
    xvalue* item=xrtValueObject(); int okay=item && (!id || put(item,"id",id)) && put(item,"name",name) && put(item,"kind",kind) && put(item,"future","保留库扩展");
    if(okay) return xrtValueArrayAppendNew(get(refs,"libraries"),item);
    xrtValueRelease(item); return 0;
}
static int settle(xide_app* a)
{ if(xuiDispatchPendingEvents(a->context)!=XUI_OK) return 0; xideEditorPoll(a); xideFileSyncPoll(a); xideTaskViewPoll(a); xideWorkbenchLayout(a); return xuiLayout(a->context)==XUI_OK && !a->frameError; }
static int pointer(xide_app* a,xui_rect_t r)
{ float x=r.fX+r.fW/2,y=r.fY+r.fH/2; return xuiInputPointerMove(a->context,x,y,0)==XUI_OK && xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(a); }
static int click(xide_app* a,int id)
{
    xui_widget w=xideReferencesViewControl(a,id),scroll=xideDialogGetScrollViewWidget(xideReferencesViewWindow(a));
    return xuiScrollViewEnsureChildVisible(scroll,w)==XUI_OK && settle(a) && pointer(a,xuiWidgetGetWorldRect(w));
}
static int choose(xide_app* a,int id,int index)
{
    xui_widget combo=xideReferencesViewControl(a,id); if(xuiScrollViewEnsureChildVisible(xideDialogGetScrollViewWidget(xideReferencesViewWindow(a)),combo)!=XUI_OK || !settle(a) || xuiComboBoxOpen(combo)!=XUI_OK || !settle(a)) return 0;
    xui_widget menu=xuiComboBoxGetMenuWidget(combo); xui_rect_t r=xuiMenuGetItemRect(menu,index),world=xuiWidgetGetWorldRect(menu); r.fX+=world.fX; r.fY+=world.fY;
    return pointer(a,r) && xuiComboBoxGetSelected(combo)==index;
}
static int selectRow(xide_app* a,int index)
{
    xui_widget list=xideReferencesViewControl(a,XIDE_REF_LIST); if(xuiListViewEnsureVisible(list,index)!=XUI_OK || !settle(a)) return 0;
    xui_rect_t item=xuiListViewGetItemRect(list,index),local=xuiWidgetGetRect(list);
    item.fX+=local.fX; item.fY+=local.fY;
    if(xuiScrollViewEnsureRectVisible(xideDialogGetScrollViewWidget(xideReferencesViewWindow(a)),item)!=XUI_OK || !settle(a)) return 0;
    xui_rect_t r=xuiListViewGetItemRect(list,index),world=xuiWidgetGetWorldRect(list); r.fX+=world.fX; r.fY+=world.fY;
    xui_rect_t visible=xuiWidgetGetWorldRect(xideDialogGetScrollViewWidget(xideReferencesViewWindow(a)));
    float right=r.fX+r.fW,bottom=r.fY+r.fH;
    if(r.fX<visible.fX) r.fX=visible.fX;
    if(r.fY<visible.fY) r.fY=visible.fY;
    if(right>visible.fX+visible.fW) right=visible.fX+visible.fW;
    if(bottom>visible.fY+visible.fH) bottom=visible.fY+visible.fH;
    r.fW=right-r.fX; r.fH=bottom-r.fY;
    return r.fW>0 && r.fH>0 && pointer(a,r) && xuiListViewGetSelected(list)==index;
}
static int capture(xide_app* a,const char* path)
{
    if(!settle(a) || xuiUpdate(a->context,0)!=XUI_OK) return 0;
    xui_rect_i_t rect={0,0,a->viewportWidth,a->viewportHeight}; return a->proxy.surfaceClear(&a->proxy,a->target,0xffeeeeeeu)==XUI_OK && xuiRender(a->context,a->target,&rect,1)==XUI_OK && xideAppCapture(a,path)==XGE_OK;
}
static int resizeTarget(xide_app* a,int width,int height)
{
    xui_surface_desc_t d={0}; d.iKind=XUI_SURFACE_KIND_TEXTURE; d.iWidth=width; d.iHeight=height; d.iFormat=XUI_SURFACE_FORMAT_RGBA8; d.iFlags=XUI_SURFACE_USAGE_TARGET|XUI_SURFACE_ALPHA_PREMULTIPLIED;
    xui_surface target=NULL; if(a->proxy.surfaceCreate(&a->proxy,&target,&d)!=XUI_OK) return 0;
    a->proxy.surfaceDestroy(&a->proxy,a->target); a->target=target; a->viewportWidth=width; a->viewportHeight=height;
    return xuiInputViewport(a->context,(float)width,(float)height)==XUI_OK && settle(a);
}
static xui_widget findButton(xui_context context,xui_widget w,const char* name)
{
    if(xuiWidgetGetType(w)==xuiWidgetFindType(context,"button") && !strcmp(xuiButtonGetText(w),name)) return w;
    for(xui_widget child=xuiWidgetGetFirstChild(w);child;child=xuiWidgetGetNextSibling(child)) { xui_widget found=findButton(context,child,name); if(found) return found; }
    return NULL;
}
static int versionMatches(const char* path)
{
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,NULL,0); wchar_t* wide=n ? malloc((size_t)n*sizeof(*wide)) : NULL; if(!wide) return 0;
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,wide,n); DWORD ignored=0,size=GetFileVersionInfoSizeW(wide,&ignored); void* data=size ? malloc(size) : NULL; int okay=0;
    if(data && GetFileVersionInfoW(wide,0,size,data)) { VS_FIXEDFILEINFO* info=NULL; UINT length=0; okay=VerQueryValueW(data,L"\\",(void**)&info,&length) && length>=sizeof(*info) && info->dwFileVersionMS==MAKELONG(3,2) && info->dwFileVersionLS==MAKELONG(5,4); }
    free(data); free(wide); return okay;
}
static int libraryFixtures(const char* directory,xide_model_error* error)
{
    xide_project project={0}; xide_library_info info={0}; int okay=0;
    char *base=xidePathResolve(directory,"包候选😀"),*entry=base ? xidePathResolve(base,"main.xl") : NULL,*manifest=base ? xidePathResolve(base,"project.xlp") : NULL;
    char *package=base ? xidePathResolve(base,"local") : NULL,*file=package ? xidePathResolve(package,"xlang.package.json") : NULL,*source=package ? xidePathResolve(package,"helper.xl") : NULL;
    char *env=base ? xidePathResolve(base,"env/local") : NULL,*envFile=env ? xidePathResolve(env,"xlang.package.json") : NULL;
    char *compiler=base ? xidePathResolve(base,"tool/bin/xl.exe") : NULL,*tool=base ? xidePathResolve(base,"tool/extlibs/local") : NULL,*toolFile=tool ? xidePathResolve(tool,"xlang.package.json") : NULL;
    if(!base || !entry || !manifest || !package || !file || !source || !env || !envFile || !compiler || !tool || !toolFile || !xrtDirCreateAll(package) || !xrtDirCreateAll(env) || !xrtDirCreateAll(tool)) goto done;
    project.entry=entry; project.manifest=manifest;
    if(!writeText(envFile,"{\"format\":\"xlang-package-1\",\"version\":\"2.0.0\"}") || !writeText(toolFile,"{\"format\":\"xlang-package-1\",\"version\":\"3.0.0\"}")) goto done;
    if(!xideLibraryInspect(&info,&project,"package","local",compiler,"env",error) || info.state!=XIDE_LIBRARY_FOUND || strcmp(info.version,"2.0.0") || !xidePathEqual(info.manifest,envFile)) goto done;
    if(!xideLibraryInspect(&info,&project,"package","local",compiler,";",error) || strcmp(info.version,"3.0.0") || !xidePathEqual(info.manifest,toolFile)) goto done;
    if(!writeText(file,"\xef\xbb\xbf{\"format\":\"xlang-package-1\",\"version\":\"4.5.6\",\"abiVersion\":\"2\",\"artifact\":{\"xlSources\":[\"helper.xl\"]},\"future\":42}") || !xideLibraryInspect(&info,&project,"package","local",compiler,"env",error) || info.state!=XIDE_LIBRARY_INCOMPLETE || strcmp(info.version,"4.5.6") || strcmp(info.abiVersion,"2") || info.declaredFiles!=1 || info.missingFiles!=1 || !xidePathEqual(info.manifest,file)) goto done;
    if(!writeText(source,"func helper() as int\n    return 1\nend\n") || !xideLibraryInspect(&info,&project,"package","local",compiler,"env",error) || info.state!=XIDE_LIBRARY_FOUND || info.missingFiles) goto done;
    if(!xideLibraryInspect(&info,&project,"package","./local",NULL,NULL,error) || !xidePathEqual(info.manifest,file)) goto done;
    if(!xideLibraryInspect(&info,&project,"package",file,NULL,NULL,error) || !xidePathEqual(info.manifest,file)) goto done;
    if(!writeText(file,"{\"format\":\"xlang-package-1\",\"version\":\"bad\\u0000value\"}") || !xideLibraryInspect(&info,&project,"package","local",compiler,"env",error) || info.state!=XIDE_LIBRARY_INVALID || info.version) goto done;
    if(!writeText(file,"{\"format\":\"xlang-package-1\",\"artifact\":{\"cSources\":[42]}}") || !xideLibraryInspect(&info,&project,"package","local",compiler,NULL,error) || info.state!=XIDE_LIBRARY_INVALID) goto done;
    if(!xideLibraryInspect(&info,&project,"future-kind","local",compiler,NULL,error) || info.state!=XIDE_LIBRARY_UNSUPPORTED || info.manifest) goto done;
    if(!xideLibraryInspect(&info,&project,"package","missing",compiler,NULL,error) || info.state!=XIDE_LIBRARY_NOT_FOUND || info.manifest) goto done;
    okay=1;
done:
    if(!okay) fprintf(stderr,"library candidate fixtures failed: %s\n",error->message);
    xideLibraryInfoClear(&info); free(base); free(entry); free(manifest); free(package); free(file); free(source); free(env); free(envFile); free(compiler); free(tool); free(toolFile); return okay;
}
static int verify(void* user)
{
    fixture* f=user; if(f->started) return XGE_OK; f->started=1;
    xide_app app={0}; xide_project model={0},loaded={0}; xide_model_error error={0}; xvalue *refs=NULL,*before=NULL,*bad=NULL,*after=NULL; int began=0;
    char *root=NULL,*entry=NULL,*native=NULL,*resource=NULL,*data=NULL,*manifest=NULL,*workspace=NULL,*settings=NULL,*copyDir=NULL,*copyPath=NULL,*alias=NULL,*filesImage=NULL,*libsImage=NULL,*largeImage=NULL,*treeKey=NULL;
    char* id=xideCreateId("references"); root=id ? xidePathResolve(f->directory,id) : NULL; free(id); CHECK(root && xrtDirCreateAll(root));
    CHECK(libraryFixtures(root,&error));
    entry=xidePathResolve(root,"main.xl"); native=xidePathResolve(root,"helper.c"); resource=xidePathResolve(root,"app.res.json"); data=xidePathResolve(root,"data.json"); manifest=xidePathResolve(root,"app.xlp"); workspace=xidePathResolve(root,"workspace.xlw"); settings=xidePathResolve(root,"settings.json"); copyDir=xidePathResolve(root,"另存为"); copyPath=copyDir ? xidePathResolve(copyDir,"copy.xlp") : NULL; alias=xidePathResolve(root,"hardlink.c");
    filesImage=xidePathResolve(root,"native-references-files.png"); libsImage=xidePathResolve(root,"native-references-libraries.png"); largeImage=xidePathResolve(root,"native-references-large-font.png");
    CHECK(entry && native && resource && data && manifest && workspace && settings && copyDir && copyPath && alias && filesImage && libsImage && largeImage && xrtDirCreateAll(copyDir));
    CHECK(writeText(entry,"func main(...) as int\n    print(\"RESOURCE_OK\")\n    return 0\nend\n") && writeText(native,"int native_answer(void) { return 42; }\n") && writeText(data,"ordinary resource; not compiler JSON\n"));
    CHECK(writeText(resource,"{\"format\":\"xlang-windows-resource-1\",\"version\":{\"fileVersion\":\"2.3.4.5\",\"productVersion\":\"2.3.4.5\",\"fileDescription\":\"IDE resource verification\",\"productName\":\"xlang IDE\"}}"));
    CHECK(xideProjectCreate(&model,manifest,"引用验收😀",entry,&error) && put(model.source,"unknownRoot","保留根扩展"));
    refs=xideProjectReferencesJson(&model); CHECK(refs && addFile(refs,"native-stable","helper.c","c") && addFile(refs,"resource-stable","app.res.json","resource-config") && addFile(refs,"data-stable","data.json","resource") && addLibrary(refs,NULL,"legacy-package","package") && addLibrary(refs,"future-library","未来库","future-library-kind"));
    for(int i=0;i<320;++i) { char idText[40],path[80]; snprintf(idText,sizeof(idText),"missing-%d",i); snprintf(path,sizeof(path),"缺失 文件/%d.txt",i); CHECK(addFile(refs,idText,path,"text")); }
    CHECK(xideProjectApplyReferences(&model,refs,&error) && model.files.count==324 && model.cSources.count==1 && model.resources.count==1 && xrtValueCount(model.libraries)==2 && xideProjectSave(&model,NULL,&error));
    before=xideProjectReferencesJson(&model); CHECK(before && xideProjectApplyReferences(&model,before,&error) && !model.dirty);
    bad=xrtValueDeepClone(before); CHECK(bad && addFile(bad,"duplicate","helper.c","c") && !xideProjectApplyReferences(&model,bad,&error) && error.code==XIDE_MODEL_CONFLICT && !model.dirty); xrtValueRelease(bad); bad=NULL;
    bad=xrtValueDeepClone(before); CHECK(bad && xrtValueArrayRemove(get(bad,"files"),0,1) && !xideProjectApplyReferences(&model,bad,&error)); xrtValueRelease(bad); bad=NULL;
    bad=xrtValueDeepClone(before); CHECK(bad && put(xrtValueArrayGet(get(bad,"files"),2),"path","icon.png") && !xideProjectApplyReferences(&model,bad,&error)); xrtValueRelease(bad); bad=NULL;
    bad=xrtValueDeepClone(before); CHECK(bad && addLibrary(bad,"duplicate-library","legacy-package","package") && !xideProjectApplyReferences(&model,bad,&error) && error.code==XIDE_MODEL_CONFLICT); xrtValueRelease(bad); bad=NULL;
    bad=xrtValueDeepClone(before); CHECK(bad && xrtValueObjectSetNew(xrtValueArrayGet(get(bad,"libraries"),0),sv("name"),xrtValueString((xstrview){"bad\0name",8})) && !xideProjectApplyReferences(&model,bad,&error)); xrtValueRelease(bad); bad=NULL;
    int n=MultiByteToWideChar(CP_UTF8,0,native,-1,NULL,0),m=MultiByteToWideChar(CP_UTF8,0,alias,-1,NULL,0); wchar_t *nw=malloc((size_t)n*sizeof(*nw)),*aw=malloc((size_t)m*sizeof(*aw)); CHECK(nw && aw); MultiByteToWideChar(CP_UTF8,0,native,-1,nw,n); MultiByteToWideChar(CP_UTF8,0,alias,-1,aw,m); int linked=CreateHardLinkW(aw,nw,NULL); free(nw); free(aw); CHECK(linked);
    bad=xrtValueDeepClone(before); CHECK(bad && addFile(bad,"alias","hardlink.c","c") && !xideProjectApplyReferences(&model,bad,&error) && error.code==XIDE_MODEL_CONFLICT); xrtValueRelease(bad); bad=NULL;
    after=xideProjectReferencesJson(&model); CHECK(after && xrtValueEqual(before,after) && !model.dirty); xrtValueRelease(after); after=NULL;
    CHECK(xideProjectSave(&model,copyPath,&error) && xideProjectLoad(&loaded,copyPath,&error) && loaded.files.count==324 && xidePathEqual(loaded.resources.items[0],resource) && xidePathEqual(loaded.cSources.items[0],native) && !strcmp(text(get(loaded.source,"unknownRoot")),"保留根扩展") && !strcmp(text(get(loaded.files.items[1].source,"future")),"保留😀") && !strcmp(text(get(xrtValueArrayGet(loaded.libraries,1),"future")),"保留库扩展"));
    app.hidden=1; app.maxFrames=1; app.noSession=1; app.highDpi=1; app.dpiOverride=1.25f; app.settingsPath=settings; app.compilerPath=f->compiler;
    CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1;
    CHECK(xideWorkspaceCreate(&app.workspaceModel,"引用工作空间",&error) && xideWorkspaceAddProject(&app.workspaceModel,manifest,"alpha",&error) && xideWorkspaceSetStartup(&app.workspaceModel,"alpha",&error) && xideWorkspaceViewRefresh(&app)==XUI_OK && xideWorkspaceViewSelectProject(&app,"alpha") && settle(&app));
    xide_project* p=xideWorkspaceFindProject(&app.workspaceModel,"alpha"); CHECK(p && xideActionQuery(&app,XIDE_ACTION_REFERENCES).enabled && xideActionExecute(&app,XIDE_ACTION_REFERENCES)==XIDE_ACTION_DISPATCHED && xideReferencesViewIsOpen(&app) && settle(&app));
    CHECK(click(&app,XIDE_REF_CANCEL)); xui_widget referenceButton=findButton(app.context,xuiScrollViewGetContentWidget(app.properties),"项目文件、资源与库…");
    CHECK(referenceButton && xuiScrollViewEnsureChildVisible(app.properties,referenceButton)==XUI_OK && settle(&app) && pointer(&app,xuiWidgetGetWorldRect(referenceButton)) && xideReferencesViewIsOpen(&app));
    CHECK(xuiListViewGetItemCount(xideReferencesViewControl(&app,XIDE_REF_LIST))==324 && selectRow(&app,0) && click(&app,XIDE_REF_REMOVE) && strstr(xideReferencesViewMessage(&app),"入口"));
    CHECK(selectRow(&app,3)); xuiEditSetText(xideReferencesViewControl(&app,XIDE_REF_VALUE),"更改的 数据.json");
    CHECK(!xideReferencesViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && xideReferencesViewIsOpen(&app) && !p->dirty && !strcmp(xuiEditGetText(xideReferencesViewControl(&app,XIDE_REF_VALUE)),"更改的 数据.json"));
    CHECK(click(&app,XIDE_REF_UPDATE) && xideReferencesViewOpen(&app) && !strcmp(xuiEditGetText(xideReferencesViewControl(&app,XIDE_REF_VALUE)),"更改的 数据.json") && !p->dirty && xidePathEqual(p->files.items[3].path,data) && click(&app,XIDE_REF_CANCEL));
    for(int i=0;i<xuiTreeViewGetNodeCount(app.workspace);++i) { const xui_tree_view_node_t* node=xuiTreeViewGetNode(app.workspace,i); if(node && !strcmp(node->sText,"未来库")) { CHECK(xideWorkspaceTreeSelect(&app,node->iId)); treeKey=xideWorkspaceTreeKey(&app,node->iId); } }
    CHECK(treeKey);
    CHECK(xideReferencesViewOpen(&app) && choose(&app,XIDE_REF_CATEGORY,1) && selectRow(&app,1) && xuiComboBoxGetSelected(xideReferencesViewControl(&app,XIDE_REF_KIND))==1);
    xuiEditSetText(xideReferencesViewControl(&app,XIDE_REF_VALUE),"未来库更新😀");
    CHECK(click(&app,XIDE_REF_ADD) && xuiListViewGetItemCount(xideReferencesViewControl(&app,XIDE_REF_LIST))==2 && !strcmp(xuiEditGetText(xideReferencesViewControl(&app,XIDE_REF_VALUE)),"未来库更新😀"));
    CHECK(click(&app,XIDE_REF_UPDATE) && choose(&app,XIDE_REF_KIND,0)); xuiEditSetText(xideReferencesViewControl(&app,XIDE_REF_VALUE),"sqlite"); CHECK(click(&app,XIDE_REF_ADD) && click(&app,XIDE_REF_UP) && capture(&app,libsImage) && click(&app,XIDE_REF_APPLY) && !xideReferencesViewIsOpen(&app) && p->dirty && xrtValueCount(p->libraries)==3);
    CHECK(xideReferencesViewOpen(&app) && choose(&app,XIDE_REF_CATEGORY,1) && selectRow(&app,1));
    CHECK(click(&app,XIDE_REF_INSPECT) && strstr(xideReferencesViewLibraryDetails(&app),"命令目录版本：0.1.0"));
    if(f->compiler) CHECK(strstr(xideReferencesViewLibraryDetails(&app),"磁盘版本：0.1.0") && strstr(xideReferencesViewLibraryDetails(&app),"候选清单可读"));
    CHECK(selectRow(&app,2) && click(&app,XIDE_REF_INSPECT) && strstr(xideReferencesViewLibraryDetails(&app),"尚无检查适配器") && click(&app,XIDE_REF_DOCUMENTATION) && xideReferencesViewIsOpen(&app));
    CHECK(selectRow(&app,0) && click(&app,XIDE_REF_DOCUMENTATION) && strstr(xideReferencesViewMessage(&app),"尚未收录") && xideReferencesViewIsOpen(&app) && selectRow(&app,1));
    char* localDir=xidePathResolve(root,"sqlite"),*localFile=localDir ? xidePathResolve(localDir,"xlang.package.json") : NULL;
    CHECK(localDir && localFile && xrtDirCreateAll(localDir) && writeText(localFile,"{\"format\":\"xlang-package-1\",\"version\":\"9.9.9\"}") && click(&app,XIDE_REF_INSPECT) && strstr(xideReferencesViewLibraryDetails(&app),"磁盘版本：9.9.9") && strstr(xideReferencesViewLibraryDetails(&app),"与磁盘版本不同")); free(localDir); free(localFile);
    char* infoImage=xidePathResolve(root,"native-library-info.png"); CHECK(infoImage && capture(&app,infoImage)); free(infoImage);
    xuiEditSetText(xideReferencesViewControl(&app,XIDE_REF_VALUE),"尚未应用的新包"); CHECK(click(&app,XIDE_REF_ADD) && xuiListViewGetItemCount(xideReferencesViewControl(&app,XIDE_REF_LIST))==4 && xrtValueCount(p->libraries)==3 && selectRow(&app,1));
    xuiEditSetText(xideReferencesViewControl(&app,XIDE_REF_VALUE),"尚未更新的包名");
    CHECK(click(&app,XIDE_REF_DOCUMENTATION) && xideReferencesViewIsOpen(&app) && !xideReferencesViewSuspended(&app) && strstr(xideReferencesViewMessage(&app),"输入已经保留"));
    xuiEditSetText(xideReferencesViewControl(&app,XIDE_REF_VALUE),"sqlite");
    CHECK(click(&app,XIDE_REF_DOCUMENTATION) && !xideReferencesViewIsOpen(&app) && xideReferencesViewSuspended(&app) && settle(&app) && !strcmp(xuiLabelGetText(app.helpTitle),"sqlite"));
    CHECK(xideHelpChapterCount(xideCatalogHelpDocument(&app))>0 && xuiWidgetGetVisible(xideCatalogReturnReferencesWidget(&app)));
    char* helpImage=xidePathResolve(root,"native-library-help.png"); CHECK(helpImage && capture(&app,helpImage)); free(helpImage);
    CHECK(xideHelpJump(xideCatalogHelpDocument(&app),1) && settle(&app)); xideHelpApplyJump(xideCatalogHelpDocument(&app));
    char* docImage=xidePathResolve(root,"native-library-document.png"); CHECK(docImage && capture(&app,docImage)); free(docImage);
    CHECK(xideCatalogViewReload(&app) && settle(&app) && xideReferencesViewSuspended(&app));
    CHECK(pointer(&app,xuiWidgetGetWorldRect(xideCatalogReturnReferencesWidget(&app))) && xideReferencesViewIsOpen(&app) && !xideReferencesViewSuspended(&app) && xuiListViewGetSelected(xideReferencesViewControl(&app,XIDE_REF_LIST))==1 && !strcmp(xuiEditGetText(xideReferencesViewControl(&app,XIDE_REF_VALUE)),"sqlite") && xuiListViewGetItemCount(xideReferencesViewControl(&app,XIDE_REF_LIST))==4 && xrtValueCount(p->libraries)==3 && click(&app,XIDE_REF_CANCEL) && xrtValueCount(p->libraries)==3);
    CHECK(xideReferencesViewOpen(&app) && choose(&app,XIDE_REF_CATEGORY,1) && selectRow(&app,1) && click(&app,XIDE_REF_DOCUMENTATION));
    CHECK(xideProjectSetMetadata(p,p->name,"帮助期间改变的项目",p->kind,&error) && pointer(&app,xuiWidgetGetWorldRect(xideCatalogReturnReferencesWidget(&app))) && !xideReferencesViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && xideReferencesViewIsOpen(&app) && !strcmp(xuiEditGetText(xideReferencesViewControl(&app,XIDE_REF_VALUE)),"sqlite") && click(&app,XIDE_REF_CANCEL));
    CHECK(!xideReferencesViewSuspended(&app) && xideWorkspaceSaveAll(&app,workspace,&error));
    char* restoredKey=xideWorkspaceTreeKey(&app,xuiTreeViewGetSelected(app.workspace)); CHECK(restoredKey && !strcmp(treeKey,restoredKey)); xrtFree(restoredKey);
    CHECK(!strcmp(text(get(xrtValueArrayGet(p->libraries,2),"kind")),"future-library-kind") && !strcmp(text(get(xrtValueArrayGet(p->libraries,2),"id")),"future-library"));
    CHECK(xideReferencesViewOpen(&app) && choose(&app,XIDE_REF_KIND,5)); xuiEditSetText(xideReferencesViewControl(&app,XIDE_REF_VALUE),"新引用.txt");
    CHECK(!xideReferencesViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && xideReferencesViewIsOpen(&app));
    CHECK(click(&app,XIDE_REF_ADD) && selectRow(&app,2) && capture(&app,filesImage) && click(&app,XIDE_REF_APPLY) && p->files.count==325 && xideWorkspaceSaveAll(&app,workspace,&error));
    CHECK(xideEditorOpen(&app,native,&error) && settle(&app)); xide_document* doc=app.activeDocument; uint32_t version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(doc->editor));
    CHECK(xideReferencesViewOpen(&app) && choose(&app,XIDE_REF_KIND,0)); xuiEditSetText(xideReferencesViewControl(&app,XIDE_REF_VALUE),"library-field"); CHECK(xuiSetFocusWidget(app.context,xideReferencesViewControl(&app,XIDE_REF_VALUE))==XUI_OK && xuiInputKeyDownEx(app.context,'Z',XUI_MOD_CTRL,NULL)==XUI_OK && xuiInputKeyUpEx(app.context,'Z',0,NULL)==XUI_OK && settle(&app) && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(doc->editor))==version);
    CHECK(xideActionExecute(&app,XIDE_ACTION_NEW_FILE)==XIDE_ACTION_DISABLED && !xideConfigurationViewOpen(&app) && !xideFontViewOpen(&app));
    CHECK(xideProjectSetMetadata(p,p->name,"外部模型变化",p->kind,&error) && !xideReferencesViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && click(&app,XIDE_REF_CANCEL));
    CHECK(xideWorkspaceSaveAll(&app,NULL,&error) && xideEditorOpen(&app,manifest,&error)); xide_document* md=app.activeDocument;
    CHECK(xuiCodeEditSetText(md->editor,"{\n")==XUI_OK && settle(&app) && md->dirty && xideReferencesViewOpen(&app) && !xideReferencesViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && click(&app,XIDE_REF_CANCEL));
    CHECK(xideDocumentReload(md,1,&error) && settle(&app) && xideReferencesViewOpen(&app));
    CHECK(xideUiFontApply(&app,app.uiFontPath,40,&error) && resizeTarget(&app,900,600));
    CHECK(settle(&app) && click(&app,XIDE_REF_CANCEL) && !xideReferencesViewIsOpen(&app) && xideReferencesViewOpen(&app) && click(&app,XIDE_REF_APPLY));
    CHECK(xideReferencesViewOpen(&app)); CHECK(choose(&app,XIDE_REF_CATEGORY,1)); CHECK(selectRow(&app,1) && click(&app,XIDE_REF_INSPECT));
    CHECK(xuiScrollViewEnsureChildVisible(xideDialogGetScrollViewWidget(xideReferencesViewWindow(&app)),xideReferencesViewControl(&app,XIDE_REF_APPLY))==XUI_OK && capture(&app,largeImage));
    CHECK(click(&app,XIDE_REF_CANCEL) && xideUiFontApply(&app,app.uiFontPath,16,&error) && resizeTarget(&app,1280,800));
    CHECK(xideReferencesViewOpen(&app) && selectRow(&app,1) && click(&app,XIDE_REF_REMOVE) && click(&app,XIDE_REF_APPLY) && p->cSources.count==0 && xrtFileExists(native) && doc->open && xideWorkspaceSaveAll(&app,NULL,&error));
    CHECK(xideProjectAddFile(p,native,"c",&error) && xideWorkspaceSaveAll(&app,NULL,&error));
    CHECK(xideReferencesViewOpen(&app)); xvalue* disk=xideProjectExport(p,&error); CHECK(disk && put(get(disk,"ide"),"name","磁盘更新的项目"));
    size_t diskLength=0; char* diskText=xrtJsonStringify(disk,false,&diskLength); xrtValueRelease(disk); CHECK(diskText && writeText(manifest,diskText)); xrtFree(diskText);
    uint64_t changesBefore=xideFileSyncRevision(&app),deadline=GetTickCount64()+8000;
    do { CHECK(settle(&app)); if(xideFileSyncRevision(&app)!=changesBefore && xideFileSyncCount(&app)) break; Sleep(5); } while(GetTickCount64()<deadline);
    CHECK(xideReferencesViewIsOpen(&app) && strcmp(p->name,"磁盘更新的项目") && xideFileSyncCount(&app));
    CHECK(click(&app,XIDE_REF_CANCEL)); deadline=GetTickCount64()+8000;
    do { CHECK(settle(&app)); p=xideWorkspaceFindProject(&app.workspaceModel,"alpha"); if(p && !strcmp(p->name,"磁盘更新的项目")) break; Sleep(5); } while(GetTickCount64()<deadline);
    CHECK(p && !strcmp(p->name,"磁盘更新的项目") && !p->dirty && doc->open);
    if(f->compiler) {
        xide_task* task=xideTasksBuild(app.tasks,p,p->activeConfiguration,f->compiler,1,&error); CHECK(task); xide_task_info info={0}; uint64_t until=GetTickCount64()+45000;
        do { CHECK(settle(&app)); xideTaskInfo(task,&info); if(info.state>=XIDE_TASK_SUCCEEDED) break; Sleep(5); } while(GetTickCount64()<until);
        if(info.state!=XIDE_TASK_SUCCEEDED) fprintf(stderr,"resource task: %s\n%s\n",info.error,xideTaskOutput(task));
        CHECK(info.state==XIDE_TASK_SUCCEEDED && info.built && info.ran && versionMatches(xideTaskOutput(task)));
        uint64_t cursor=0,dropped=0; char log[4096]; size_t length=xideTaskReadTerminal(task,&cursor,log,sizeof(log)-1,&dropped); log[length]=0; CHECK(strstr(log,"RESOURCE_OK"));
        puts("references_test real DEMO6 resource integration passed: C/xlang build/run and actual PE version 2.3.4.5; ordinary resource and package metadata were not compiled/imported.");
    }
    puts("references_test passed: transactional 324-file model, identity/extension/entry/duplicate/NUL guards, hardlinks, unknown metadata, relative Save As, native draft isolation/pending-field protection and buttons/category/list, stable libraries, cancel/no-op/stale/dirty manifest protection, plain-field shortcut, actual external manifest deferral/resume, modal/DPI/fonts and six screenshots, reference-only removal, read-only candidate/version/file inspection and package help draft suspension/resume/reload/stale protection.");
cleanup:
    if(began) xgeEnd();
    xideAppDestroy(&app); xideProjectClear(&model); xideProjectClear(&loaded); xrtValueRelease(refs); xrtValueRelease(before); xrtValueRelease(bad); xrtValueRelease(after);
    free(root); free(entry); free(native); free(resource); free(data); free(manifest); free(workspace); free(settings); free(copyDir); free(copyPath); free(alias); free(filesImage); free(libsImage); free(largeImage); xrtFree(treeKey); xgeQuit(); return XGE_OK;
}
static DWORD WINAPI worker(void* data)
{ fixture* f=data; if(!SetThreadDesktop(f->desktop)) return 1; xge_desc_t d={0}; d.iWidth=1600; d.iHeight=1000; d.sTitle="xlang project references verification"; d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; d.iRunMode=XGE_RUN_GAME_LOOP; int result=xgeInit(&d); if(result==XGE_OK) result=xgeRun(verify,f); xgeUnit(); return result==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv)
{
    if(argc<2 || argc>3 || !xrtDirCreateAll(argv[1])) return 2;
    fixture f={.directory=argv[1],.compiler=argc==3 ? argv[2] : NULL}; wchar_t name[100]; swprintf(name,100,L"XlangReferencesVerification-%lu",GetCurrentProcessId()); f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) return 1;
    HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1; if(thread) { if(WaitForSingleObject(thread,90000)!=WAIT_OBJECT_0) { fputs("references_test timed out\n",stderr); ExitProcess(1); } GetExitCodeThread(thread,&code); CloseHandle(thread); } CloseDesktop(f.desktop); return (int)code;
}
