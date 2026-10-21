#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "ui/language_view.h"
#include "ui/catalog_view.h"
#include "ui/font_view.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Include the actual adapter to test malformed and late response rejection. */
#include "../src/ui/completion_view.c"
typedef struct fixture { const char *directory,*compiler,*legacy; HDESK desktop; int started,failed,checks; } fixture;
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"completion_view_test:%d: %s; %s\n",__LINE__,#v,error.message); if(app.activeDocument) fprintf(stderr,"current text [%s]; completion wanted=%d ready=%d count=%d open=%d\n",xuiCodeEditGetText(app.activeDocument->editor),app.completionView->wanted,app.completionView->ready,app.completionView->count,xuiCodeEditIsCompletionOpen(app.activeDocument->editor)); fprintf(stderr,"focus editor=%d modal=%d check busy=%d check message=%s\n",app.activeDocument && xuiGetFocusWidget(app.context)==app.activeDocument->editor,xideActionsHasModal(&app),xideLanguageViewBusy(&app),xideLanguageViewMessage(&app)); f->failed=1; goto cleanup; } ++f->checks; } while(0)
static int writeText(const char* path,const char* text) { return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)}); }
static int settle(xide_app* a) { if(xuiDispatchPendingEvents(a->context)!=XUI_OK) return 0; xideEditorPoll(a); xideTaskViewPoll(a); xideLanguageViewPoll(a); xideWorkbenchLayout(a); return xuiLayout(a->context)==XUI_OK && !a->frameError; }
static int waitReady(xide_app* a) { uint64_t until=GetTickCount64()+20000; do { if(!settle(a)) return 0; if(a->completionView->ready) return 1; Sleep(5); } while(GetTickCount64()<until); return 0; }
static int idle(xide_app* a,int ms) { uint64_t until=GetTickCount64()+(uint64_t)ms; do { if(!settle(a)) return 0; Sleep(5); } while(GetTickCount64()<until); return 1; }
static int key(xide_app* a,int k,uint32_t mods) { return xuiInputKeyDownEx(a->context,k,mods,NULL)==XUI_OK && xuiInputKeyUpEx(a->context,k,mods,NULL)==XUI_OK && settle(a); }
static int position(xide_app* a,xide_document* d,int offset) { return xideEditorShow(a,d)==XUI_OK && xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(d->editor),xuiCodeEditGetDocument(d->editor),offset,0)==XUI_OK && xuiSetFocusWidget(a->context,d->editor)==XUI_OK; }
static int replace(xide_document* d,const char* text) { return xuiCodeDocumentReplace(xuiCodeEditGetDocument(d->editor),0,xuiCodeDocumentGetLength(xuiCodeEditGetDocument(d->editor)),text)==XUI_OK; }
static int prepare(xide_app* a,xide_document* d,const char* text,const char* token,int prefix) { cvdismiss(a->completionView); const char* p=strstr(text,token); return p && replace(d,text) && position(a,d,(int)(p-text)+prefix) && settle(a); }
static int invoke(xide_app* a) { return key(a,XUI_KEY_SPACE,XUI_MOD_CTRL) && waitReady(a); }
static int has(xide_completion_view* v,const char* name) { for(int i=0;i<v->count;++i) if(!strcmp(v->items[i].sLabel,name)) return 1; return 0; }
static xui_widget candidateList(xide_app* a,xui_widget widget)
{
 if(!widget) return NULL;
 if(xuiWidgetGetType(widget)==xuiListViewGetType(a->context)) return widget;
 for(xui_widget child=xuiWidgetGetFirstChild(widget);child;child=xuiWidgetGetNextSibling(child)) { xui_widget found=candidateList(a,child); if(found) return found; }
 return NULL;
}
static int capture(xide_app* a,const char* path) { if(!settle(a) || xuiUpdate(a->context,0)!=XUI_OK || xuiLayout(a->context)!=XUI_OK) return 0; xui_widget list=candidateList(a,xuiCodeEditGetCompletionPopupWidget(a->activeDocument->editor)); if(list) { xui_rect_t row=xuiListViewGetItemRect(list,0),viewport=xuiListViewGetViewportRect(list); if(xuiListViewGetSelected(list)!=0 || row.fY<viewport.fY || row.fY+row.fH>viewport.fY+viewport.fH || !a->completionView->count || strcmp(xuiListViewGetItemText(list,0),a->completionView->items[0].sLabel)) return 0; } xui_rect_i_t r={0,0,a->viewportWidth,a->viewportHeight}; return a->proxy.surfaceClear(&a->proxy,a->target,0xffeeeeeeu)==XUI_OK && xuiRender(a->context,a->target,&r,1)==XUI_OK && xideAppCapture(a,path)==XGE_OK; }
static int verify(void* user)
{
 fixture* f=user; if(f->started) return XGE_OK; f->started=1; xide_app app={0}; xide_model_error error={0}; int began=0; xvalue *fake=NULL,*valid=NULL; xui_code_completion_item_t* composed=NULL;
 char *root=NULL,*entry=NULL,*dep=NULL,*disk=NULL,*native=NULL,*light=NULL,*dark=NULL,*canonicalChinese=NULL;
 char* previousPackage=NULL; int packageTouched=0; CHECK(xideLanguageCheckPackagePath(&previousPackage,&error));
 char unique[80]; snprintf(unique,sizeof(unique),"completion-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64()); root=xidePathResolve(f->directory,unique); entry=xidePathResolve(root,"入口😀.xl"); dep=xidePathResolve(root,"dep.xl"); disk=xidePathResolve(root,"disk.xl"); native=xidePathResolve(root,"helper.c"); light=xidePathResolve(root,"completion-light.png"); dark=xidePathResolve(root,"completion-dark.png");
 const char *saved="func main(...) as int\n    return 0\nend\n",*local="func main(...) as int\n    int zzResult = 7\n    return zzROther\nend\n",*chinese="\xef\xbb\xbf" "func main(...) as int\r\n    string 文本 = \"😀\"; int 结果 = 7\r\n    return 结后缀\r\nend\r\n",*klass="class Box\n    int value\n    func getValue() as int\n        return self.value\n    end\nend\nfunc main(...) as int\n    auto box = Box()\n    return box\nend\n",*imported="import dep\nfunc main(...) as int\n    auto box = dep.Box()\n    return box.\nend\n",*helper="export class Box\n    int value\nend\n";
 CHECK(root && entry && dep && disk && native && light && dark && xrtDirCreateAll(root) && writeText(entry,saved) && writeText(dep,helper) && writeText(disk,helper) && writeText(native,"int helper(void) { return 1; }\n"));
 app.hidden=1; app.noSession=1; app.compilerPath=f->compiler; CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1;
 CHECK(xideEditorOpen(&app,entry,&error) && settle(&app)); xide_document* xl=app.activeDocument; xide_completion_view* v=app.completionView;
 CHECK(prepare(&app,xl,local,"zzROther",3) && invoke(&app) && v->count==1 && has(v,"zzResult") && !strcmp(v->items[0].sDetail,"int") && xuiCodeEditIsCompletionOpen(xl->editor));
 CHECK(v->start==(int)(strstr(local,"zzROther")-local) && v->end==v->start+8 && capture(&app,light));
 /* Repeating a query keeps the connection, wire documents and captured body. */
 xls_sync_stats syncBefore={0},syncAfter={0}; xlsSyncStats(v->sync,&syncBefore);
 xide_language_client* retainedClient=v->client; char* retainedText=v->inputs[0].text;
 CHECK(v->sync && (v->syncKind==1 || v->syncKind==2) && invoke(&app) && has(v,"zzResult"));
 xlsSyncStats(v->sync,&syncAfter);
 CHECK(v->client==retainedClient && v->inputs[0].text==retainedText && !memcmp(&syncBefore,&syncAfter,sizeof(syncBefore)));
 int literal=(int)(strstr(local,"= 7")-local)+2;
 CHECK(xuiCodeDocumentReplace(xuiCodeEditGetDocument(xl->editor),literal,literal+1,"8")==XUI_OK &&
       position(&app,xl,(int)(strstr(local,"zzROther")-local)+3) && invoke(&app) && has(v,"zzResult"));
 xlsSyncStats(v->sync,&syncAfter);
 CHECK(v->client==retainedClient && syncAfter.opens==syncBefore.opens && syncAfter.closes==syncBefore.closes && syncAfter.changes==syncBefore.changes+1);
 CHECK(syncAfter.textBytes-syncBefore.textBytes==(v->syncKind==2 ? 1u : strlen(local)));
 CHECK(prepare(&app,xl,local,"zzROther",3) && invoke(&app) && has(v,"zzResult"));
 CHECK(key(&app,XUI_KEY_ENTER,0) && strstr(xuiCodeEditGetText(xl->editor),"return zzResult\n") && !strstr(xuiCodeEditGetText(xl->editor),"Other") && xl->dirty);
 CHECK(key(&app,'Z',XUI_MOD_CTRL) && !strcmp(xuiCodeEditGetText(xl->editor),local));
 CHECK(key(&app,'Y',XUI_MOD_CTRL) && strstr(xuiCodeEditGetText(xl->editor),"return zzResult\n"));
 CHECK(prepare(&app,xl,chinese,"结后缀",3)); canonicalChinese=cvcopy(xuiCodeEditGetText(xl->editor)); CHECK(canonicalChinese && invoke(&app) && v->count==1 && has(v,"结果") && v->end-v->start==9);
 CHECK(xideWorkbenchApplyTheme(&app,1)==XUI_OK && invoke(&app) && capture(&app,dark) && xuiCodeEditIsCompletionOpen(xl->editor));
 CHECK(key(&app,XUI_KEY_TAB,0)); CHECK(strstr(xuiCodeEditGetText(xl->editor),"return 结果") && !strstr(xuiCodeEditGetText(xl->editor),"后缀")); CHECK(key(&app,'Z',XUI_MOD_CTRL)); CHECK(!strcmp(xuiCodeEditGetText(xl->editor),canonicalChinese));
 CHECK(prepare(&app,xl,klass,"return box",10) && xuiInputSetModifiers(app.context,0)==XUI_OK && xuiInputTextEx(app.context,'.',NULL)==XUI_OK && waitReady(&app) && v->count==2 && has(v,"value") && has(v,"getValue") && v->start==v->end);
 CHECK(key(&app,XUI_KEY_ESCAPE,0) && idle(&app,250) && !v->wanted && !xuiCodeEditIsCompletionOpen(xl->editor));
 CHECK(xideEditorOpen(&app,dep,&error) && settle(&app)); xide_document* dependency=app.activeDocument;
 CHECK(replace(dependency,"export class Box\n    string value\nend\n") && prepare(&app,xl,imported,"box.",4) && invoke(&app) && v->count==1 && has(v,"value") && !strcmp(v->items[0].sDetail,"string"));
 CHECK(key(&app,XUI_KEY_ENTER,0) && strstr(xuiCodeEditGetText(xl->editor),"return box.value\n") && key(&app,'Z',XUI_MOD_CTRL) && !strcmp(xuiCodeEditGetText(xl->editor),imported));
 CHECK(position(&app,xl,(int)(strstr(imported,"box.")-imported)+4) && invoke(&app) && has(v,"value")); xlsSyncStats(v->sync,&syncBefore); retainedClient=v->client;
 CHECK(position(&app,xl,(int)(strstr(xuiCodeEditGetText(xl->editor),"return box.")-xuiCodeEditGetText(xl->editor))+11) && replace(dependency,helper) && invoke(&app) && has(v,"value") && !strcmp(v->items[0].sDetail,"int"));
 xlsSyncStats(v->sync,&syncAfter);
 CHECK(v->client==retainedClient && syncAfter.opens==syncBefore.opens && syncAfter.closes==syncBefore.closes && syncAfter.changes==syncBefore.changes+1);
 CHECK(prepare(&app,xl,local,"zzROther",3) && key(&app,XUI_KEY_SPACE,XUI_MOD_CTRL) && key(&app,XUI_KEY_ESCAPE,0) && idle(&app,250) && !v->wanted && !xuiCodeEditIsCompletionOpen(xl->editor));
 CHECK(invoke(&app) && has(v,"zzResult") && position(&app,xl,0) && settle(&app) && !v->result && !xuiCodeEditIsCompletionOpen(xl->editor));
 CHECK(prepare(&app,xl,local,"zzROther",3) && invoke(&app) && xuiSetFocusWidget(app.context,app.commands)==XUI_OK && settle(&app) && !v->result && !xuiCodeEditIsCompletionOpen(xl->editor));
 CHECK(prepare(&app,xl,local,"zzROther",3) && invoke(&app) && replace(dependency,"export class Box\n    string value\nend\n") && settle(&app) && !v->result);
 CHECK(replace(dependency,helper) && prepare(&app,xl,local,"zzROther",3) && invoke(&app));
 CHECK(replace(dependency,"export class Box\n    string value\nend\n") && key(&app,XUI_KEY_ENTER,0) && !strcmp(xuiCodeEditGetText(xl->editor),local));
 CHECK(replace(dependency,helper) && position(&app,xl,(int)(strstr(local,"zzROther")-local)+3) && invoke(&app));
 CHECK(xideFontViewOpen(&app) && idle(&app,160) && !v->wanted && !v->result && xideDialogSetOpen(xideFontViewWindow(&app),0)==XUI_OK);
 CHECK(position(&app,xl,(int)(strstr(local,"zzROther")-local)+3) && invoke(&app));
 packageTouched=1; CHECK(SetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",L".") && settle(&app) && !v->result);
 CHECK(invoke(&app) && has(v,"zzResult"));
 wchar_t restored[32768]; CHECK(!previousPackage || MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,previousPackage,-1,restored,32768));
 CHECK(SetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",previousPackage ? restored : NULL) && settle(&app)); packageTouched=0;
 CHECK(position(&app,xl,(int)(strstr(local,"zzROther")-local)+3) && invoke(&app));
 CHECK(xuiInputSetModifiers(app.context,0)==XUI_OK && xuiInputTextEx(app.context,'e',NULL)==XUI_OK && waitReady(&app) && v->version==xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xl->editor)) && has(v,"zzResult"));
 CHECK(key(&app,XUI_KEY_ENTER,0) && strstr(xuiCodeEditGetText(xl->editor),"return zzResult\n") && key(&app,'Z',XUI_MOD_CTRL) && strstr(xuiCodeEditGetText(xl->editor),"return zzReOther\n"));
 const char* sameName="func main(...) as int\n    int print = 7\n    return pri\nend\n";
 CHECK(prepare(&app,xl,sameName,"return pri",10) && invoke(&app) && has(v,"print"));
 composed=calloc(8192,sizeof(*composed)); CHECK(composed); int composedCount=0,printCount=0;
 CHECK(xuiCodeProviderRequestCompletion(xuiCodeEditGetProviders(xl->editor),xl->editor,v->offset,"pri",composed,8192,&composedCount)==XUI_OK);
 for(int i=0;i<composedCount;++i) if(!strcmp(composed[i].sLabel,"print")) ++printCount;
 CHECK(printCount==1);
 const char* checked="func main(...) as int\n    int zzResult = 7\n    return zzResult\nend\n";
 CHECK(prepare(&app,xl,checked,"return zzResult",10)); CHECK(xideActionExecute(&app,XIDE_ACTION_CHECK_CURRENT)==XIDE_ACTION_DISPATCHED); CHECK(invoke(&app)); CHECK(has(v,"zzResult"));
 for(int i=0;i<3000 && xideLanguageViewBusy(&app);++i) { CHECK(settle(&app)); Sleep(5); }
 CHECK(!xideLanguageViewBusy(&app) && xideLanguageViewCanDefine(&app) && v->result && has(v,"zzResult"));
 CHECK(xideActionExecute(&app,XIDE_ACTION_CANCEL_CHECK)==XIDE_ACTION_DISPATCHED && settle(&app) && v->result && has(v,"zzResult"));
 /* The real adapter must reject every malformed edit before exposing strings. */
 CHECK(prepare(&app,xl,local,"zzROther",3) && invoke(&app)); valid=xrtValueDeepClone(v->result); CHECK(valid);
 for(int mode=0;mode<12;++mode) {
  fake=xrtValueDeepClone(valid); CHECK(fake); cvclearResult(v);
  xvalue *item=xrtValueArrayGet(cvget(fake,"items"),0),*edit=cvget(item,"textEdit"),*range=cvget(fake,"replacementRange");
  if(mode==0) cvput(fake,"queryId",xrtValueInt(v->request+1));
  if(mode==1) cvput(fake,"version",xrtValueInt(999999));
  if(mode==2) cvput(fake,"scope",xrtValueString(cvsv("whole-project")));
  if(mode==3) cvput(fake,"guardScope",xrtValueString(cvsv("unchecked")));
  if(mode==4) cvput(fake,"checkedCompilation",xrtValueBool(true));
  if(mode==5) cvput(cvget(range,"end"),"character",xrtValueInt(999999));
  if(mode==6) cvput(edit,"newText",xrtValueString(cvsv("mismatch")));
  if(mode==7) cvput(item,"insertTextFormat",xrtValueInt(2));
  if(mode==8) cvput(cvget(cvget(edit,"range"),"start"),"character",xrtValueInt(0));
  if(mode==9) cvput(item,"filterText",xrtValueString(cvsv("another")));
  if(mode==10) cvput(cvget(item,"data"),"symbolId",xrtValueInt(-1));
  if(mode==11) cvput(fake,"prefix",xrtValueString(cvsv("wrong")));
  CHECK(!cvapply(v,fake) && !v->result && !v->count); xrtValueRelease(fake); fake=NULL; CHECK(cvapply(v,valid) && has(v,"zzResult"));
 } xrtValueRelease(valid); valid=NULL;
 /* A valid-looking response cannot cross a caret, tab, version or identity. */
 fake=xrtValueDeepClone(v->result); CHECK(fake && position(&app,xl,0) && !cvapply(v,fake)); xrtValueRelease(fake); fake=NULL; CHECK(settle(&app) && !v->result);
 CHECK(prepare(&app,xl,local,"zzROther",3) && invoke(&app) && xideEditorOpen(&app,native,&error) && settle(&app) && !v->result); xide_document* c=app.activeDocument;
 CHECK(prepare(&app,c,"ret","ret",3) && key(&app,XUI_KEY_SPACE,XUI_MOD_CTRL) && xuiCodeEditIsCompletionOpen(c->editor) && !v->wanted);
 CHECK(key(&app,XUI_KEY_ENTER,0) && !strcmp(xuiCodeEditGetText(c->editor),"return"));
 CHECK(prepare(&app,xl,local,"zzROther",3) && invoke(&app)); xl->externalState=XIDE_WATCH_UNSTABLE; CHECK(settle(&app) && !v->result); xl->externalState=0;
 CHECK(prepare(&app,xl,"// zzR","zzR",3) && key(&app,XUI_KEY_SPACE,XUI_MOD_CTRL) && idle(&app,180) && !v->wanted && !xuiCodeEditIsCompletionOpen(xl->editor));
 CHECK(prepare(&app,xl,"\"zzR","zzR",3) && key(&app,XUI_KEY_SPACE,XUI_MOD_CTRL) && idle(&app,180) && !v->wanted && !xuiCodeEditIsCompletionOpen(xl->editor));
 const char* overload="func zzPick(int v) as int\n    return v\nend\nfunc zzPick(string v) as int\n    return 1\nend\nfunc main(...) as int\n    zzPi\n    return 0\nend\n";
 CHECK(prepare(&app,xl,overload,"    zzPi",8) && invoke(&app) && v->count==2 && has(v,"zzPick") && strcmp(v->items[0].sDetail,v->items[1].sDetail) && v->items[0].iUserData!=v->items[1].iUserData && xuiCodeEditGetCompletionCount(xl->editor)==2);
 CHECK(key(&app,XUI_KEY_ENTER,0) && strstr(xuiCodeEditGetText(xl->editor),"    zzPick\n") && key(&app,'Z',XUI_MOD_CTRL) && !strcmp(xuiCodeEditGetText(xl->editor),overload));
 const char* shadow="int zzValue = 1\nfunc main(...) as int\n    string zzValue = \"s\"\n    zzVal\n    return 0\nend\n";
 CHECK(prepare(&app,xl,shadow,"    zzVal\n",9) && invoke(&app) && v->count==1 && !strcmp(v->items[0].sDetail,"string"));
 const char* diskQuery="import disk\nfunc main(...) as int\n    auto box = disk.Box()\n    return box.\nend\n";
 CHECK(prepare(&app,xl,diskQuery,"box.",4) && invoke(&app) && has(v,"value") && writeText(disk,"export class Box\n    string value\nend\n"));
 for(int i=0;i<600 && v->wanted;++i) { CHECK(settle(&app)); Sleep(5); } CHECK(!v->result && !v->wanted && !xuiCodeEditIsCompletionOpen(xl->editor));
 CHECK(writeText(disk,helper) && invoke(&app) && has(v,"value") && writeText(disk,"export class Box\n    string value\nend\n"));
 Sleep(1800); CHECK(key(&app,XUI_KEY_ENTER,0) && !strcmp(xuiCodeEditGetText(xl->editor),diskQuery) && !v->result);
 CHECK(writeText(disk,helper) && invoke(&app) && has(v,"value"));
 CHECK(prepare(&app,xl,local,"zzROther",3) && invoke(&app)); xideLanguageClientStop(v->client); v->client=NULL; v->initialized=0; CHECK(key(&app,XUI_KEY_ENTER,0) && !strcmp(xuiCodeEditGetText(xl->editor),local) && !v->result && invoke(&app) && has(v,"zzResult"));
 /* A pending request cannot survive reuse of the editor's document slot. */
 CHECK(prepare(&app,xl,local,"zzROther",3) && key(&app,XUI_KEY_SPACE,XUI_MOD_CTRL)); uint64_t oldId=xl->id;
 CHECK(xideEditorClose(&app,xl,1,&error) && settle(&app) && !v->result); xide_document* reused=xideDocumentNew(&app.documents,"c",&error);
 CHECK(reused==xl && reused->id!=oldId && xideEditorShow(&app,reused)==XUI_OK && idle(&app,150) && !v->wanted && !v->result);
 xide_text_file original={0}; CHECK(xideTextFileRead(entry,&original,&error) && !strcmp(original.text,saved)); xideTextFileClear(&original); CHECK(xideTextFileRead(dep,&original,&error) && !strcmp(original.text,helper)); xideTextFileClear(&original);
 /* An older compiler returns no semantic items, retaining native catalog help. */
 xgeEnd(); began=0; xideAppDestroy(&app); memset(&app,0,sizeof(app)); app.hidden=1; app.noSession=1; app.compilerPath=f->legacy;
 CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1; CHECK(xideEditorOpen(&app,entry,&error) && settle(&app)); xl=app.activeDocument; v=app.completionView;
 CHECK(prepare(&app,xl,"math.sq","math.sq",7) && invoke(&app) && !v->count && xuiCodeEditIsCompletionOpen(xl->editor) && key(&app,XUI_KEY_ENTER,0) && !strcmp(xuiCodeEditGetText(xl->editor),"math.sqrt"));
 printf("completion_view_test passed: %d native assertions; whole-token Enter/Tab and undo, BOM/CRLF/Unicode, dot trigger, real overloads and shadowing, unsaved dependencies, malformed responses, typing refresh, Esc/caret/focus/tab/edit/close/reuse/C rejection, modal/environment, static-name deduplication, concurrent diagnostics, source guard, restart and old-compiler catalog fallback; renders in %s.\n",f->checks,root);
cleanup:
 if(packageTouched) { wchar_t restore[32768]; if(!previousPackage || MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,previousPackage,-1,restore,32768)) SetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",previousPackage ? restore : NULL); } free(previousPackage);
 free(composed); xrtValueRelease(fake); xrtValueRelease(valid); if(began) xgeEnd(); xideAppDestroy(&app); free(root); free(entry); free(dep); free(disk); free(native); free(light); free(dark); free(canonicalChinese); xgeQuit(); return XGE_OK;
}
static DWORD WINAPI worker(void* data) { fixture* f=data; if(!SetThreadDesktop(f->desktop)) return 1; xge_desc_t d={0}; d.iWidth=1600; d.iHeight=1000; d.sTitle="xlang completion verification"; d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; d.iRunMode=XGE_RUN_GAME_LOOP; int result=xgeInit(&d); if(result==XGE_OK) result=xgeRun(verify,f); xgeUnit(); return result==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv) { if(argc!=4 || !xrtDirCreateAll(argv[1])) return 2; fixture f={.directory=argv[1],.compiler=argv[2],.legacy=argv[3]}; wchar_t name[100]; swprintf(name,100,L"XlangCompletionVerification-%lu",GetCurrentProcessId()); f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) return 1; HANDLE t=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1; if(t) { if(WaitForSingleObject(t,120000)!=WAIT_OBJECT_0) { fputs("completion_view_test timed out\n",stderr); ExitProcess(1); } GetExitCodeThread(t,&code); CloseHandle(t); } CloseDesktop(f.desktop); return (int)code; }
