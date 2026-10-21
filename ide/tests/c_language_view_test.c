#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "ui/language_view.h"
#include "ui/task_view.h"
#include "ui/font_view.h"
#include "ui/configuration_view.h"
#include "platform/path.h"
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Native production adapter, including rejection of mutated real replies. */
#include "../src/ui/c_language_view.c"
typedef struct fixture { const char *directory,*server; HDESK desktop; int started,failed,checks; } fixture;
#define CHECK(t) do { if(!(t)) { fprintf(stderr,"c_language_view_test:%d: %s; %s; C %s\n",__LINE__,#t,error.message,xideCLanguageViewMessage(&app)); f->failed=1; goto cleanup; } ++f->checks; } while(0)
static int writeText(const char* p,const char* s) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static int settle(xide_app* a) { if(xuiDispatchPendingEvents(a->context)!=XUI_OK) return 0; xideEditorPoll(a); xideTaskViewPoll(a); xideLanguageViewPoll(a); xideCLanguageViewPoll(a); xideWorkbenchLayout(a); return xuiLayout(a->context)==XUI_OK && !a->frameError; }
static int idle(xide_app* a,int ms) { uint64_t until=GetTickCount64()+(uint64_t)ms; do { if(!settle(a)) return 0; Sleep(5); } while(GetTickCount64()<until); return 1; }
static int key(xide_app* a,int k,uint32_t mods) { return xuiInputKeyDownEx(a->context,k,mods,NULL)==XUI_OK && xuiInputKeyUpEx(a->context,k,mods,NULL)==XUI_OK && settle(a); }
static int position(xide_app* a,xide_document* d,int offset) { return xideEditorShow(a,d)==XUI_OK && xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(d->editor),xuiCodeEditGetDocument(d->editor),offset,0)==XUI_OK && xuiSetFocusWidget(a->context,d->editor)==XUI_OK && settle(a); }
static int replace(xide_document* d,const char* s) { return xuiCodeDocumentReplace(xuiCodeEditGetDocument(d->editor),0,xuiCodeDocumentGetLength(xuiCodeEditGetDocument(d->editor)),s)==XUI_OK; }
static int waitReady(xide_app* a) { uint64_t until=GetTickCount64()+30000; do { if(!settle(a)) return 0; xide_c_language_view* v=a->cLanguageView; if(v->ready && v->inputs && v->received==(int)v->inputs->count && v->symbolResult) return 1; if(v->paused) return 0; Sleep(5); } while(GetTickCount64()<until); return 0; }
static int check(xide_app* a) { return xideActionExecute(a,XIDE_ACTION_CHECK_CURRENT)==XIDE_ACTION_DISPATCHED && waitReady(a) && xuiSetFocusWidget(a->context,a->activeDocument->editor)==XUI_OK && settle(a); }
static int waitHover(xide_app* a) { uint64_t until=GetTickCount64()+30000; do { if(!settle(a)) return 0; if(a->cLanguageView->hoverResult) return 1; if(!a->cLanguageView->hover.token && !a->cLanguageView->hoverDue) return 0; Sleep(5); } while(GetTickCount64()<until); return 0; }
static int waitDefinition(xide_app* a) { uint64_t until=GetTickCount64()+30000; do { if(!settle(a)) return 0; if(!a->cLanguageView->definition.token) return 1; Sleep(5); } while(GetTickCount64()<until); return 0; }
static int record(const char* root,const char* name,const xvalue* value) { char* path=xidePathResolve(root,name); size_t n=0; char* text=xrtJsonStringify(value,true,&n); int okay=path && text && xrtFileWriteAtomic(path,(xbytesview){(unsigned char*)text,n}); free(path); xrtFree(text); return okay; }
static int capture(xide_app* a,const char* path) { if(!settle(a) || xuiUpdate(a->context,0)!=XUI_OK || xuiLayout(a->context)!=XUI_OK) return 0; xui_rect_i_t r={0,0,a->viewportWidth,a->viewportHeight}; return a->proxy.surfaceClear(&a->proxy,a->target,0xffeeeeeeu)==XUI_OK && xuiRender(a->context,a->target,&r,1)==XUI_OK && xideAppCapture(a,path)==XGE_OK; }
/* Only terminate the real server children created by this test executable. */
static int crashOwnedServer(const char* server)
{
 HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0); if(snapshot==INVALID_HANDLE_VALUE) return 0;
 PROCESSENTRY32W row={0}; row.dwSize=sizeof(row); int killed=0;
 for(BOOL next=Process32FirstW(snapshot,&row);next;next=Process32NextW(snapshot,&row)) if(row.th32ParentProcessID==GetCurrentProcessId()) {
  HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_TERMINATE,FALSE,row.th32ProcessID); wchar_t wide[32768]; DWORD n=32768;
  if(process && QueryFullProcessImageNameW(process,0,wide,&n)) {
   int bytes=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide,-1,NULL,0,NULL,NULL); char* path=bytes ? malloc((size_t)bytes) : NULL;
   if(path && WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide,-1,path,bytes,NULL,NULL) && xidePathLexicalEqual(path,server) && TerminateProcess(process,91)) ++killed;
   free(path);
  } if(process) CloseHandle(process);
 } CloseHandle(snapshot); return killed>0;
}
static xvalue* reply(xide_c_language_view* v,enum xide_c_query kind,uint64_t id,int offset)
{
 uint64_t token=xideCAnalysisJobRequest(v->job,kind,id,offset),until=GetTickCount64()+30000; xide_c_event e={0}; if(!token) return NULL;
 while(GetTickCount64()<until && xideCCompletionJobState(v->job,NULL,0)==XIDE_C_JOB_READY) { if(xideCAnalysisJobTake(v->job,token,&e)) { xvalue* message=e.message; e.message=NULL; return message; } Sleep(5); }
 xideCAnalysisJobCancelRequest(v->job,token); return NULL;
}
static int setValue(xvalue* v,const char* name,xvalue* item) { return item && xrtValueObjectSetNew(v,cvsv(name),item); }
static int verify(void* user)
{
 fixture* f=user; if(f->started) return XGE_OK; f->started=1; xide_app app={0}; xide_model_error error={0}; xide_project project={0}; int began=0; xvalue *validHover=NULL,*validSymbols=NULL,*validDefinition=NULL,*fake=NULL; unsigned char* savedManifest=NULL; size_t savedManifestLength=0;
 char *root=NULL,*entry=NULL,*header=NULL,*disk=NULL,*xlpath=NULL,*manifest=NULL,*light=NULL,*dark=NULL,*settings=NULL,*chainLeaf=NULL,*chainImage=NULL;
 char unique[80]; snprintf(unique,sizeof(unique),"c-semantic-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64()); root=xidePathResolve(f->directory,unique);
 entry=xidePathResolve(root,"入口😀.c"); header=xidePathResolve(root,"types.h"); disk=xidePathResolve(root,"disk.h"); xlpath=xidePathResolve(root,"main.xl"); manifest=xidePathResolve(root,"工程.xlp"); light=xidePathResolve(root,"c-semantic-light.png"); dark=xidePathResolve(root,"c-semantic-dark.png"); settings=xidePathResolve(root,"settings.json");
 const char *saved="int main(void) { return 0; }\r\n",*types="typedef struct Box { int value; } Box;\n",*diskText="static const int disk_value = 3;\n";
 const char *source="#include \"types.h\"\n#include \"disk.h\"\nconst char *标签 = \"😀\";\nint main(void) { Box box = {0}; return box.value + disk_value; }\n";
 CHECK(root && entry && header && disk && xlpath && manifest && settings && light && dark && xrtDirCreateAll(root));
 CHECK(writeText(entry,saved) && writeText(header,types) && writeText(disk,diskText) && writeText(xlpath,"func main(...) as int\n    return 0\nend\n"));
 CHECK(xideProjectCreate(&project,manifest,"C native semantics",xlpath,&error) && xideProjectAddFile(&project,entry,"c",&error) && xideProjectAddFile(&project,header,"header",&error) && xideProjectAddFile(&project,disk,"header",&error) && xideProjectSave(&project,manifest,&error));
 app.hidden=1; app.noSession=1; app.cServerPath=f->server; app.settingsPath=settings; app.projectPath=manifest;
 CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1; xide_c_language_view* v=app.cLanguageView;
 CHECK(xideEditorOpen(&app,entry,&error)); xide_document* c=app.activeDocument;
 CHECK(xideEditorOpen(&app,header,&error)); xide_document* h=app.activeDocument;
 CHECK(replace(h,"typedef struct Box { const char *value; } Box;\n") && replace(c,source) && position(&app,c,(int)(strstr(source,"box.value")-source)+5) && check(&app));
 CHECK(v->received==2 && v->diagnosticCount>0 && v->symbolCount>=2 && c->dirty && h->dirty);
 CHECK(xideTaskViewLanguageProblemsOwner(&app)==v && xuiCodeAnnotationGetDiagnosticCount(xuiCodeEditGetAnnotations(c->editor))>0);
 CHECK(xideActionQuery(&app,XIDE_ACTION_DEFINITION).enabled && xideActionQuery(&app,XIDE_ACTION_HOVER).enabled && xideActionCount()==71);
 CHECK(record(root,"symbols.json",v->symbolResult));
 for(size_t i=0;i<v->eventCount;++i) if(v->diagnostics[i].documentId==c->id) CHECK(record(root,"diagnostics.json",v->diagnostics[i].message));
 CHECK(xideTaskViewJump(&app,0) && app.activeDocument==c); xui_code_selection_t selection={0}; CHECK(xuiCodeSelectionGetState(xuiCodeEditGetSelection(c->editor),&selection)==XUI_OK && selection.iAnchorOffset==v->flat[0].start && selection.iCaretOffset==v->flat[0].end);
 CHECK(position(&app,c,(int)(strstr(source,"box.value")-source)+5)); CHECK(key(&app,'I',XUI_MOD_CTRL)); CHECK(waitHover(&app));
 CHECK(xideCLanguageViewHintOpen(&app) && strstr(cvstr(cvget(cvget(v->hoverResult,"contents"),"value")),"const char") && record(root,"hover.json",v->hoverResult)); validHover=xrtValueDeepClone(v->hoverResult); CHECK(validHover);
 /* Actual pointer hit-test, complete multiline content and movement into popup. */
 CHECK(!strcmp(xuiTextEditGetText(v->hoverText),cvstr(cvget(cvget(v->hoverResult,"contents"),"value"))) && xuiWidgetGetWorldRect(v->hoverText).fH>100);
 xui_rect_t glyph={0}; CHECK(xuiCodeEditGetTextOffsetRect(c->editor,v->hover.offset,&glyph)==XUI_OK && glyph.fW>0 && xuiInputPointerMove(app.context,(int)(glyph.fX+1),(int)(glyph.fY+glyph.fH/2),0)==XUI_OK);
 CHECK(settle(&app) && waitHover(&app));
 CHECK(v->hoverResult && v->hoverPointer && xideCLanguageViewHintOpen(&app));
 xui_rect_t popupRect=xuiWidgetGetWorldRect(v->hoverText); CHECK(xuiInputPointerMove(app.context,(int)(popupRect.fX+30),(int)(popupRect.fY+30),0)==XUI_OK && settle(&app) && xideCLanguageViewHintOpen(&app));
 CHECK(xuiInputPointerMove(app.context,20,120,0)==XUI_OK && settle(&app) && !xideCLanguageViewHintOpen(&app) && !v->hoverResult);
 CHECK(position(&app,c,(int)(strstr(source,"box.value")-source)+5) && key(&app,'I',XUI_MOD_CTRL) && waitHover(&app));
 validSymbols=xrtValueDeepClone(v->symbolResult); validDefinition=reply(v,XIDE_C_DEFINITION,c->id,v->hover.offset); CHECK(validSymbols && validDefinition && record(root,"definition.json",validDefinition));
 /* Mutations of real standard responses cannot publish malformed UI ranges. */
 for(int variant=0;variant<5;++variant) {
  fake=xrtValueObject(); xvalue* result=xrtValueDeepClone(validHover); CHECK(fake && result && setValue(fake,"result",result));
  if(variant==0) CHECK(setValue((xvalue*)cvget(cvget(result,"range"),"end"),"character",xrtValueInt(999999)));
  if(variant==1) CHECK(setValue((xvalue*)cvget(result,"contents"),"kind",xrtValueString(cvsv("markdown"))));
  if(variant==2) CHECK(setValue((xvalue*)cvget(result,"contents"),"value",xrtValueString(cvsv(""))));
  if(variant==3) CHECK(setValue((xvalue*)cvget(cvget(result,"range"),"start"),"line",xrtValueInt(999999)));
  if(variant==4) CHECK(setValue((xvalue*)cvget(cvget(result,"range"),"end"),"character",xrtValueInt(0)));
  CHECK(!cvapplyHover(v,fake)); xrtValueRelease(fake); fake=NULL;
 }
 fake=xrtValueObject(); CHECK(fake && setValue(fake,"result",xrtValueDeepClone(validHover)) && position(&app,c,0) && !cvapplyHover(v,fake)); xrtValueRelease(fake); fake=NULL;
 CHECK(position(&app,c,(int)(strstr(source,"box.value")-source)+5) && key(&app,'I',XUI_MOD_CTRL) && waitHover(&app));
 fake=xrtValueObject(); CHECK(fake && setValue(fake,"result",xrtValueDeepClone(validSymbols))); xvalue* symbol=(xvalue*)xrtValueArrayGet(cvget(fake,"result"),0);
 CHECK(setValue((xvalue*)cvget(cvget(symbol,"selectionRange"),"end"),"character",xrtValueInt(999999)) && !cvapplyOutline(v,fake)); xrtValueRelease(fake); fake=NULL;
 xide_c_event invalid={0}; invalid=v->diagnostics[0]; invalid.message=xrtValueDeepClone(invalid.message); invalid.version++;
 CHECK(invalid.message && !cvapplyDiagnostics(v,&invalid,1,v->sequence+1)); xideCEventClear(&invalid);
 CHECK(xideFontViewOpen(&app) && settle(&app) && !xideCLanguageViewHintOpen(&app) && !xideActionQuery(&app,XIDE_ACTION_HOVER).enabled && xideDialogSetOpen(xideFontViewWindow(&app),0)==XUI_OK);
 CHECK(position(&app,c,(int)(strstr(source,"box.value")-source)+5) && key(&app,'I',XUI_MOD_CTRL) && waitHover(&app));
 CHECK(xideActionExecute(&app,XIDE_ACTION_OUTLINE)==XIDE_ACTION_DISPATCHED && xuiDockPanelGetWindowClient(app.dock,app.windows[XIDE_OUTLINE])==v->outline && capture(&app,light));
 CHECK(xideWorkbenchApplyTheme(&app,1)==XUI_OK && capture(&app,dark));
 CHECK(position(&app,c,(int)(strstr(source,"box.value")-source)+5) && key(&app,XUI_KEY_F12,0) && waitDefinition(&app));
 CHECK(app.activeDocument==h && xuiCodeSelectionGetState(xuiCodeEditGetSelection(h->editor),&selection)==XUI_OK && selection.iAnchorOffset==(int)(strstr(xuiCodeEditGetText(h->editor),"value")-xuiCodeEditGetText(h->editor)) && selection.iCaretOffset-selection.iAnchorOffset==5);
 CHECK(position(&app,c,0) && waitReady(&app)); int mainId=0; for(size_t i=0;i<v->symbolCount;++i) if(!strncmp(v->symbols[i].title,"main",4)) mainId=(int)i+1;
 CHECK(mainId && xuiTreeViewSetSelected(v->outline,mainId)==XUI_OK && xuiSetFocusWidget(app.context,v->outline)==XUI_OK && key(&app,XUI_KEY_ENTER,0));
 CHECK(app.activeDocument==c && xuiCodeSelectionGetState(xuiCodeEditGetSelection(c->editor),&selection)==XUI_OK && selection.iCaretOffset-selection.iAnchorOffset==4);
 CHECK(replace(h,"typedef struct Box { double value; } Box;\n") && position(&app,c,(int)(strstr(source,"box.value")-source)+5) && settle(&app) && !v->ready && !v->symbolResult && !v->diagnosticCount && !xideTaskViewLanguageProblemsOwner(&app));
 CHECK(check(&app)); CHECK(v->diagnosticCount==0 && key(&app,'I',XUI_MOD_CTRL) && waitHover(&app) && strstr(cvstr(cvget(cvget(v->hoverResult,"contents"),"value")),"double") && record(root,"changed-header-hover.json",v->hoverResult));
 chainLeaf=xidePathResolve(root,"leaf 草稿😀.h");chainImage=xidePathResolve(root,"c-unsaved-header-chain.png");
 xide_project* chainProject=&app.workspaceModel.projects[0];
 CHECK(chainLeaf && chainImage && writeText(chainLeaf,"#define FIELD_TYPE int\n") && xideProjectAddFile(chainProject,chainLeaf,"header",&error) && xideProjectSave(chainProject,chainProject->manifest,&error) && xideEditorOpen(&app,chainLeaf,&error));
 xide_document* chain=app.activeDocument;
 const char* chainHeader="#include \"leaf 草稿😀.h\"\ntypedef struct Box { FIELD_TYPE value; } Box;\n";
 CHECK(replace(chain,"#define FIELD_TYPE double\n") && replace(h,chainHeader) && position(&app,h,(int)(strstr(chainHeader,"value")-chainHeader)+1) && check(&app) && key(&app,'I',XUI_MOD_CTRL) && waitHover(&app));
 CHECK(strstr(xuiTextEditGetText(v->hoverText),"double") && record(root,"unsaved-header-chain-hover.json",v->hoverResult) && capture(&app,chainImage));
 CHECK(replace(chain,"#define FIELD_TYPE const char*\n") && settle(&app) && !v->ready && check(&app) && key(&app,'I',XUI_MOD_CTRL) && waitHover(&app));
 CHECK(strstr(xuiTextEditGetText(v->hoverText),"const char") && record(root,"changed-leaf-header-chain-hover.json",v->hoverResult));
 CHECK(replace(h,"typedef struct Box { double value; } Box;\n"));
 const char* diskQuery="#include \"disk.h\"\nint main(void) { return disk_value; }\n";
 CHECK(replace(c,diskQuery) && position(&app,c,(int)(strstr(diskQuery,"disk_value")-diskQuery)+2) && check(&app) && key(&app,XUI_KEY_F12,0) && waitDefinition(&app));
 CHECK(app.activeDocument->path && xidePathLexicalEqual(app.activeDocument->path,disk) && xuiCodeSelectionGetState(xuiCodeEditGetSelection(app.activeDocument->editor),&selection)==XUI_OK && selection.iCaretOffset-selection.iAnchorOffset==10);
 CHECK(position(&app,c,0) && check(&app));
 CHECK(xideEditorOpen(&app,xlpath,&error) && settle(&app) && xuiDockPanelGetWindowClient(app.dock,app.windows[XIDE_OUTLINE])==v->xlangOutline); xide_document* xl=app.activeDocument;
 CHECK(!xideCLanguageViewCanQuery(&app,XIDE_C_HOVER) && !v->symbolResult && !xideCLanguageViewHintOpen(&app));
 CHECK(position(&app,c,0) && waitReady(&app) && xuiDockPanelGetWindowClient(app.dock,app.windows[XIDE_OUTLINE])==v->outline);
 /* Real long function documentation is wrapped and scrollable, without truncation. */
 char longSource[8192]="/**\n"; size_t longLength=strlen(longSource);
 for(int i=0;i<60;++i) longLength+=(size_t)snprintf(longSource+longLength,sizeof(longSource)-longLength," * Documentation line %d: complete original server text for scrolling.\n",i);
 snprintf(longSource+longLength,sizeof(longSource)-longLength," */\nint long_answer(void) { return 3; }\nint main(void) { return long_answer(); }\n");
 CHECK(replace(c,longSource) && position(&app,c,(int)(strrchr(longSource,'l')-longSource)) && check(&app));
 int longCall=(int)(strstr(strstr(longSource,"int main"),"long_answer")-longSource); CHECK(position(&app,c,longCall+2) && key(&app,'I',XUI_MOD_CTRL) && waitHover(&app));
 CHECK(strlen(xuiTextEditGetText(v->hoverText))>3000 && strstr(xuiTextEditGetText(v->hoverText),"Documentation line 59") && record(root,"long-hover.json",v->hoverResult));
 xui_scroll_model_t* scroll=xuiTextEditGetScrollModel(v->hoverText); CHECK(scroll && scroll->fContentHeight>scroll->tViewportRect.fH && xuiTextEditScrollBy(v->hoverText,0,10000)==XUI_OK && settle(&app));
 float scrollX=0,scrollY=0; CHECK(xuiTextEditGetScroll(v->hoverText,&scrollX,&scrollY)==XUI_OK && scrollY>0);
 char* longImage=xidePathResolve(root,"c-long-hover.png"); int longCapture=longImage && capture(&app,longImage); free(longImage); CHECK(longCapture);
 CHECK(replace(c,diskQuery) && position(&app,c,0) && check(&app));
 /* Analysis-only configuration is consumed by the real native C session.
    This forced header is not an explicit FileEntry: the command enrolls it. */
 char* forced=xidePathResolve(root,"强制 声明😀.h"); CHECK(forced && writeText(forced,"typedef int BridgeValue;\n"));
 fake=xideProjectConfigurationsJson(&app.workspaceModel.projects[0]); xvalue* ca=xrtJsonParse(cvsv("{\"systemIncludes\":[],\"forcedIncludes\":[],\"defines\":[\"C_ANSWER=42\"],\"undefines\":[]}"));
 CHECK(fake && ca && xrtValueArrayAppendNew((xvalue*)cvget(ca,"forcedIncludes"),xrtValueString(cvsv(forced))) && setValue(xrtValueArrayGet(fake,0),"cAnalysis",ca));
 CHECK(xideProjectApplyConfigurations(&app.workspaceModel.projects[0],fake,app.workspaceModel.projects[0].activeConfiguration,&error)); xrtValueRelease(fake); fake=NULL;
 const char* configured="BridgeValue native_answer(void) { return C_ANSWER; }\nint main(void) { return native_answer(); }\n";
 CHECK(replace(c,configured) && position(&app,c,(int)(strstr(configured,"C_ANSWER")-configured)+2) && check(&app) && v->diagnosticCount==0);
 CHECK(key(&app,'I',XUI_MOD_CTRL) && waitHover(&app) && strstr(xuiTextEditGetText(v->hoverText),"42") && record(root,"configured-macro-hover.json",v->hoverResult));
 char* configuredImage=xidePathResolve(root,"configured-c-hover.png"); CHECK(configuredImage && capture(&app,configuredImage)); free(configuredImage);
 CHECK(xideConfigurationViewOpen(&app) && xuiComboBoxOpen(xideConfigurationViewControl(&app,XIDE_CONFIG_CATEGORY))==XUI_OK && settle(&app));
 char* configurationImage=xidePathResolve(root,"c-analysis-configuration.png"); CHECK(configurationImage && capture(&app,configurationImage)); free(configurationImage);
 CHECK(xuiComboBoxClose(xideConfigurationViewControl(&app,XIDE_CONFIG_CATEGORY))==XUI_OK && xideDialogSetOpen(xideConfigurationViewWindow(&app),0)==XUI_OK && check(&app));
 CHECK(writeText(forced,"typedef double BridgeValue;\n")); uint64_t forcedUntil=GetTickCount64()+6000;
 while(xideCCompletionJobState(v->job,NULL,0)==XIDE_C_JOB_READY && GetTickCount64()<forcedUntil) Sleep(5);
 CHECK(xideCCompletionJobState(v->job,NULL,0)==XIDE_C_JOB_INVALIDATED && settle(&app) && !v->ready && !xideCLanguageViewHintOpen(&app));
 CHECK(writeText(forced,"typedef int BridgeValue;\n") && check(&app)); free(forced);
 fake=xideProjectConfigurationsJson(&app.workspaceModel.projects[0]); CHECK(fake && xrtValueObjectRemove(xrtValueArrayGet(fake,0),cvsv("cAnalysis")) && xideProjectApplyConfigurations(&app.workspaceModel.projects[0],fake,app.workspaceModel.projects[0].activeConfiguration,&error)); xrtValueRelease(fake); fake=NULL;
 CHECK(replace(c,diskQuery) && position(&app,c,0) && check(&app));
 /* Worker request capacity/cancellation is independent of UI pending queries. */
 CHECK(!xideCAnalysisJobRequest(v->job,XIDE_C_HOVER,UINT64_MAX,0) && !xideCAnalysisJobRequest(v->job,XIDE_C_HOVER,c->id,999999) && v->ready);
 uint64_t tokens[64]={0}; int queued=1; for(int i=0;i<64;++i) { tokens[i]=xideCAnalysisJobRequest(v->job,XIDE_C_SYMBOLS,c->id,0); if(!tokens[i]) queued=0; }
 CHECK(queued && !xideCAnalysisJobRequest(v->job,XIDE_C_SYMBOLS,c->id,0));
 for(int i=0;i<64;++i) xideCAnalysisJobCancelRequest(v->job,tokens[i]);
 CHECK(idle(&app,250) && v->ready); xide_c_event cancelled={0}; for(int i=0;i<64;++i) CHECK(!xideCAnalysisJobTake(v->job,tokens[i],&cancelled));
 CHECK(position(&app,c,0) && xideDocumentSetLanguage(c,"text",&error) && settle(&app) && !v->ready && !v->diagnosticCount && !xideCLanguageViewCanCheck(&app));
 CHECK(xideDocumentSetLanguage(c,"c",&error) && check(&app));
 app.configurationId="missing-configuration"; CHECK(settle(&app) && !v->ready && !v->diagnosticCount); app.configurationId=NULL; CHECK(check(&app));
 app.cServerPath="missing-server.exe"; CHECK(settle(&app) && !v->ready && !v->symbolResult); app.cServerPath=f->server; CHECK(check(&app));
 CHECK(crashOwnedServer(f->server)); uint64_t crashUntil=GetTickCount64()+6000; while(xideCCompletionJobState(v->job,NULL,0)!=XIDE_C_JOB_INVALIDATED && GetTickCount64()<crashUntil) Sleep(5);
 CHECK(xideCCompletionJobState(v->job,NULL,0)==XIDE_C_JOB_INVALIDATED && settle(&app) && !v->ready && !v->diagnosticCount && check(&app));
 CHECK(xideActionExecute(&app,XIDE_ACTION_CANCEL_CHECK)==XIDE_ACTION_DISPATCHED && v->paused && !v->job && !v->diagnosticCount && !v->symbolResult && idle(&app,700) && !v->job);
 CHECK(check(&app) && v->ready);
 /* C clearing cannot erase xlang problems, and vice versa. */
 const char* one[]={"synthetic owner routing only"};
 CHECK(xideTaskViewShowLanguageProblemsScoped(&app,"xlang route",one,1,0)==XUI_OK); cvclearDiagnostics(v); CHECK(xideTaskViewLanguageProblemsOwner(&app)==app.languageView);
 CHECK(check(&app) && xideTaskViewLanguageProblemsOwner(&app)==v); xideTaskViewClearLanguageProblems(&app); CHECK(xideTaskViewLanguageProblemsOwner(&app)==v);
 CHECK(position(&app,xl,0) && settle(&app)); CHECK(position(&app,c,0) && check(&app));
 savedManifest=xrtFileReadAllLimit(manifest,1024*1024,&savedManifestLength); CHECK(savedManifest && writeText(manifest,"{}")); uint64_t until=GetTickCount64()+6000; while(xideCCompletionJobState(v->job,NULL,0)!=XIDE_C_JOB_INVALIDATED && GetTickCount64()<until) Sleep(5);
 CHECK(xideCCompletionJobState(v->job,NULL,0)==XIDE_C_JOB_INVALIDATED && !xideActionQuery(&app,XIDE_ACTION_DEFINITION).enabled && settle(&app) && !v->ready && !v->symbolResult && !v->diagnosticCount);
 CHECK(xrtFileWriteAtomic(manifest,(xbytesview){savedManifest,savedManifestLength}) && check(&app));
 uint64_t oldId=c->id; CHECK(xideEditorClose(&app,c,1,&error) && settle(&app) && !v->ready); xide_document* reused=xideDocumentNew(&app.documents,"c",&error);
 CHECK(reused==c && reused->id!=oldId && xideEditorShow(&app,reused)==XUI_OK && idle(&app,700) && !v->ready);
 size_t bytes=0; unsigned char* original=xrtFileReadAllLimit(entry,1024,&bytes); CHECK(original && bytes==strlen(saved) && !memcmp(original,saved,bytes)); xrtFree(original);
 original=xrtFileReadAllLimit(header,1024,&bytes); CHECK(original && bytes==strlen(types) && !memcmp(original,types,bytes)); xrtFree(original);
 original=xrtFileReadAllLimit(chainLeaf,1024,&bytes);CHECK(original && bytes==strlen("#define FIELD_TYPE int\n") && !memcmp(original,"#define FIELD_TYPE int\n",bytes));xrtFree(original);
 printf("c_language_view_test passed: %d native assertions; real clangd diagnostics, C/header unsaved types, Unicode, problem navigation, F12 open/disk targets, outline Enter, multiline pointer/keyboard hover, malformed real replies, request capacity/cancellation, real server exit/restart, C/xlang dock and problem ownership, cancellation/restart, explicit disk identity invalidation and slot reuse; renders in %s.\n",f->checks,root);
cleanup:
 xrtFree(savedManifest); xrtValueRelease(validHover); xrtValueRelease(validSymbols); xrtValueRelease(validDefinition); xrtValueRelease(fake); if(began) xgeEnd(); xideAppDestroy(&app); xideProjectClear(&project); free(root); free(entry); free(header); free(disk); free(xlpath); free(manifest); free(light); free(dark); free(settings);free(chainLeaf);free(chainImage); xgeQuit(); return XGE_OK;
}
static DWORD WINAPI worker(void* data) { fixture* f=data; if(!SetThreadDesktop(f->desktop)) return 1; xge_desc_t d={0}; d.iWidth=1600; d.iHeight=1000; d.sTitle="C semantic verification"; d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; d.iRunMode=XGE_RUN_GAME_LOOP; int r=xgeInit(&d); if(r==XGE_OK) r=xgeRun(verify,f); xgeUnit(); return r==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv) { if(argc!=3 || !xrtDirCreateAll(argv[1])) return 2; fixture f={.directory=argv[1],.server=argv[2]}; wchar_t name[100]; swprintf(name,100,L"CSemanticVerification-%lu",GetCurrentProcessId()); f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) return 1; HANDLE t=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1; if(t) { if(WaitForSingleObject(t,240000)!=WAIT_OBJECT_0) { fputs("c_language_view_test timed out\n",stderr); ExitProcess(1); } GetExitCodeThread(t,&code); CloseHandle(t); } CloseDesktop(f.desktop); return (int)code; }
