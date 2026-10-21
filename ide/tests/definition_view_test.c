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
typedef struct fixture { const char *directory,*compiler; HDESK desktop; int started,failed,typeFacts; } fixture;
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"definition_view_test:%d: %s; %s; %s\n",__LINE__,#v,error.message,xideLanguageViewMessage(&app)); f->failed=1; goto cleanup; } } while(0)
static int writeText(const char* path,const char* text) { return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)}); }
static int settle(xide_app* app) { if(xuiDispatchPendingEvents(app->context)!=XUI_OK) return 0; xideEditorPoll(app); xideFileSyncPoll(app); xideTaskViewPoll(app); xideLanguageViewPoll(app); xideWorkbenchLayout(app); return xuiLayout(app->context)==XUI_OK && !app->frameError; }
static int waitChecked(xide_app* a) { uint64_t until=GetTickCount64()+20000; do { if(!settle(a)) return 0; if(!a->languageView->busy && a->languageView->snapshotResult) return 1; Sleep(5); } while(GetTickCount64()<until); return 0; }
static int waitDefinition(xide_app* a) { uint64_t until=GetTickCount64()+5000; do { if(!settle(a)) return 0; if(!a->languageView->definitionRequest) return 1; Sleep(5); } while(GetTickCount64()<until); return 0; }
static int caret(xide_document* d) { xui_code_selection_t s={0}; return xuiCodeSelectionGetState(xuiCodeEditGetSelection(d->editor),&s)==XUI_OK ? s.iCaretOffset : -1; }
static int gotoOffset(xide_app* a,xide_document* d,int offset) { return xideEditorShow(a,d)==XUI_OK && xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(d->editor),xuiCodeEditGetDocument(d->editor),offset,0)==XUI_OK && xuiSetFocusWidget(a->context,d->editor)==XUI_OK; }
static int replace(xide_document* d,const char* text) { xui_code_document core=xuiCodeEditGetDocument(d->editor); return xuiCodeDocumentReplace(core,0,xuiCodeDocumentGetLength(core),text)==XUI_OK; }
static int menuDefinition(xide_app* a) { int i=-1; for(int j=0;j<xuiMenuGetItemCount(a->fileMenu);++j) if(xuiMenuGetItem(a->fileMenu,j)->iValue==XIDE_ACTION_DEFINITION) i=j; if(i<0 || xuiMenuOpenAt(a->fileMenu,a->root,80,20)!=XUI_OK || !settle(a)) return 0; xui_rect_t r=xuiMenuGetItemRect(a->fileMenu,i),w=xuiWidgetGetWorldRect(a->fileMenu); float x=w.fX+r.fX+r.fW/2,y=w.fY+r.fY+r.fH/2; return xuiInputPointerMove(a->context,x,y,0)==XUI_OK && xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(a); }
static int capture(xide_app* a,const char* path) { if(!settle(a) || xuiUpdate(a->context,0)!=XUI_OK) return 0; xui_rect_i_t r={0,0,a->viewportWidth,a->viewportHeight}; return a->proxy.surfaceClear(&a->proxy,a->target,0xffeeeeeeu)==XUI_OK && xuiRender(a->context,a->target,&r,1)==XUI_OK && xideAppCapture(a,path)==XGE_OK; }
static xvalue* location(const char* path,int sl,int sc,int el,int ec) { xvalue *out=xrtValueObject(),*range=xrtValueObject(),*start=xrtValueObject(),*end=xrtValueObject(); char* uri=xideLspPathToUri(path,NULL); put(start,"line",xrtValueInt(sl)); put(start,"character",xrtValueInt(sc)); put(end,"line",xrtValueInt(el)); put(end,"character",xrtValueInt(ec)); put(range,"start",start); put(range,"end",end); put(out,"range",range); put(out,"uri",xrtValueString(sv(uri))); free(uri); return out; }
static int verify(void* user)
{
 fixture* f=user; if(f->started) return XGE_OK; f->started=1; xide_app app={0}; xide_model_error error={0}; int began=0; char *root=NULL,*entry=NULL,*dep=NULL,*native=NULL,*light=NULL,*dark=NULL,*diskPath=NULL,*diskImage=NULL; xvalue* response=NULL;
 char unique[60]; snprintf(unique,sizeof(unique),"def-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64()); root=xidePathResolve(f->directory,unique); entry=xidePathResolve(root,"入口😀.xl"); dep=xidePathResolve(root,"helper.xl"); native=xidePathResolve(root,"helper.c"); light=xidePathResolve(root,"definition-overload-light.png"); dark=xidePathResolve(root,"definition-dependency-dark.png"); diskPath=xidePathResolve(root,"disk.xl"); diskImage=xidePathResolve(root,"definition-disk.png");
 const char *saved="func main(...) as int\n    return 0\nend\n",*overload="func pick(int value) as int\n    return value\nend\nfunc pick(string value) as int\n    return 2\nend\nfunc main(...) as int\n    string 中文 = \"😀\"; auto second = pick(\"x\")\n    return second - 2\nend\n",*helper="export func answer() as int\n    return 42\nend\n",*overlay="\n\nexport func answer() as int\n    return 42\nend\n",*imported="import helper\nfunc main(...) as int\n    return helper.answer() - 42\nend\n";
 CHECK(root && entry && dep && native && light && dark && diskPath && diskImage && xrtDirCreateAll(root) && writeText(entry,saved) && writeText(dep,helper) && writeText(native,"int helper(void) { return 1; }\n") && writeText(diskPath,helper));
 app.hidden=1; app.noSession=1; app.compilerPath=f->compiler; CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1;
 CHECK(xideActionCount()==73 && xideActionNamed("language.definition")->id==69 && !xideActionQuery(&app,XIDE_ACTION_DEFINITION).enabled);
 CHECK(xideEditorOpen(&app,entry,&error) && xideEditorOpen(&app,dep,&error) && xideEditorOpen(&app,native,&error) && settle(&app)); xide_document *xl=xideDocumentsFind(&app.documents,entry),*dependency=xideDocumentsFind(&app.documents,dep),*c=app.activeDocument;
 CHECK(!strcmp(c->language,"c") && !xideActionQuery(&app,XIDE_ACTION_DEFINITION).enabled && !strcmp(xl->language,"xlang"));
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
 CHECK(xideEditorShow(&app,xl)==XUI_OK && xideActionExecute(&app,XIDE_ACTION_CANCEL_CHECK)==XIDE_ACTION_DISPATCHED && !xideActionQuery(&app,XIDE_ACTION_DEFINITION).enabled);
 const char* diskMain="import disk\nfunc main(...) as int\n    return disk.answer() - 42\nend\n";
 CHECK(replace(xl,diskMain) && xideActionExecute(&app,XIDE_ACTION_CHECK_CURRENT)==XIDE_ACTION_DISPATCHED && waitChecked(&app)); call=(int)(strstr(diskMain,"answer")-diskMain);
 CHECK(!xideDocumentsFind(&app.documents,diskPath) && gotoOffset(&app,xl,call) && xideActionExecute(&app,XIDE_ACTION_DEFINITION)==XIDE_ACTION_DISPATCHED && waitDefinition(&app) && app.activeDocument->path && xidePathLexicalEqual(app.activeDocument->path,diskPath) && caret(app.activeDocument)==(int)(strstr(helper,"answer")-helper) && capture(&app,diskImage));
 CHECK(waitChecked(&app) && xideEditorShow(&app,xl)==XUI_OK && replace(xl,"func main(...) as int\n    return \"bad\"\nend\n") && settle(&app) && !xideActionQuery(&app,XIDE_ACTION_DEFINITION).enabled);
 CHECK(waitChecked(&app) && xideLanguageViewDiagnosticCount(&app)==1 && !xideActionQuery(&app,XIDE_ACTION_DEFINITION).enabled);
 CHECK(xideActionExecute(&app,XIDE_ACTION_CANCEL_CHECK)==XIDE_ACTION_DISPATCHED && xideEditorShow(&app,c)==XUI_OK && !xideActionQuery(&app,XIDE_ACTION_DEFINITION).enabled && !c->dirty);
 xide_text_file file={0}; CHECK(xideTextFileRead(entry,&file,&error) && !strcmp(file.text,saved)); xideTextFileClear(&file); CHECK(xideTextFileRead(dep,&file,&error) && !strcmp(file.text,helper)); xideTextFileClear(&file);
 printf("definition_view_test passed: real F12 overload and TypeId aliases/class; new type facts=%d; current menu, Unicode and unsaved imported declaration, guarded unopened disk navigation, moved-caret/tab/malformed-range late response rejection, cancel/failure/C states, original files preserved; renders in %s.\n",f->typeFacts,root);
cleanup:
 xrtValueRelease(response); if(began) xgeEnd(); xideAppDestroy(&app); free(root); free(entry); free(dep); free(native); free(light); free(dark); free(diskPath); free(diskImage); xgeQuit(); return XGE_OK;
}
static DWORD WINAPI worker(void* data) { fixture* f=data; if(!SetThreadDesktop(f->desktop)) return 1; xge_desc_t d={0}; d.iWidth=1600; d.iHeight=1000; d.sTitle="xlang definition verification"; d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; d.iRunMode=XGE_RUN_GAME_LOOP; int result=xgeInit(&d); if(result==XGE_OK) result=xgeRun(verify,f); xgeUnit(); return result==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv) { if((argc!=3 && argc!=4) || (argc==4 && strcmp(argv[3],"type-facts")) || !xrtDirCreateAll(argv[1])) return 2; fixture f={.directory=argv[1],.compiler=argv[2],.typeFacts=argc==4}; wchar_t name[100]; swprintf(name,100,L"XlangDefinitionVerification-%lu",GetCurrentProcessId()); f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) return 1; HANDLE t=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1; if(t) { if(WaitForSingleObject(t,120000)!=WAIT_OBJECT_0) { fputs("definition_view_test timed out\n",stderr); ExitProcess(1); } GetExitCodeThread(t,&code); CloseHandle(t); } CloseDesktop(f.desktop); return (int)code; }
