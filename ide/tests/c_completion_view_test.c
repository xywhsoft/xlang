#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "ui/workspace_view.h"
#include "ui/catalog_view.h"
#include "ui/task_view.h"
#include "ui/font_view.h"
#include "platform/path.h"
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Exercise the production adapter and replay mutated real LSP replies. */
#include "../src/ui/c_completion_view.c"
static char* cccopy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static int ccput(xvalue* v,const char* k,xvalue* child) { return child && xrtValueObjectSetNew(v,ccsv(k),child); }

typedef struct fixture { const char *directory,*server; HDESK desktop; int started,failed,checks; } fixture;
#define CHECK(t) do { if(!(t)) { fprintf(stderr,"c_completion_view_test:%d: %s; %s\n",__LINE__,#t,error.message); if(app.cCompletionView) fprintf(stderr,"C state wanted=%d ready=%d count=%d; %s\n",app.cCompletionView->wanted,app.cCompletionView->ready,app.cCompletionView->count,app.cCompletionView->message); if(app.activeDocument) fprintf(stderr,"text [%s]\n",xuiCodeEditGetText(app.activeDocument->editor)); f->failed=1; goto cleanup; } ++f->checks; } while(0)
static int writeText(const char* p,const char* s) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static int settle(xide_app* a) { if(xuiDispatchPendingEvents(a->context)!=XUI_OK) return 0; xideEditorPoll(a); xideTaskViewPoll(a); xideWorkbenchLayout(a); return xuiLayout(a->context)==XUI_OK && !a->frameError; }
static int idle(xide_app* a,int ms) { uint64_t until=GetTickCount64()+(uint64_t)ms; do { if(!settle(a)) return 0; Sleep(5); } while(GetTickCount64()<until); return 1; }
static int key(xide_app* a,int k,uint32_t mods) { return xuiInputKeyDownEx(a->context,k,mods,NULL)==XUI_OK && xuiInputKeyUpEx(a->context,k,mods,NULL)==XUI_OK && settle(a); }
static int position(xide_app* a,xide_document* d,int offset) { return xideEditorShow(a,d)==XUI_OK && xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(d->editor),xuiCodeEditGetDocument(d->editor),offset,0)==XUI_OK && xuiSetFocusWidget(a->context,d->editor)==XUI_OK; }
static int replace(xide_document* d,const char* s) { return xuiCodeDocumentReplace(xuiCodeEditGetDocument(d->editor),0,xuiCodeDocumentGetLength(xuiCodeEditGetDocument(d->editor)),s)==XUI_OK; }
static int prepare(xide_app* a,xide_document* d,const char* s,const char* token,int prefix) { ccdismiss(a->cCompletionView); const char* p=strstr(s,token); return p && replace(d,s) && position(a,d,(int)(p-s)+prefix) && settle(a); }
static int waitReady(xide_app* a) { uint64_t until=GetTickCount64()+30000; do { if(!settle(a)) return 0; if(a->cCompletionView->ready) return a->cCompletionView->result!=NULL; if(!a->cCompletionView->wanted) return 0; Sleep(5); } while(GetTickCount64()<until); return 0; }
static int waitInvalidated(xide_c_completion_view* v) { uint64_t until=GetTickCount64()+6000; do { if(xideCCompletionJobState(v->job,NULL,0)==XIDE_C_JOB_INVALIDATED) return 1; Sleep(5); } while(GetTickCount64()<until); return 0; }
static int invoke(xide_app* a) { return key(a,XUI_KEY_SPACE,XUI_MOD_CTRL) && waitReady(a); }
static int has(xide_c_completion_view* v,const char* name) { for(int i=0;i<v->count;++i) if(!strcmp(v->items[i].sFilterText,name)) return 1; return 0; }
static const char* detail(xide_c_completion_view* v,const char* name) { for(int i=0;i<v->count;++i) if(!strcmp(v->items[i].sFilterText,name)) return v->items[i].sDetail; return ""; }
static xui_widget listWidget(xide_app* a,xui_widget w) { if(!w) return NULL; if(xuiWidgetGetType(w)==xuiListViewGetType(a->context)) return w; for(xui_widget c=xuiWidgetGetFirstChild(w);c;c=xuiWidgetGetNextSibling(c)) { xui_widget found=listWidget(a,c); if(found) return found; } return NULL; }
static int capture(xide_app* a,const char* path)
{
 if(!settle(a) || xuiUpdate(a->context,0)!=XUI_OK || xuiLayout(a->context)!=XUI_OK) return 0;
 xui_widget list=listWidget(a,xuiCodeEditGetCompletionPopupWidget(a->activeDocument->editor)); if(!list || xuiListViewGetSelected(list)!=0) return 0;
 xui_rect_t row=xuiListViewGetItemRect(list,0),viewport=xuiListViewGetViewportRect(list); if(row.fY<viewport.fY || row.fY+row.fH>viewport.fY+viewport.fH || strcmp(xuiListViewGetItemText(list,0),a->cCompletionView->items[0].sLabel)) return 0;
 xui_rect_i_t r={0,0,a->viewportWidth,a->viewportHeight}; return a->proxy.surfaceClear(&a->proxy,a->target,0xffeeeeeeu)==XUI_OK && xuiRender(a->context,a->target,&r,1)==XUI_OK && xideAppCapture(a,path)==XGE_OK;
}
static int record(const char* root,const char* name,const xvalue* value)
{ char* path=xidePathResolve(root,name); size_t n=0; char* text=xrtJsonStringify(value,true,&n); int okay=path && text && xrtFileWriteAtomic(path,(xbytesview){(unsigned char*)text,n}); free(path); xrtFree(text); return okay; }
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
static int verify(void* user)
{
 fixture* f=user; if(f->started) return XGE_OK; f->started=1;
 xide_app app={0}; xide_model_error error={0}; xide_project project={0}; xvalue *valid=NULL,*fake=NULL; int began=0; xui_code_completion_item_t* composed=NULL;
 char *root=NULL,*entry=NULL,*header=NULL,*disk=NULL,*xlpath=NULL,*manifest=NULL,*light=NULL,*dark=NULL,*settings=NULL,*canonical=NULL;
 char unique[80]; snprintf(unique,sizeof(unique),"c-completion-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64()); root=xidePathResolve(f->directory,unique);
 entry=xidePathResolve(root,"入口😀.c"); header=xidePathResolve(root,"types.h"); disk=xidePathResolve(root,"disk.h"); xlpath=xidePathResolve(root,"main.xl"); manifest=xidePathResolve(root,"工程.xlp"); light=xidePathResolve(root,"c-completion-light.png"); dark=xidePathResolve(root,"c-completion-dark.png"); settings=xidePathResolve(root,"settings.json");
 const char *saved="int helper(void) { return 1; }\r\n",*types="typedef struct Box { int value; } Box;\n",*diskText="typedef struct Disk { double value; } Disk;\n";
 const char *local="int main(void) { int zzResult = 7; return zzROther; }\n",*member="#include \"types.h\"\nint main(void) { Box box = {0}; return box.vOther; }\n",*unicode="\xef\xbb\xbf" "int main(void) { const char *label = \"😀\"; int 结果 = 7; return 结后缀; }\r\n";
 CHECK(root && entry && header && disk && xlpath && manifest && light && dark && settings && xrtDirCreateAll(root));
 CHECK(writeText(entry,saved) && writeText(header,types) && writeText(disk,diskText) && writeText(xlpath,"func main(...) as int\n    return 0\nend\n"));
 CHECK(xideProjectCreate(&project,manifest,"C native completion",xlpath,&error) && xideProjectAddFile(&project,entry,"c",&error) && xideProjectAddFile(&project,header,"header",&error) && xideProjectAddFile(&project,disk,"header",&error) && xideProjectSave(&project,manifest,&error));
 app.hidden=1; app.noSession=1; app.cServerPath=f->server; app.settingsPath=settings; app.projectPath=manifest;
 CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1;
 CHECK(xideEditorOpen(&app,entry,&error) && settle(&app)); xide_document* c=app.activeDocument; xide_c_completion_view* v=app.cCompletionView;
 CHECK(xideActionQuery(&app,XIDE_ACTION_C_SERVER).enabled && xideCCompletionViewServer(&app)==f->server);
 CHECK(prepare(&app,c,local,"zzROther",3) && invoke(&app) && has(v,"zzResult") && !strcmp(detail(v,"zzResult"),"int") && xuiCodeEditIsCompletionOpen(c->editor));
 CHECK(v->start==(int)(strstr(local,"zzROther")-local) && v->end-v->start==8 && capture(&app,light) && record(root,"real-completion.json",v->result));
 CHECK(key(&app,XUI_KEY_ENTER,0) && strstr(xuiCodeEditGetText(c->editor),"return zzResult;") && !strstr(xuiCodeEditGetText(c->editor),"Other") && c->dirty);
 CHECK(key(&app,'Z',XUI_MOD_CTRL) && !strcmp(xuiCodeEditGetText(c->editor),local));
 CHECK(key(&app,'Y',XUI_MOD_CTRL) && strstr(xuiCodeEditGetText(c->editor),"return zzResult;"));
 CHECK(prepare(&app,c,unicode,"结后缀",3)); canonical=cccopy(xuiCodeEditGetText(c->editor)); CHECK(canonical && invoke(&app) && has(v,"结果") && v->end-v->start==9 && record(root,"unicode-completion.json",v->result));
 CHECK(xideWorkbenchApplyTheme(&app,1)==XUI_OK && invoke(&app) && capture(&app,dark));
 CHECK(key(&app,XUI_KEY_TAB,0) && strstr(xuiCodeEditGetText(c->editor),"return 结果;") && !strstr(xuiCodeEditGetText(c->editor),"后缀"));
 CHECK(key(&app,'Z',XUI_MOD_CTRL) && !strcmp(xuiCodeEditGetText(c->editor),canonical));
 CHECK(xideEditorOpen(&app,header,&error) && settle(&app)); xide_document* h=app.activeDocument;
 CHECK(replace(h,"typedef struct Box { const char *value; } Box;\n") && prepare(&app,c,member,"vOther",1) && invoke(&app) && has(v,"value") && strstr(detail(v,"value"),"char") && record(root,"unsaved-header-completion.json",v->result));
 CHECK(key(&app,XUI_KEY_ENTER,0) && strstr(xuiCodeEditGetText(c->editor),"return box.value;") && key(&app,'Z',XUI_MOD_CTRL) && !strcmp(xuiCodeEditGetText(c->editor),member));
 CHECK(replace(h,types) && position(&app,c,(int)(strstr(member,"vOther")-member)+1) && invoke(&app) && !strcmp(detail(v,"value"),"int"));

 const char* dot="#include \"types.h\"\nint main(void) { Box box = {0}; box }\n";
 CHECK(prepare(&app,c,dot,"box }",3) && xuiInputSetModifiers(app.context,0)==XUI_OK && xuiInputTextEx(app.context,'.',NULL)==XUI_OK && waitReady(&app) && has(v,"value") && v->start==v->end);
 CHECK(key(&app,XUI_KEY_ESCAPE,0) && idle(&app,150) && !v->wanted && !xuiCodeEditIsCompletionOpen(c->editor));
 const char* arrow="#include \"types.h\"\nint main(void) { Box box = {0}; Box *ptr = &box; ptr- }\n";
 CHECK(prepare(&app,c,arrow,"ptr- }",4) && xuiInputSetModifiers(app.context,0)==XUI_OK && xuiInputTextEx(app.context,'>',NULL)==XUI_OK && waitReady(&app) && has(v,"value"));
 CHECK(prepare(&app,c,local,"zzROther",3) && key(&app,XUI_KEY_SPACE,XUI_MOD_CTRL) && key(&app,XUI_KEY_ESCAPE,0) && idle(&app,150) && !v->wanted);
 CHECK(invoke(&app) && has(v,"zzResult") && position(&app,c,0) && settle(&app) && !v->result);
 CHECK(prepare(&app,c,local,"zzROther",3) && invoke(&app) && xuiSetFocusWidget(app.context,app.commands)==XUI_OK && settle(&app) && !v->result);
 CHECK(prepare(&app,c,member,"vOther",1) && invoke(&app) && replace(h,"typedef struct Box { double value; } Box;\n") && key(&app,XUI_KEY_ENTER,0) && !strcmp(xuiCodeEditGetText(c->editor),member));
 CHECK(position(&app,c,(int)(strstr(member,"vOther")-member)+1) && invoke(&app) && !strcmp(detail(v,"value"),"double"));
 CHECK(xideDocumentSetLanguage(h,"text",&error) && key(&app,XUI_KEY_ENTER,0) && !strcmp(xuiCodeEditGetText(c->editor),member) && xideDocumentSetLanguage(h,"c",&error));
 CHECK(prepare(&app,c,local,"zzROther",3) && invoke(&app) && xideFontViewOpen(&app) && idle(&app,100) && !v->result && xideDialogSetOpen(xideFontViewWindow(&app),0)==XUI_OK);
 CHECK(position(&app,c,(int)(strstr(local,"zzROther")-local)+3) && invoke(&app));
 c->externalState=XIDE_WATCH_UNSTABLE; CHECK(key(&app,XUI_KEY_ENTER,0) && !strcmp(xuiCodeEditGetText(c->editor),local)); c->externalState=0;
 CHECK(position(&app,c,(int)(strstr(local,"zzROther")-local)+3) && invoke(&app));
 app.configurationId="missing-configuration"; CHECK(settle(&app) && !v->result && key(&app,XUI_KEY_SPACE,XUI_MOD_CTRL) && idle(&app,150) && !v->wanted); app.configurationId=NULL;
 CHECK(invoke(&app) && has(v,"zzResult"));
 xide_project* live=&app.workspaceModel.projects[0]; CHECK(xideProjectSetMetadata(live,"Updated","C query invalidation",live->kind,&error) && settle(&app) && !v->result);
 CHECK(invoke(&app) && has(v,"zzResult"));
 app.cServerPath="missing-server.exe"; CHECK(key(&app,XUI_KEY_ENTER,0) && !strcmp(xuiCodeEditGetText(c->editor),local)); app.cServerPath=f->server;
 CHECK(invoke(&app) && has(v,"zzResult")); xideCCompletionJobCancel(v->job);
 CHECK(!xideCCompletionJobResult(v->job) && key(&app,XUI_KEY_ENTER,0) && !strcmp(xuiCodeEditGetText(c->editor),local) && invoke(&app) && has(v,"zzResult"));
 CHECK(crashOwnedServer(f->server));
 CHECK(waitInvalidated(v));
 CHECK(key(&app,XUI_KEY_ENTER,0) && !strcmp(xuiCodeEditGetText(c->editor),local) && !v->result && invoke(&app) && has(v,"zzResult"));
 CHECK(prepare(&app,c,local,"zzROther",3) && invoke(&app) && xuiInputSetModifiers(app.context,0)==XUI_OK && xuiInputTextEx(app.context,'e',NULL)==XUI_OK && waitReady(&app) && has(v,"zzResult") && v->version==xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(c->editor)));
 CHECK(key(&app,XUI_KEY_ENTER,0) && strstr(xuiCodeEditGetText(c->editor),"return zzResult;") && key(&app,'Z',XUI_MOD_CTRL) && strstr(xuiCodeEditGetText(c->editor),"return zzReOther;"));
 CHECK(prepare(&app,c,"// zzR","zzR",3) && key(&app,XUI_KEY_SPACE,XUI_MOD_CTRL) && idle(&app,120) && !v->wanted && !xuiCodeEditIsCompletionOpen(c->editor));
 CHECK(prepare(&app,c,"\"zzR","zzR",3) && key(&app,XUI_KEY_SPACE,XUI_MOD_CTRL) && idle(&app,120) && !v->wanted && !xuiCodeEditIsCompletionOpen(c->editor));
 const char* function="int zzPick(int x) { return x; } int main(void) { zzPiOther; }\n";
 CHECK(prepare(&app,c,function,"zzPiOther",4) && invoke(&app) && has(v,"zzPick"));
 CHECK(key(&app,XUI_KEY_ENTER,0) && strstr(xuiCodeEditGetText(c->editor),"zzPick;") && !strstr(xuiCodeEditGetText(c->editor),"Other") && key(&app,'Z',XUI_MOD_CTRL) && !strcmp(xuiCodeEditGetText(c->editor),function));
 const char* macro="#define ZZVALUE 7\nint main(void) { return ZZV; }\n";
 CHECK(prepare(&app,c,macro,"return ZZV",10) && invoke(&app) && has(v,"ZZVALUE"));
 CHECK(prepare(&app,c,"int main(void) { ret }\n","ret }",3) && invoke(&app) && has(v,"return"));
 composed=calloc(8192,sizeof(*composed)); CHECK(composed); int count=0,returns=0;
 CHECK(xuiCodeProviderRequestCompletion(xuiCodeEditGetProviders(c->editor),c->editor,v->offset,"ret",composed,8192,&count)==XUI_OK);
 for(int i=0;i<count;++i) if(!strcmp(composed[i].sFilterText,"return")) ++returns;
 CHECK(returns==1 && key(&app,XUI_KEY_ENTER,0) && strstr(xuiCodeEditGetText(c->editor),"return"));
 CHECK(prepare(&app,c,local,"zzROther",3) && invoke(&app)); valid=xrtValueDeepClone(v->result); CHECK(valid);
 for(int mode=0;mode<12;++mode) {
  fake=xrtValueDeepClone(valid); CHECK(fake); ccclear(v);
  xvalue* result=ccget(fake,"result"),*items=xrtValueType(result)==XVALUE_ARRAY ? result : ccget(result,"items"),*item=xrtValueArrayGet(items,0),*edit=ccget(item,"textEdit"),*range=ccget(edit,"range");
  if(mode==0) ccput(item,"insertTextFormat",xrtValueInt(2));
  if(mode==1) ccput(edit,"newText",xrtValueString(ccsv("bad\ncode")));
  if(mode==2) ccput(item,"filterText",xrtValueString(ccsv("not an identifier")));
  if(mode==3) ccput(item,"label",xrtValueInt(3));
  if(mode==4) ccput(ccget(range,"start"),"character",xrtValueInt(0));
  if(mode==5) ccput(ccget(range,"end"),"character",xrtValueInt(999999));
  if(mode==6) ccput(ccget(range,"end"),"line",xrtValueInt(-1));
  if(mode==7) ccput(item,"detail",xrtValueInt(0));
  if(mode==8) { xvalue* additional=xrtValueArray(); xrtValueArrayAppendNew(additional,xrtValueObject()); ccput(item,"additionalTextEdits",additional); }
  if(mode==9) ccput(edit,"newText",xrtValueString(ccsv("")));
  if(mode==10) ccput(item,"sortText",xrtValueBool(true));
  if(mode==11) ccput(fake,"error",xrtValueObject());
  CHECK(!ccapply(v,fake) && !v->result && !v->count); xrtValueRelease(fake); fake=NULL; CHECK(ccapply(v,valid) && has(v,"zzResult"));
 } xrtValueRelease(valid); valid=NULL;
 fake=xrtValueDeepClone(v->result); CHECK(fake && position(&app,c,0) && !ccapply(v,fake)); xrtValueRelease(fake); fake=NULL; CHECK(settle(&app) && !v->result);
 const char* diskQuery="#include \"disk.h\"\nint main(void) { Disk box = {0}; return box.vOther; }\n";
 CHECK(prepare(&app,c,diskQuery,"vOther",1) && invoke(&app) && !strcmp(detail(v,"value"),"double"));
 CHECK(writeText(disk,"typedef struct Disk { int value; } Disk;\n"));
 /* No UI polling before Enter: the worker's observed-file state is checked in PREVIEW. */
 CHECK(waitInvalidated(v)); CHECK(key(&app,XUI_KEY_ENTER,0) && !strcmp(xuiCodeEditGetText(c->editor),diskQuery) && !v->result);
 CHECK(invoke(&app) && !strcmp(detail(v,"value"),"int"));
 CHECK(writeText(disk,"typedef struct Disk { int value; } Disk;\n")); CHECK(waitInvalidated(v));
 CHECK(key(&app,XUI_KEY_ENTER,0) && !strcmp(xuiCodeEditGetText(c->editor),diskQuery) && !v->result);
 CHECK(invoke(&app)); wchar_t wd[32768]; CHECK(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,disk,-1,wd,32768) && DeleteFileW(wd)); CHECK(waitInvalidated(v));
 CHECK(key(&app,XUI_KEY_ENTER,0) && !strcmp(xuiCodeEditGetText(c->editor),diskQuery) && !v->result && writeText(disk,diskText));
 CHECK(invoke(&app) && !strcmp(detail(v,"value"),"double"));
 HANDLE lock=CreateFileW(wd,GENERIC_READ,0,NULL,OPEN_EXISTING,0,NULL); CHECK(lock!=INVALID_HANDLE_VALUE); int invalid=waitInvalidated(v); int rejected=invalid &&key(&app,XUI_KEY_ENTER,0) && !strcmp(xuiCodeEditGetText(c->editor),diskQuery) && !v->result; CloseHandle(lock); CHECK(rejected);
 CHECK(invoke(&app) && has(v,"value"));
 CHECK(xideEditorOpen(&app,xlpath,&error) && settle(&app) && !v->result && !v->wanted); xide_document* xl=app.activeDocument;
 CHECK(prepare(&app,xl,"math.sq","math.sq",7) && !v->wanted);
 CHECK(prepare(&app,c,local,"zzROther",3) && invoke(&app) && xideDocumentSetLanguage(c,"text",&error) && settle(&app) && !v->result && xideDocumentSetLanguage(c,"c",&error));
 for(int i=0;i<8;++i) { CHECK(prepare(&app,c,local,"zzROther",3) && key(&app,XUI_KEY_SPACE,XUI_MOD_CTRL)); CHECK(v->retiredCount<=4); }
 CHECK(idle(&app,1000) && invoke(&app) && has(v,"zzResult"));
 CHECK(prepare(&app,c,local,"zzROther",3) && key(&app,XUI_KEY_SPACE,XUI_MOD_CTRL)); uint64_t oldId=c->id;
 CHECK(xideEditorClose(&app,c,1,&error) && settle(&app) && !v->result); xide_document* reused=xideDocumentNew(&app.documents,"c",&error);
 CHECK(reused==c && reused->id!=oldId && xideEditorShow(&app,reused)==XUI_OK && idle(&app,150) && !v->wanted);
 size_t bytes=0; unsigned char* original=xrtFileReadAllLimit(entry,1024,&bytes); CHECK(original && bytes==strlen(saved) && !memcmp(original,saved,bytes)); xrtFree(original);
 original=xrtFileReadAllLimit(header,1024,&bytes); CHECK(original && bytes==strlen(types) && !memcmp(original,types,bytes)); xrtFree(original);
 printf("c_completion_view_test passed: %d native assertions; real clangd Enter/Tab/undo/redo, full-token suffix replacement, C/header unsaved types, Unicode, dot/arrow, function/macro/keyword, catalog deduplication, malformed LSP edits, input/model/identity invalidation, explicit disk identity/content/missing/unreadable guards, real server crash/cancellation/restart, slot reuse and original-byte preservation; renders in %s.\n",f->checks,root);
