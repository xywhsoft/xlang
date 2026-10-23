#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "ui/task_view.h"
#include "services/file_sync.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Real view included only to assert late-response acceptance decisions. */
#include "../src/ui/language_view.c"
typedef struct fixture { const char *directory,*compiler; HDESK desktop; int started,failed,typeFacts,legacy; } fixture;
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"definition_view_test:%d: %s; %s; %s\n",__LINE__,#v,error.message,xideLanguageViewMessage(&app)); f->failed=1; goto cleanup; } } while(0)
static int writeText(const char* path,const char* text) { return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)}); }
static int settle(xide_app* app) { if(xuiDispatchPendingEvents(app->context)!=XUI_OK) return 0; xideEditorPoll(app); xideFileSyncPoll(app); xideTaskViewPoll(app); xideLanguageViewPoll(app); xideWorkbenchLayout(app); return xuiLayout(app->context)==XUI_OK && !app->frameError; }
static int waitChecked(xide_app* a) { uint64_t until=GetTickCount64()+20000; do { if(!settle(a)) return 0; if(!a->languageView->busy && a->languageView->snapshotResult) return 1; Sleep(5); } while(GetTickCount64()<until); return 0; }
static int waitDefinition(xide_app* a) { uint64_t until=GetTickCount64()+5000; do { if(!settle(a)) return 0; if(!a->languageView->definitionRequest && !a->languageView->definitionPending) return 1; Sleep(5); } while(GetTickCount64()<until); return 0; }
static int waitReady(xide_app* a) { uint64_t until=GetTickCount64()+5000; do { if(!settle(a)) return 0; if(a->languageView->receiptReady && current(a->languageView)) return 1; Sleep(5); } while(GetTickCount64()<until); return 0; }
static int caret(xide_document* d) { xui_code_selection_t s={0}; return xuiCodeSelectionGetState(xuiCodeEditGetSelection(d->editor),&s)==XUI_OK ? s.iCaretOffset : -1; }
static int gotoOffset(xide_app* a,xide_document* d,int offset) { return xideEditorShow(a,d)==XUI_OK && xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(d->editor),xuiCodeEditGetDocument(d->editor),offset,0)==XUI_OK && xuiSetFocusWidget(a->context,d->editor)==XUI_OK; }
static int replace(xide_document* d,const char* text) { xui_code_document core=xuiCodeEditGetDocument(d->editor); return xuiCodeDocumentReplace(core,0,xuiCodeDocumentGetLength(core),text)==XUI_OK; }
static int menuDefinition(xide_app* a) { int i=-1; for(int j=0;j<xuiMenuGetItemCount(a->fileMenu);++j) if(xuiMenuGetItem(a->fileMenu,j)->iValue==XIDE_ACTION_DEFINITION) i=j; if(i<0 || xuiMenuOpenAt(a->fileMenu,a->root,80,20)!=XUI_OK || !settle(a)) return 0; xui_rect_t r=xuiMenuGetItemRect(a->fileMenu,i),w=xuiWidgetGetWorldRect(a->fileMenu); float x=w.fX+r.fX+r.fW/2,y=w.fY+r.fY+r.fH/2; return xuiInputPointerMove(a->context,x,y,0)==XUI_OK && xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(a); }
static int capture(xide_app* a,const char* path) { if(!settle(a) || xuiUpdate(a->context,0)!=XUI_OK) return 0; xui_rect_i_t r={0,0,a->viewportWidth,a->viewportHeight}; return a->proxy.surfaceClear(&a->proxy,a->target,0xffeeeeeeu)==XUI_OK && xuiRender(a->context,a->target,&r,1)==XUI_OK && xideAppCapture(a,path)==XGE_OK; }
static xvalue* location(const char* path,int sl,int sc,int el,int ec) { xvalue *out=xrtValueObject(),*range=xrtValueObject(),*start=xrtValueObject(),*end=xrtValueObject(); char* uri=xideLspPathToUri(path,NULL); put(start,"line",xrtValueInt(sl)); put(start,"character",xrtValueInt(sc)); put(end,"line",xrtValueInt(el)); put(end,"character",xrtValueInt(ec)); put(range,"start",start); put(range,"end",end); put(out,"range",range); put(out,"uri",xrtValueString(sv(uri))); free(uri); return out; }
/* Controlled representation for malformed/stale client validation cases;
   positive navigation below always consumes actual native server responses. */
