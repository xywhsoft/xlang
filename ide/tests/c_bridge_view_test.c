#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "ui/language_view.h"
#include "ui/task_view.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Real native adapters: no fabricated bridge declarations or semantic replies. */
#include "../src/ui/c_language_view.c"
#include "../src/ui/c_completion_view.c"
typedef struct fixture { const char *directory,*server,*compiler; HDESK desktop; int started,failed,checks; } fixture;
#define CHECK(t) do { if(!(t)) { fprintf(stderr,"c_bridge_view_test:%d: %s; %s; C %s; completion %s\n",__LINE__,#t,error.message,xideCLanguageViewMessage(&app),app.cCompletionView ? app.cCompletionView->message : ""); f->failed=1; goto cleanup; } ++f->checks; } while(0)
static int writeText(const char* p,const char* s) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static int settle(xide_app* a) { if(xuiDispatchPendingEvents(a->context)!=XUI_OK) return 0; xideEditorPoll(a);xideTaskViewPoll(a);xideLanguageViewPoll(a);xideCLanguageViewPoll(a);xideWorkbenchLayout(a);return xuiLayout(a->context)==XUI_OK && !a->frameError; }
static int key(xide_app* a,int k,uint32_t mods) { return xuiInputKeyDownEx(a->context,k,mods,NULL)==XUI_OK && xuiInputKeyUpEx(a->context,k,mods,NULL)==XUI_OK && settle(a); }
static int position(xide_app* a,xide_document* d,int offset) { return xideEditorShow(a,d)==XUI_OK && xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(d->editor),xuiCodeEditGetDocument(d->editor),offset,0)==XUI_OK && xuiSetFocusWidget(a->context,d->editor)==XUI_OK && settle(a); }
static int replace(xide_document* d,const char* s) { return xuiCodeDocumentReplace(xuiCodeEditGetDocument(d->editor),0,xuiCodeDocumentGetLength(xuiCodeEditGetDocument(d->editor)),s)==XUI_OK; }
static int waitReady(xide_app* a) { uint64_t until=GetTickCount64()+60000;do { if(!settle(a)) return 0;xide_c_language_view* v=a->cLanguageView; if(v->ready && v->received==(int)v->inputs->count && v->symbolResult) return 1;if(v->paused) return 0;Sleep(5); } while(GetTickCount64()<until);return 0; }
static int check(xide_app* a) { return xideActionExecute(a,XIDE_ACTION_CHECK_CURRENT)==XIDE_ACTION_DISPATCHED && waitReady(a) && xuiSetFocusWidget(a->context,a->activeDocument->editor)==XUI_OK && settle(a); }
static int waitHover(xide_app* a) { uint64_t until=GetTickCount64()+30000;do { if(!settle(a)) return 0;if(a->cLanguageView->hoverResult) return 1;if(!a->cLanguageView->hover.token && !a->cLanguageView->hoverDue) return 0;Sleep(5); } while(GetTickCount64()<until);return 0; }
static int completion(xide_app* a) { if(!key(a,XUI_KEY_SPACE,XUI_MOD_CTRL)) return 0;uint64_t until=GetTickCount64()+60000;do { if(!settle(a)) return 0;if(a->cCompletionView->ready) return a->cCompletionView->result!=NULL;if(!a->cCompletionView->wanted) return 0;Sleep(5); } while(GetTickCount64()<until);return 0; }
static const char* detail(xide_c_completion_view* v,const char* name) { for(int i=0;i<v->count;++i) if(!strcmp(v->items[i].sFilterText,name)) return v->items[i].sDetail;return ""; }
static int record(const char* root,const char* name,const xvalue* v) { char* p=xidePathResolve(root,name);size_t n=0;char* text=xrtJsonStringify(v,true,&n);int okay=p && text && xrtFileWriteAtomic(p,(xbytesview){(unsigned char*)text,n});free(p);xrtFree(text);return okay; }
static int capture(xide_app* a,const char* path) { if(!settle(a) || xuiUpdate(a->context,0)!=XUI_OK || xuiLayout(a->context)!=XUI_OK) return 0;xui_rect_i_t r={0,0,a->viewportWidth,a->viewportHeight};return a->proxy.surfaceClear(&a->proxy,a->target,0xffeeeeeeu)==XUI_OK && xuiRender(a->context,a->target,&r,1)==XUI_OK && xideAppCapture(a,path)==XGE_OK; }
static int verify(void* user)
{
 fixture* f=user;if(f->started) return XGE_OK;f->started=1;xide_app app={0};xide_model_error error={0};xide_project project={0};int began=0;char *root=NULL,*xlpath=NULL,*cpath=NULL,*manifest=NULL,*settings=NULL,*light=NULL,*dark=NULL;unsigned char* bytes=NULL;size_t length=0;
 char unique[80];snprintf(unique,sizeof(unique),"bridge-ui-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64());root=xidePathResolve(f->directory,unique);xlpath=xidePathResolve(root,"入口😀.xl");cpath=xidePathResolve(root,"native.c");manifest=xidePathResolve(root,"工程.xlp");settings=xidePathResolve(root,"settings.json");light=xidePathResolve(root,"automatic-bridge-light.png");dark=xidePathResolve(root,"automatic-bridge-dark.png");
 const char* intXL="export \"xl_add\" func add(c_int a,c_int b) as c_int\n return a+b\nend\n";
 const char* doubleXL="export \"xl_add\" func add(c_double a,c_double b) as c_double\n return a+b\nend\n";
 const char* savedC="int native_answer(void) { return 42; }\r\n";
 const char* doubleC="/* Declarations come from the current xlang draft. */\n_Static_assert(__builtin_types_compatible_p(__typeof__(xl_add(1,2)), double), \"draft ABI\");\ndouble native_answer(void) { return xl_add(1,2); }\n";
 const char* intC="_Static_assert(__builtin_types_compatible_p(__typeof__(xl_add(1,2)), int), \"changed draft ABI\");\nint native_answer(void) { return xl_add(1,2); }\n";
 const char* completeC="double native_answer(void) { return xl_aOther; }\n";
 CHECK(root && xlpath && cpath && manifest && settings && light && dark && xrtDirCreateAll(root));CHECK(writeText(xlpath,intXL) && writeText(cpath,savedC));
 CHECK(xideProjectCreate(&project,manifest,"Automatic C/xlang bridge",xlpath,&error) && xideProjectAddFile(&project,cpath,"c",&error) && xideProjectSave(&project,manifest,&error));
 app.hidden=1;app.noSession=1;app.compilerPath=f->compiler;app.cServerPath=f->server;app.settingsPath=settings;app.projectPath=manifest;CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK);began=1;
 CHECK(xideEditorOpen(&app,xlpath,&error));xide_document* xl=app.activeDocument;CHECK(replace(xl,doubleXL));CHECK(xideEditorOpen(&app,cpath,&error));xide_document* c=app.activeDocument;xide_c_language_view* v=app.cLanguageView;
 CHECK(replace(c,doubleC) && position(&app,c,(int)(strrchr(doubleC,'x')-doubleC)+2) && check(&app));CHECK(v->diagnosticCount==0 && v->inputs->xlangCount==1 && xl->dirty && c->dirty && key(&app,'I',XUI_MOD_CTRL) && waitHover(&app));
 CHECK(strstr(xuiTextEditGetText(v->hoverText),"double") && record(root,"draft-hover.json",v->hoverResult) && capture(&app,light));CHECK(xideWorkbenchApplyTheme(&app,1)==XUI_OK && capture(&app,dark));
 CHECK(replace(xl,intXL) && settle(&app) && !v->ready && !v->symbolResult && !v->hoverResult && !v->diagnosticCount);CHECK(replace(c,intC) && position(&app,c,(int)(strrchr(intC,'x')-intC)+2) && waitReady(&app));CHECK(v->diagnosticCount==0 && key(&app,'I',XUI_MOD_CTRL) && waitHover(&app) && strstr(xuiTextEditGetText(v->hoverText),"int") && record(root,"changed-draft-hover.json",v->hoverResult));
 CHECK(replace(xl,doubleXL) && replace(c,completeC) && position(&app,c,(int)(strstr(completeC,"xl_aOther")-completeC)+4) && completion(&app));CHECK(record(root,"draft-completion.json",app.cCompletionView->result)); CHECK(strstr(detail(app.cCompletionView,"xl_add"),"double") && app.cCompletionView->batch->xlangCount==1);
 CHECK(replace(xl,intXL) && key(&app,XUI_KEY_ENTER,0) && !strcmp(xuiCodeEditGetText(c->editor),completeC) && !app.cCompletionView->ready);CHECK(completion(&app) && strstr(detail(app.cCompletionView,"xl_add"),"int") && record(root,"changed-draft-completion.json",app.cCompletionView->result));
 CHECK(key(&app,XUI_KEY_ENTER,0) && strstr(xuiCodeEditGetText(c->editor),"return xl_add;") && !strstr(xuiCodeEditGetText(c->editor),"Other"));
 CHECK(replace(c,intC) && position(&app,c,0) && check(&app));CHECK(replace(xl,"func broken( as\n") && settle(&app) && !v->ready && !v->symbolResult);uint64_t until=GetTickCount64()+30000;do { CHECK(settle(&app));if(*v->message && !v->job && v->retryCount) break;Sleep(10); } while(GetTickCount64()<until);CHECK(!v->ready && !v->symbolResult && !v->paused && v->retryCount>0 && *v->message);
 CHECK(replace(xl,intXL) && waitReady(&app) && v->diagnosticCount==0 && v->retryCount==0);
 CHECK(xideDocumentSetLanguage(xl,"text",&error) && settle(&app) && !v->ready);CHECK(xideDocumentSetLanguage(xl,"xlang",&error) && waitReady(&app));
 CHECK(xideActionExecute(&app,XIDE_ACTION_CANCEL_CHECK)==XIDE_ACTION_DISPATCHED && v->paused && !v->job);CHECK(check(&app));
 bytes=xrtFileReadAllLimit(xlpath,1024,&length);CHECK(bytes && length==strlen(intXL) && !memcmp(bytes,intXL,length));xrtFree(bytes);bytes=NULL;bytes=xrtFileReadAllLimit(cpath,1024,&length);CHECK(bytes && length==strlen(savedC) && !memcmp(bytes,savedC,length));xrtFree(bytes);bytes=NULL;
 printf("c_bridge_view_test passed: %d native assertions; real draft ABI diagnostics/hover/completion, automatic XL edit withdrawal/restart, rejection of stale Enter, malformed draft bounded recovery, language binding, cancellation, original files unchanged; renders in %s.\n",f->checks,root);
cleanup:
 xrtFree(bytes);if(began) xgeEnd();xideAppDestroy(&app);xideProjectClear(&project);free(root);free(xlpath);free(cpath);free(manifest);free(settings);free(light);free(dark);xgeQuit();return XGE_OK;
}
static DWORD WINAPI worker(void* data) { fixture* f=data;if(!SetThreadDesktop(f->desktop)) return 1;xge_desc_t d={0};d.iWidth=1600;d.iHeight=1000;d.sTitle="Automatic C/xlang bridge verification";d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI;d.iRunMode=XGE_RUN_GAME_LOOP;int r=xgeInit(&d);if(r==XGE_OK) r=xgeRun(verify,f);xgeUnit();return r==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv) { if(argc!=4 || !xrtDirCreateAll(argv[1])) return 2;fixture f={.directory=argv[1],.server=argv[2],.compiler=argv[3]};wchar_t name[100];swprintf(name,100,L"CBridgeVerification-%lu",GetCurrentProcessId());f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);if(!f.desktop) return 1;HANDLE t=CreateThread(NULL,0,worker,&f,0,NULL);DWORD code=1;if(t) { if(WaitForSingleObject(t,240000)!=WAIT_OBJECT_0) { fputs("c_bridge_view_test timed out\n",stderr);ExitProcess(1); }GetExitCodeThread(t,&code);CloseHandle(t); }CloseDesktop(f.desktop);return (int)code; }