cleanup:
 free(composed); xrtValueRelease(valid); xrtValueRelease(fake); if(began) xgeEnd(); xideAppDestroy(&app); xideProjectClear(&project);
 free(root); free(entry); free(header); free(disk); free(xlpath); free(manifest); free(light); free(dark); free(settings); free(canonical); xgeQuit(); return XGE_OK;
}
static DWORD WINAPI worker(void* data) { fixture* f=data; if(!SetThreadDesktop(f->desktop)) return 1; xge_desc_t d={0}; d.iWidth=1600; d.iHeight=1000; d.sTitle="C completion verification"; d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; d.iRunMode=XGE_RUN_GAME_LOOP; int r=xgeInit(&d); if(r==XGE_OK) r=xgeRun(verify,f); xgeUnit(); return r==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv) { if(argc!=3 || !xrtDirCreateAll(argv[1])) return 2; fixture f={.directory=argv[1],.server=argv[2]}; wchar_t name[100]; swprintf(name,100,L"CCompletionVerification-%lu",GetCurrentProcessId()); f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) return 1; HANDLE t=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1; if(t) { if(WaitForSingleObject(t,240000)!=WAIT_OBJECT_0) { fputs("c_completion_view_test timed out\n",stderr); ExitProcess(1); } GetExitCodeThread(t,&code); CloseHandle(t); } CloseDesktop(f.desktop); return (int)code; }
