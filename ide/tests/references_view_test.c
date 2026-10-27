#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "ui/task_view.h"
#include "ui/workspace_view.h"
#include "services/file_sync.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Inspect acceptance decisions;positive results/jumps use the real server. */
#include "../src/ui/language_view.c"
typedef struct fixture {const char *directory,*compiler;HDESK desktop;int started,failed,checks,legacy,standalone,owners;} fixture;
#define CHECK(v) do{++f->checks;if(!(v)){fprintf(stderr,"references_view_test:%d:%s; %s; %s; %s\n",__LINE__,#v,error.message,xideLanguageViewMessage(&app),app.languageView && app.languageView->references ? app.languageView->references->message : "");f->failed=1;goto cleanup;}}while(0)
static int writeText(const char* path,const char* text){return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)});}
static int pump(xide_app* app)
{if(xuiDispatchPendingEvents(app->context)!=XUI_OK)return 0;xideEditorPoll(app);xideFileSyncPoll(app);xideTaskViewPoll(app);xideLanguageViewPoll(app);xideWorkbenchLayout(app);return xuiLayout(app->context)==XUI_OK && !app->frameError;}
static int waitReferences(xide_app* app)
{uint64_t end=GetTickCount64()+20000;do{if(!pump(app))return 0;struct xide_reference_view* r=app->languageView->references;if(!r->pending && !r->request && !r->decoding)return 1;Sleep(2);}while(GetTickCount64()<end);return 0;}
static int key(xide_app* app,int code,uint32_t modifiers)
{return xuiInputKeyDownEx(app->context,code,modifiers,NULL)==XUI_OK && xuiInputKeyUpEx(app->context,code,modifiers,NULL)==XUI_OK && pump(app);}
static int gotoSymbol(xide_app* app,xide_document* doc,const char* marker)
{const char* text=xuiCodeEditGetText(doc->editor);const char* at=strstr(text,marker);return at && xideEditorShow(app,doc)==XUI_OK && xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(doc->editor),xuiCodeEditGetDocument(doc->editor),(int)(at-text),0)==XUI_OK && xuiSetFocusWidget(app->context,doc->editor)==XUI_OK;}
static int click(xide_app* app,xui_widget widget)
{if(!pump(app))return 0;xui_rect_t rect=xuiWidgetGetWorldRect(widget);float x=rect.fX+rect.fW/2,y=rect.fY+rect.fH/2;return rect.fW>0 && rect.fH>0 && xuiInputPointerMove(app->context,x,y,0)==XUI_OK && xuiInputPointerDown(app->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(app->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && pump(app);}
static int capture(xide_app* app,const char* path)
{if(!pump(app) || xuiUpdate(app->context,0)!=XUI_OK)return 0;xui_rect_i_t rect={0,0,app->viewportWidth,app->viewportHeight};return app->proxy.surfaceClear(&app->proxy,app->target,0xffeeeeeeu)==XUI_OK && xuiRender(app->context,app->target,&rect,1)==XUI_OK && xideAppCapture(app,path)==XGE_OK;}
static size_t pathRow(struct xide_reference_view* r,const char* path)
{for(size_t i=0;r->result && i<r->result->count;++i)if(xidePathLexicalEqual(r->result->items[i].path,path))return i;return SIZE_MAX;}
static int enterRow(xide_app* app,size_t row)
{struct xide_reference_view* r=app->languageView->references;r->page=(row/REFERENCE_PAGE)*REFERENCE_PAGE;referenceRows(app->languageView);return xuiTreeViewSetSelected(r->tree,(int)(row-r->page)+1)==XUI_OK && xuiSetFocusWidget(app->context,r->tree)==XUI_OK && key(app,XUI_KEY_ENTER,0) && waitReferences(app);}
static int selectedStart(xide_document* d,xide_lsp_position expected)
{xui_code_selection_t caret={0};int line=0,character=0;return xuiCodeSelectionGetState(xuiCodeEditGetSelection(d->editor),&caret)==XUI_OK && xideCodeDocumentOffsetToLsp(xuiCodeEditGetDocument(d->editor),caret.iCaretOffset,&line,&character) && line==expected.line && character==expected.character;}
static int verify(void* user)
{
 fixture* f=user;if(f->started)return XGE_OK;f->started=1;xide_app app={0};xide_model_error error={0};int began=0;char *root=NULL,*entry=NULL,*other=NULL,*shared=NULL,*native=NULL,*manifest=NULL,*image=NULL,*dark=NULL,*large=NULL;xvalue* catalogProof=NULL;size_t row=0;
 char unique[80];snprintf(unique,sizeof(unique),"references-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64());root=xidePathResolve(f->directory,unique);entry=xidePathResolve(root,"入口😀.xl");other=xidePathResolve(root,"other.xl");shared=xidePathResolve(root,"shared.xl");native=xidePathResolve(root,"native.c");manifest=xidePathResolve(root,"refs.xlp");image=xidePathResolve(root,"references-light.png");dark=xidePathResolve(root,"references-dark.png");
 const char* mainText="\xef\xbb\xbf" "import shared\r\nfunc main(...) as int\r\n string 中文 = \"😀\"; return shared.compute(7)\r\nend\r\n";const char* otherText="import shared\r\nfunc main(...) as int\r\n return shared.compute(8)+shared.compute(9)\r\nend\r\n";const char* shifted="\nimport shared\r\nfunc main(...) as int\r\n return shared.compute(8)+shared.compute(9)\r\nend\r\n";const char* body="export func compute(int value) as int\r\n return value+1\r\nend\r\n";
 if(f->owners){
  mainText="\xef\xbb\xbf" "import shared\r\nfunc main(...) as int\r\n string 中文 = \"😀\"; return shared.main(7)\r\nend\r\n";
  otherText="import shared\r\nfunc main(...) as int\r\n return shared.main(8)+shared.main(9)\r\nend\r\n";
  body="export func main(int value) as int\r\n return value+1\r\nend\r\n";
 }
 const char* project="{\"format\":\"xlang-project-1\",\"entry\":\"入口😀.xl\",\"ide\":{\"name\":\"引用验证\",\"files\":[{\"id\":\"other\",\"path\":\"other.xl\",\"kind\":\"xlang\"},{\"id\":\"shared\",\"path\":\"shared.xl\",\"kind\":\"xlang\"}],\"activeConfiguration\":\"debug\",\"configurations\":[{\"id\":\"debug\",\"output\":\"build/app.exe\"},{\"id\":\"release\",\"output\":\"build/release.exe\"}]}}";
 CHECK(root && entry && other && shared && native && manifest && image && dark && xrtDirCreateAll(root));CHECK(writeText(entry,mainText) && writeText(other,otherText) && writeText(shared,body) && writeText(native,"int main(void){return 0;}\n") && writeText(manifest,project));app.hidden=1;app.noSession=1;app.compilerPath=f->compiler;CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK);began=1;CHECK(xideActionCount()==75 && xideActionNamed("language.findReferences")->id==XIDE_ACTION_FIND_REFERENCES && !xideLanguageViewCanFindReferences(&app));
 if(!f->legacy && !f->standalone){CHECK(xideWorkspaceCreate(&app.workspaceModel,"引用验证",&error) && xideWorkspaceAddProject(&app.workspaceModel,manifest,NULL,&error) && xideWorkspaceViewRefresh(&app)==XUI_OK && xideWorkspaceViewSelectProject(&app,app.workspaceModel.projects[0].id));}CHECK(xideEditorOpen(&app,entry,&error) && xideEditorOpen(&app,native,&error) && pump(&app));xide_document* source=xideDocumentsFind(&app.documents,entry);CHECK(source && !xideLanguageViewCanFindReferences(&app));CHECK(xuiCodeDocumentReplace(xuiCodeEditGetDocument(source->editor),0,xuiCodeDocumentGetLength(xuiCodeEditGetDocument(source->editor)),mainText)==XUI_OK && gotoSymbol(&app,source,f->owners?"main(7)":"compute(7)"));
 CHECK(xideLanguageViewCanFindReferences(&app) && key(&app,XUI_KEY_F12,XUI_MOD_SHIFT) && waitReferences(&app));struct xide_reference_view* r=app.languageView->references;
 if(f->legacy){CHECK(!xideLanguageViewReferenceCount(&app) && !r->pending && !r->request && !r->decoding && !xideLanguageViewCanFindReferences(&app));printf("references_view_test passed: %d checks;old0.4 service declines references/proof capability without a query or stale jump\n",f->checks);goto cleanup;}
 if(f->standalone){CHECK(r->usable && xideLanguageViewReferenceCount(&app)==2 && xideReferenceDecoderReceived(r->decoder)>0 && !xideDocumentsFind(&app.documents,other));row=pathRow(r,shared);CHECK(row!=SIZE_MAX && enterRow(&app,row) && xidePathLexicalEqual(app.activeDocument->path,shared) && r->usable);row=pathRow(r,entry);CHECK(row!=SIZE_MAX && enterRow(&app,row) && app.activeDocument==source);CHECK(xuiCheckBoxSetChecked(r->include,0)==XUI_OK && click(&app,r->refresh) && waitReferences(&app) && xideLanguageViewReferenceCount(&app)==1 && capture(&app,image));printf("references_view_test passed: %d checks;standalone file/import closure without workspace or project,real guarded consecutive jumps and declaration exclusion;render=%s\n",f->checks,image);goto cleanup;}
 if(f->owners){
  CHECK(r->usable && xideLanguageViewReferenceCount(&app)==4 && !xideDocumentsFind(&app.documents,shared));
  CHECK(xideEditorOpen(&app,shared,&error));xide_document* library=xideDocumentsFind(&app.documents,shared);CHECK(library && gotoSymbol(&app,library,"main(int"));
  CHECK(key(&app,XUI_KEY_F12,XUI_MOD_SHIFT) && waitReferences(&app) && r->usable && xideLanguageViewReferenceCount(&app)==4);
  CHECK(pathRow(r,entry)!=SIZE_MAX && pathRow(r,other)!=SIZE_MAX && pathRow(r,shared)!=SIZE_MAX && capture(&app,image));
  CHECK(xuiCheckBoxSetChecked(r->include,0)==XUI_OK && click(&app,r->refresh) && waitReferences(&app) && xideLanguageViewReferenceCount(&app)==3);
  CHECK(xuiCheckBoxSetChecked(r->include,1)==XUI_OK && click(&app,r->refresh) && waitReferences(&app) && xideLanguageViewReferenceCount(&app)==4);
  row=pathRow(r,other);CHECK(row!=SIZE_MAX);xide_lsp_position target=r->result->items[row].start;
  CHECK(enterRow(&app,row) && xidePathLexicalEqual(app.activeDocument->path,other) && selectedStart(app.activeDocument,target) && r->usable);
  CHECK(xideWorkspaceViewSelectConfiguration(&app,"release",&error) && !xideLanguageViewReferenceCount(&app));
  CHECK(gotoSymbol(&app,library,"main(int") && xideActionExecute(&app,XIDE_ACTION_FIND_REFERENCES)==XIDE_ACTION_DISPATCHED && waitReferences(&app) && r->usable && xideLanguageViewReferenceCount(&app)==4);
  CHECK(xideActionExecute(&app,XIDE_ACTION_THEME_GRAPHITE)==XIDE_ACTION_DISPATCHED && capture(&app,dark));
  printf("references_view_test passed: %d checks;real project library export-main origin,actual primary/importer source-owned references,include toggle,guarded unopened jump,configuration invalidation,C/xlang tabs and light/dark center-dock renders;render=%s\n",f->checks,image);goto cleanup;
 }
 CHECK(r->usable && xideLanguageViewReferenceCount(&app)==4 && xideReferenceDecoderReceived(r->decoder)>0 && !xideDocumentsFind(&app.documents,other) && !xideDocumentsFind(&app.documents,shared));catalogProof=xrtValueRetain(app.languageView->analysisCatalog);int oldId=app.languageView->nextId;CHECK(catalogProof && xideWorkspaceViewRefresh(&app)==XUI_OK && app.languageView->analysisCatalog==catalogProof && app.languageView->nextId>=oldId && r->usable);xui_dock_window_info_t dock={.iSize=sizeof(dock)};CHECK(xuiDockPanelGetWindowInfo(app.dock,app.windows[XIDE_USAGES],&dock)==XUI_OK && dock.iRegion==XUI_DOCK_PANEL_REGION_DOCUMENT && dock.iPane==app.helpPane);CHECK(capture(&app,image));
 CHECK(xuiCheckBoxSetChecked(r->include,0)==XUI_OK && click(&app,r->refresh) && waitReferences(&app) && xideLanguageViewReferenceCount(&app)==3);CHECK(xuiCheckBoxSetChecked(r->include,1)==XUI_OK && click(&app,r->refresh) && waitReferences(&app) && xideLanguageViewReferenceCount(&app)==4);
 row=pathRow(r,other);CHECK(row!=SIZE_MAX);CHECK(writeText(other,shifted) && enterRow(&app,row) && !xideDocumentsFind(&app.documents,other) && app.activeDocument==source && r->usable && strstr(r->message,"位置已改变"));row=pathRow(r,other);CHECK(row!=SIZE_MAX && r->result->items[row].start.line==3);xide_lsp_position target=r->result->items[row].start;CHECK(enterRow(&app,row) && app.activeDocument!=source && xidePathLexicalEqual(app.activeDocument->path,other) && selectedStart(app.activeDocument,target) && r->usable);
 row=pathRow(r,shared);CHECK(row!=SIZE_MAX);target=r->result->items[row].start;CHECK(enterRow(&app,row) && xidePathLexicalEqual(app.activeDocument->path,shared) && selectedStart(app.activeDocument,target) && r->usable && xideLanguageViewReferenceCount(&app)==4);row=pathRow(r,entry);CHECK(row!=SIZE_MAX);target=r->result->items[row].start;CHECK(enterRow(&app,row) && app.activeDocument==source && selectedStart(source,target));
 CHECK(xideActionExecute(&app,XIDE_ACTION_THEME_GRAPHITE)==XIDE_ACTION_DISPATCHED && capture(&app,dark));CHECK(xideWorkspaceViewSelectConfiguration(&app,"release",&error) && !xideLanguageViewReferenceCount(&app));CHECK(gotoSymbol(&app,source,"compute(7)") && xideActionExecute(&app,XIDE_ACTION_FIND_REFERENCES)==XIDE_ACTION_DISPATCHED && waitReferences(&app) && xideLanguageViewReferenceCount(&app)==4);
 CHECK(xideActionExecute(&app,XIDE_ACTION_FIND_REFERENCES)==XIDE_ACTION_DISPATCHED && referencesBusy(app.languageView));CHECK(xuiSetFocusWidget(app.context,r->tree)==XUI_OK && key(&app,XUI_KEY_ESCAPE,0) && waitReferences(&app) && !xideLanguageViewReferenceCount(&app));
 large=malloc(65536);CHECK(large);size_t used=(size_t)snprintf(large,65536,"import shared\nfunc main(...) as int\n int total=0\n");for(int i=0;i<220;++i)used+=(size_t)snprintf(large+used,65536-used," total=total+shared.compute(%d)\n",i);snprintf(large+used,65536-used," return total\nend\n");CHECK(xuiCodeDocumentReplace(xuiCodeEditGetDocument(source->editor),0,xuiCodeDocumentGetLength(xuiCodeEditGetDocument(source->editor)),large)==XUI_OK && gotoSymbol(&app,source,"compute(0)") && pump(&app));CHECK(!xideLanguageViewReferenceCount(&app) && xideActionExecute(&app,XIDE_ACTION_FIND_REFERENCES)==XIDE_ACTION_DISPATCHED && waitReferences(&app) && xideLanguageViewReferenceCount(&app)==223);CHECK(r->page==0 && click(&app,r->next) && r->page==200 && click(&app,r->previous) && r->page==0);CHECK(click(&app,r->clear) && !xideLanguageViewReferenceCount(&app));
 CHECK(gotoSymbol(&app,source,"compute(0)") && xideActionExecute(&app,XIDE_ACTION_FIND_REFERENCES)==XIDE_ACTION_DISPATCHED);for(unsigned i=0;i<3000 && !r->request;++i){CHECK(pump(&app));Sleep(1);}CHECK(r->request);xvalue* malformed=xrtJsonParse(xrtStrView("{\"jsonrpc\":\"2.0\",\"result\":[null]}"));CHECK(malformed && put(malformed,"id",xrtValueInt(r->request)) && takeReferences(app.languageView,malformed) && waitReferences(&app) && !xideLanguageViewReferenceCount(&app) && app.activeDocument==source);
 CHECK(gotoSymbol(&app,source,"compute(0)") && xideActionExecute(&app,XIDE_ACTION_FIND_REFERENCES)==XIDE_ACTION_DISPATCHED && waitReferences(&app) && xideLanguageViewReferenceCount(&app)==223);CHECK(xideActionExecute(&app,XIDE_ACTION_CANCEL_CHECK)==XIDE_ACTION_DISPATCHED && !xideLanguageViewReferenceCount(&app));
 {xide_text_file file={0};CHECK(xideTextFileRead(entry,&file,&error) && file.rawLength==strlen(mainText) && !memcmp(file.raw,mainText,strlen(mainText)));xideTextFileClear(&file);CHECK(xideTextFileRead(other,&file,&error) && file.rawLength==strlen(shifted) && !memcmp(file.raw,shifted,strlen(shifted)));xideTextFileClear(&file);CHECK(xideTextFileRead(shared,&file,&error) && file.rawLength==strlen(body) && !memcmp(file.raw,body,strlen(body)));xideTextFileClear(&file);}
 printf("references_view_test passed: %d checks;cold ShiftF12/configured unopened-unimported member,proof/BOM/CRLF/Unicode,central bottom pane,includeDeclaration,stale disk position requery,continuous Enter jumps,themes/configuration/cancel/edit,223rows pagination,malformed response/recovery and original files;render=%s and %s\n",f->checks,image,dark);
cleanup:
 if(began)xgeEnd();
 xideAppDestroy(&app);xrtValueRelease(catalogProof);free(root);free(entry);free(other);free(shared);free(native);free(manifest);free(image);free(dark);free(large);xgeQuit();return XGE_OK;
}
static DWORD WINAPI worker(void* data)
{fixture* f=data;if(!SetThreadDesktop(f->desktop))return 1;xge_desc_t d={0};d.iWidth=1600;d.iHeight=1000;d.sTitle="xlang references verification";d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI;d.iRunMode=XGE_RUN_GAME_LOOP;int result=xgeInit(&d);if(result==XGE_OK)result=xgeRun(verify,f);xgeUnit();return result==XGE_OK && f->started && !f->failed ? 0 : 1;}
int main(int argc,char** argv)
{if((argc!=3 && argc!=4) || (argc==4 && strcmp(argv[3],"legacy") && strcmp(argv[3],"standalone") && strcmp(argv[3],"owners")) || !xrtDirCreateAll(argv[1]))return 2;fixture f={.directory=argv[1],.compiler=argv[2],.legacy=argc==4 && !strcmp(argv[3],"legacy"),.standalone=argc==4 && !strcmp(argv[3],"standalone"),.owners=argc==4 && !strcmp(argv[3],"owners")};wchar_t name[80];swprintf(name,80,L"xlang-refs-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64());f.desktop=CreateDesktopW(name,NULL,NULL,0,GENERIC_ALL,NULL);if(!f.desktop)return 2;HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL);DWORD code=2;if(thread){WaitForSingleObject(thread,INFINITE);GetExitCodeThread(thread,&code);CloseHandle(thread);}CloseDesktop(f.desktop);return (int)code;}