static xvalue* proofLocation(xide_app* app,const char* path,const char* text,const char* raw,int sl,int sc,int el,int ec)
{
 xvalue *out=location(path,sl,sc,el,ec),*extension=xrtValueObject(),*doc=xrtValueObject();
 unsigned char digest[32]; char hex[65],formatted[32]; xfileinfo info={0}; document_binding* binding=inputPath(app->languageView,path);
 int okay=out && extension && doc && xrtSha256(text,strlen(text),digest) && put(extension,"schema",xrtValueInt(1));
 for(size_t i=0;okay && i<32;++i) snprintf(hex+2*i,3,"%02x",digest[i]);
 if(okay) okay=put(doc,"path",xrtValueString(sv(path))) && put(doc,"origin",xrtValueString(sv(raw ? "disk" : "overlay"))) && put(doc,"normalizedSize",xrtValueInt((int64)strlen(text))) && put(doc,"normalizedSha256",xrtValueString(sv(hex)));
 if(okay && !raw) okay=binding && put(doc,"version",xrtValueInt(binding->wireVersion));
 if(okay && raw) {
  xvalue *input=xrtValueObject(),*metadata=xrtValueObject(); okay=input && metadata && xideIoReceiptStat(path,&info) && xrtSha256(raw,strlen(raw),digest);
  for(size_t i=0;okay && i<32;++i) snprintf(hex+2*i,3,"%02x",digest[i]);
  if(okay) okay=put(input,"path",xrtValueString(sv(path))) && put(input,"origin",xrtValueString(sv("disk"))) && put(input,"rawSize",xrtValueInt((int64)strlen(raw))) && put(input,"rawSha256",xrtValueString(sv(hex))) && put(metadata,"available",xrtValueInt((int64)info.Available)) && put(metadata,"attributes",xrtValueInt((int64)info.Attributes));
  snprintf(formatted,sizeof(formatted),"%016llx",(unsigned long long)info.Device); if(okay) okay=put(metadata,"device",xrtValueString(sv(formatted)));
  snprintf(formatted,sizeof(formatted),"%016llx",(unsigned long long)info.Identity); if(okay) okay=put(metadata,"identity",xrtValueString(sv(formatted)));
  snprintf(formatted,sizeof(formatted),"%lld",(long long)info.Modified); if(okay) okay=put(metadata,"modifiedUs",xrtValueString(sv(formatted)));
  snprintf(formatted,sizeof(formatted),"%lld",(long long)info.Changed); if(okay) okay=put(metadata,"changedUs",xrtValueString(sv(formatted)));
  if(okay) { okay=put(input,"metadata",metadata); metadata=NULL; }
  if(okay) { okay=put(doc,"input",input); input=NULL; }
  xrtValueRelease(input); xrtValueRelease(metadata);
 }
 if(okay) { okay=put(extension,"document",doc); doc=NULL; }
 if(okay) { okay=put(out,"xlang",extension); extension=NULL; }
 xrtValueRelease(doc); xrtValueRelease(extension); if(!okay) { xrtValueRelease(out); out=NULL; } return out;
}
static int verify(void* user)
{
 fixture* f=user; if(f->started) return XGE_OK; f->started=1; xide_app app={0}; xide_model_error error={0}; int began=0; char *root=NULL,*entry=NULL,*dep=NULL,*native=NULL,*light=NULL,*dark=NULL,*diskPath=NULL,*diskImage=NULL,*coldImage=NULL; xvalue* response=NULL;
 char unique[60]; snprintf(unique,sizeof(unique),"def-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64()); root=xidePathResolve(f->directory,unique); entry=xidePathResolve(root,"入口😀.xl"); dep=xidePathResolve(root,"helper.xl"); native=xidePathResolve(root,"helper.c"); light=xidePathResolve(root,"definition-overload-light.png"); dark=xidePathResolve(root,"definition-dependency-dark.png"); diskPath=xidePathResolve(root,"disk.xl"); diskImage=xidePathResolve(root,"definition-disk.png");
 const char *saved="func main(...) as int\n    return 0\nend\n",*overload="func pick(int value) as int\n    return value\nend\nfunc pick(string value) as int\n    return 2\nend\nfunc main(...) as int\n    string 中文 = \"😀\"; auto second = pick(\"x\")\n    return second - 2\nend\n",*helper="export func answer() as int\n    return 42\nend\n",*overlay="\n\nexport func answer() as int\n    return 42\nend\n",*imported="import helper\nfunc main(...) as int\n    return helper.answer() - 42\nend\n";
 CHECK(root && entry && dep && native && light && dark && diskPath && diskImage && xrtDirCreateAll(root) && writeText(entry,saved) && writeText(dep,helper) && writeText(native,"int helper(void) { return 1; }\n") && writeText(diskPath,helper));
 app.hidden=1; app.noSession=1; app.compilerPath=f->compiler; CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1;
 CHECK(xideActionCount()==73 && xideActionNamed("language.definition")->id==69 && !xideActionQuery(&app,XIDE_ACTION_DEFINITION).enabled);
 CHECK(xideEditorOpen(&app,entry,&error) && xideEditorOpen(&app,dep,&error) && xideEditorOpen(&app,native,&error) && settle(&app)); xide_document *xl=xideDocumentsFind(&app.documents,entry),*dependency=xideDocumentsFind(&app.documents,dep),*c=app.activeDocument;
 CHECK(!strcmp(c->language,"c") && !xideActionQuery(&app,XIDE_ACTION_DEFINITION).enabled && !strcmp(xl->language,"xlang"));
 if(!f->legacy) {
  uint64_t closedId=xl->id;
  CHECK(gotoOffset(&app,xl,0) && requestDefinition(&app) && app.languageView->waitingInitialize && xideEditorClose(&app,xl,1,&error) && settle(&app) && !app.languageView->definitionPending && !app.languageView->definitionRequest);
  CHECK(xideEditorOpen(&app,entry,&error)); xl=app.activeDocument; CHECK(xl->id!=closedId && !strcmp(xl->language,"xlang"));
  int coldCall=(int)(strstr(overload,"pick(\"x\")")-overload),coldDefinition=(int)(strstr(overload,"pick(string")-overload);
  CHECK(replace(xl,overload) && gotoOffset(&app,xl,coldCall) && xideActionQuery(&app,XIDE_ACTION_DEFINITION).enabled && xuiInputKeyDown(app.context,XUI_KEY_F12,0)==XUI_OK && xuiInputKeyUp(app.context,XUI_KEY_F12,0)==XUI_OK && waitDefinition(&app));
  CHECK(app.activeDocument==xl && caret(xl)==coldDefinition && app.languageView->onDemandMode && app.languageView->definitionProofSupported && !app.languageView->lastSnapshotId && !app.languageView->snapshotResult);
  CHECK(replace(xl,imported) && replace(dependency,overlay) && gotoOffset(&app,xl,(coldCall=(int)(strstr(imported,"answer")-imported))) && requestDefinition(&app) && waitDefinition(&app) && app.activeDocument==dependency && caret(dependency)==(int)(strstr(overlay,"answer")-overlay));
  CHECK(gotoOffset(&app,xl,coldCall) && requestDefinition(&app) && gotoOffset(&app,xl,0) && waitDefinition(&app) && caret(xl)==0);
  CHECK(gotoOffset(&app,xl,coldCall) && requestDefinition(&app) && xideEditorShow(&app,c)==XUI_OK && waitDefinition(&app) && app.activeDocument==c);
  CHECK(gotoOffset(&app,xl,coldCall) && requestDefinition(&app)); response=location(dep,2,12,2,18); CHECK(response); applyDefinition(&app,response,NULL); xrtValueRelease(response); response=NULL; CHECK(app.activeDocument==xl && caret(xl)==coldCall);
  CHECK(requestDefinition(&app)); response=proofLocation(&app,dep,overlay,NULL,2,12,2,99999); CHECK(response); applyDefinition(&app,response,NULL); xrtValueRelease(response); response=NULL; CHECK(app.activeDocument==xl && caret(xl)==coldCall);
  CHECK(requestDefinition(&app)); response=proofLocation(&app,dep,overlay,NULL,2,12,2,18); CHECK(response && put(get(get(response,"xlang"),"document"),"normalizedSha256",xrtValueString(sv("0000000000000000000000000000000000000000000000000000000000000000")))); applyDefinition(&app,response,NULL); xrtValueRelease(response); response=NULL; CHECK(app.activeDocument==xl && caret(xl)==coldCall);
  CHECK(requestDefinition(&app)); response=proofLocation(&app,dep,overlay,NULL,2,12,2,18); document_binding* target=inputPath(app.languageView,dep); CHECK(response && target && put(get(get(response,"xlang"),"document"),"version",xrtValueInt(target->wireVersion+1))); applyDefinition(&app,response,NULL); xrtValueRelease(response); response=NULL; CHECK(app.activeDocument==xl && caret(xl)==coldCall);
  CHECK(requestDefinition(&app) && xideActionExecute(&app,XIDE_ACTION_CANCEL_CHECK)==XIDE_ACTION_DISPATCHED && waitDefinition(&app) && app.activeDocument==xl && caret(xl)==coldCall && !app.languageView->definitionPending);
  const char* coldDiskMain="import disk\nfunc main(...) as int\n return disk.answer() - 42\nend\n";
  const char* rawDisk="\xef\xbb\xbf" "export func answer() as int\r\n    return 42\r\nend\r\n";
  CHECK(writeText(diskPath,rawDisk) && replace(xl,coldDiskMain) && gotoOffset(&app,xl,(coldCall=(int)(strstr(coldDiskMain,"answer")-coldDiskMain))) && requestDefinition(&app) && waitDefinition(&app));
  coldImage=xidePathResolve(root,"definition-cold-disk.png");
  CHECK(coldImage && app.activeDocument->path && xidePathLexicalEqual(app.activeDocument->path,diskPath) && caret(app.activeDocument)==(int)(strstr(helper,"answer")-helper) && !app.languageView->lastSnapshotId && !app.languageView->snapshotResult && capture(&app,coldImage));
  xide_document* disk=app.activeDocument; CHECK(xideEditorClose(&app,disk,0,&error) && gotoOffset(&app,xl,coldCall) && prepareSymbols(&app) && waitReady(&app));
  response=proofLocation(&app,diskPath,helper,rawDisk,0,12,0,18); CHECK(response && requestDefinition(&app) && writeText(diskPath,rawDisk)); applyDefinition(&app,response,NULL); xrtValueRelease(response); response=NULL;
  CHECK(app.activeDocument==xl && caret(xl)==coldCall && !xideDocumentsFind(&app.documents,diskPath));
  const char* failedSource="func main(...) as int\n    return \"bad\"\nend\n";
  CHECK(replace(xl,failedSource) && gotoOffset(&app,xl,5) && requestDefinition(&app) && waitDefinition(&app) && app.activeDocument==xl && caret(xl)==5 && app.languageView->onDemandMode && !app.languageView->snapshotResult);
  CHECK(replace(xl,overload) && gotoOffset(&app,xl,(int)(strstr(overload,"pick(\"x\")")-overload)) && requestDefinition(&app) && waitDefinition(&app) && app.activeDocument==xl && caret(xl)==coldDefinition);
  CHECK(writeText(diskPath,helper));
 } else {
  CHECK(replace(xl,overload) && gotoOffset(&app,xl,(int)(strstr(overload,"pick(\"x\")")-overload)) && requestDefinition(&app) && waitDefinition(&app) && !app.languageView->definitionProofSupported && !app.languageView->snapshotResult && !app.languageView->definitionRequest && !app.languageView->definitionPending);
 }
 /* The cold F12/dependency/disk queries above exercise default automatic mode.
    The remaining oracle inspects the explicit manual batch representation. */
 if(!f->legacy) { CHECK(app.languageView->autoEnabled); app.disableAutomaticDiagnostics=1; reset(&app); automaticDestroy(app.languageView); }
 CHECK(replace(xl,overload) && xideEditorShow(&app,xl)==XUI_OK && xideActionExecute(&app,XIDE_ACTION_CHECK_CURRENT)==XIDE_ACTION_DISPATCHED && waitChecked(&app) && xideActionQuery(&app,XIDE_ACTION_DEFINITION).enabled);
 int call=(int)(strstr(overload,"pick(\"x\")")-overload),definition=(int)(strstr(overload,"pick(string")-overload);
 CHECK(gotoOffset(&app,xl,call) && xuiInputKeyDown(app.context,XUI_KEY_F12,0)==XUI_OK && xuiInputKeyUp(app.context,XUI_KEY_F12,0)==XUI_OK && waitDefinition(&app) && app.activeDocument==xl && caret(xl)==definition && capture(&app,light));
 /* Caret movement, active-tab change and document edits discard late replies. */
 CHECK(gotoOffset(&app,xl,call) && xideActionExecute(&app,XIDE_ACTION_DEFINITION)==XIDE_ACTION_DISPATCHED && gotoOffset(&app,xl,0) && waitDefinition(&app) && caret(xl)==0);
 CHECK(gotoOffset(&app,xl,call) && xideActionExecute(&app,XIDE_ACTION_DEFINITION)==XIDE_ACTION_DISPATCHED && xideEditorShow(&app,c)==XUI_OK && waitDefinition(&app) && app.activeDocument==c && caret(xl)==call);
 CHECK(gotoOffset(&app,xl,call) && xideActionExecute(&app,XIDE_ACTION_DEFINITION)==XIDE_ACTION_DISPATCHED); response=location(entry,3,5,3,99999); applyDefinition(&app,response,NULL); xrtValueRelease(response); response=NULL; CHECK(caret(xl)==call);
 CHECK(waitDefinition(&app) && replace(xl,imported) && replace(dependency,overlay) && waitChecked(&app) && xideActionQuery(&app,XIDE_ACTION_DEFINITION).enabled);
 call=(int)(strstr(imported,"answer")-imported); definition=(int)(strstr(overlay,"answer")-overlay);
 CHECK(gotoOffset(&app,xl,call) && menuDefinition(&app) && waitDefinition(&app) && app.activeDocument==dependency && caret(dependency)==definition && xl->dirty && dependency->dirty && !strcmp(xuiCodeEditGetText(dependency->editor),overlay));
 CHECK(xideActionExecute(&app,XIDE_ACTION_THEME_GRAPHITE)==XIDE_ACTION_DISPATCHED && capture(&app,dark));
 /* TypeId navigation uses the dirty dependency's real type declaration,
    even when two aliases have the same canonical int representation. */
 const char* typedDependency="// 未保存类型 😀\n\nexport typedef Count = int\nexport typedef Other = int\nexport class Item\n    int value\nend\n";
 const char* typedEntry="import helper\nfunc sample(helper.Count amount, helper.Other second, helper.Item item) as helper.Count\n    return amount + second\nend\nfunc main(...) as int\n    return 0\nend\n";
 CHECK(replace(dependency,typedDependency) && replace(xl,typedEntry) && xideEditorShow(&app,xl)==XUI_OK && waitChecked(&app) && !xideLanguageViewDiagnosticCount(&app));
 call=(int)(strstr(typedEntry,"helper.Count")-typedEntry)+7; definition=(int)(strstr(typedDependency,"Count")-typedDependency);
 CHECK(gotoOffset(&app,xl,call) && xuiInputKeyDown(app.context,XUI_KEY_F12,0)==XUI_OK && xuiInputKeyUp(app.context,XUI_KEY_F12,0)==XUI_OK && waitDefinition(&app) && app.activeDocument==dependency && caret(dependency)==definition);
 char* typeImage=xidePathResolve(root,"definition-type-alias-dark.png"); CHECK(typeImage && capture(&app,typeImage)); free(typeImage);
 call=(int)(strstr(typedEntry,"helper.Other")-typedEntry)+7; definition=(int)(strstr(typedDependency,"Other")-typedDependency);
 CHECK(gotoOffset(&app,xl,call) && menuDefinition(&app) && waitDefinition(&app) && app.activeDocument==dependency && caret(dependency)==definition);
 call=(int)(strstr(typedEntry,"helper.Item")-typedEntry)+7; definition=(int)(strstr(typedDependency,"Item")-typedDependency);
 CHECK(gotoOffset(&app,xl,call) && xideActionExecute(&app,XIDE_ACTION_DEFINITION)==XIDE_ACTION_DISPATCHED && waitDefinition(&app) && app.activeDocument==dependency && caret(dependency)==definition && dependency->dirty && xl->dirty);
 CHECK(xideActionExecute(&app,XIDE_ACTION_THEME_CLASSIC)==XIDE_ACTION_DISPATCHED); typeImage=xidePathResolve(root,"definition-type-class-light.png"); CHECK(typeImage && capture(&app,typeImage)); free(typeImage);
 if(f->typeFacts) {
  const char* factsDependency="// 查询类型 😀\nexport protocol Named\n    func name() as string\nend\nexport class Base\nend\nexport class Child extends Base\nend\nexport class Box<T>\n    T value\nend\n";
  const char* factsEntry="import helper\nfunc sample(helper.Named named, helper.Child child, helper.Box<int> box) as int\n    return 0\nend\nfunc main(...) as int\n    return 0\nend\n";
  CHECK(replace(dependency,factsDependency) && replace(xl,factsEntry) && xideEditorShow(&app,xl)==XUI_OK && waitChecked(&app) && !xideLanguageViewDiagnosticCount(&app));
  call=(int)(strstr(factsEntry,"helper.Named")-factsEntry)+7; definition=(int)(strstr(factsDependency,"Named")-factsDependency);
  CHECK(gotoOffset(&app,xl,call) && xuiInputKeyDown(app.context,XUI_KEY_F12,0)==XUI_OK && xuiInputKeyUp(app.context,XUI_KEY_F12,0)==XUI_OK && waitDefinition(&app) && app.activeDocument==dependency && caret(dependency)==definition);
  CHECK(xideActionExecute(&app,XIDE_ACTION_THEME_GRAPHITE)==XIDE_ACTION_DISPATCHED); typeImage=xidePathResolve(root,"definition-protocol-type-dark.png"); CHECK(typeImage && capture(&app,typeImage)); free(typeImage);
  CHECK(waitChecked(&app)); call=(int)(strstr(factsDependency,"extends Base")-factsDependency)+8; definition=(int)(strstr(factsDependency,"Base")-factsDependency);
  CHECK(gotoOffset(&app,dependency,call) && menuDefinition(&app) && waitDefinition(&app) && app.activeDocument==dependency && caret(dependency)==definition);
  CHECK(xideActionExecute(&app,XIDE_ACTION_THEME_CLASSIC)==XIDE_ACTION_DISPATCHED); typeImage=xidePathResolve(root,"definition-base-type-light.png"); CHECK(typeImage && capture(&app,typeImage)); free(typeImage);
  call=(int)(strstr(factsDependency,"T value")-factsDependency); definition=(int)(strstr(factsDependency,"Box<T>")-factsDependency)+4;
  CHECK(gotoOffset(&app,dependency,call) && xideActionExecute(&app,XIDE_ACTION_DEFINITION)==XIDE_ACTION_DISPATCHED && waitDefinition(&app) && app.activeDocument==dependency && caret(dependency)==definition && dependency->dirty && xl->dirty);
  CHECK(xideActionExecute(&app,XIDE_ACTION_THEME_GRAPHITE)==XIDE_ACTION_DISPATCHED); typeImage=xidePathResolve(root,"definition-generic-parameter-dark.png"); CHECK(typeImage && capture(&app,typeImage)); free(typeImage);
 }
 CHECK(xideEditorShow(&app,xl)==XUI_OK && xideActionExecute(&app,XIDE_ACTION_CANCEL_CHECK)==XIDE_ACTION_DISPATCHED && xideActionQuery(&app,XIDE_ACTION_DEFINITION).enabled==app.languageView->definitionProofSupported);
 const char* diskMain="import disk\nfunc main(...) as int\n    return disk.answer() - 42\nend\n";
 CHECK(replace(xl,diskMain) && xideActionExecute(&app,XIDE_ACTION_CHECK_CURRENT)==XIDE_ACTION_DISPATCHED && waitChecked(&app)); call=(int)(strstr(diskMain,"answer")-diskMain);
 CHECK(!xideDocumentsFind(&app.documents,diskPath) && gotoOffset(&app,xl,call) && xideActionExecute(&app,XIDE_ACTION_DEFINITION)==XIDE_ACTION_DISPATCHED && waitDefinition(&app) && app.activeDocument->path && xidePathLexicalEqual(app.activeDocument->path,diskPath) && caret(app.activeDocument)==(int)(strstr(helper,"answer")-helper) && capture(&app,diskImage));
 CHECK(waitChecked(&app) && xideEditorShow(&app,xl)==XUI_OK && replace(xl,"func main(...) as int\n    return \"bad\"\nend\n") && settle(&app) && !xideActionQuery(&app,XIDE_ACTION_DEFINITION).enabled);
 CHECK(waitChecked(&app) && xideLanguageViewDiagnosticCount(&app)==1 && !xideActionQuery(&app,XIDE_ACTION_DEFINITION).enabled);
 CHECK(xideActionExecute(&app,XIDE_ACTION_CANCEL_CHECK)==XIDE_ACTION_DISPATCHED && xideEditorShow(&app,c)==XUI_OK && !xideActionQuery(&app,XIDE_ACTION_DEFINITION).enabled && !c->dirty);
 xide_text_file file={0}; CHECK(xideTextFileRead(entry,&file,&error) && !strcmp(file.text,saved)); xideTextFileClear(&file); CHECK(xideTextFileRead(dep,&file,&error) && !strcmp(file.text,helper)); xideTextFileClear(&file);
 printf("definition_view_test passed: cold F12=%d; real overload and TypeId aliases/class; new type facts=%d; initialization close/reuse, target proof and same-byte identity guards, current menu, Unicode and unsaved imported declaration, guarded BOM/CRLF disk navigation, stale response/cancel/source-error recovery and legacy fallback, original files preserved; renders in %s.\n",!f->legacy,f->typeFacts,root);
cleanup:
 xrtValueRelease(response); if(began) xgeEnd(); xideAppDestroy(&app); free(root); free(entry); free(dep); free(native); free(light); free(dark); free(diskPath); free(diskImage); free(coldImage); xgeQuit(); return XGE_OK;
}
static DWORD WINAPI worker(void* data) { fixture* f=data; if(!SetThreadDesktop(f->desktop)) return 1; xge_desc_t d={0}; d.iWidth=1600; d.iHeight=1000; d.sTitle="xlang definition verification"; d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; d.iRunMode=XGE_RUN_GAME_LOOP; int result=xgeInit(&d); if(result==XGE_OK) result=xgeRun(verify,f); xgeUnit(); return result==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv) { if((argc!=3 && argc!=4) || (argc==4 && strcmp(argv[3],"type-facts") && strcmp(argv[3],"legacy")) || !xrtDirCreateAll(argv[1])) return 2; fixture f={.directory=argv[1],.compiler=argv[2],.typeFacts=argc==4 && !strcmp(argv[3],"type-facts"),.legacy=argc==4 && !strcmp(argv[3],"legacy")}; wchar_t name[100]; swprintf(name,100,L"XlangDefinitionVerification-%lu",GetCurrentProcessId()); f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) return 1; HANDLE t=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1; if(t) { if(WaitForSingleObject(t,120000)!=WAIT_OBJECT_0) { fputs("definition_view_test timed out\n",stderr); ExitProcess(1); } GetExitCodeThread(t,&code); CloseHandle(t); } CloseDesktop(f.desktop); return (int)code; }
