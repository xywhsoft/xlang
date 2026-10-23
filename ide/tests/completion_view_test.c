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
/* Controlled malformed/late replies use the same parser as the production
   worker. Synchronous parsing is test-only; the UI handler never does this. */
static int applyFixtureResult(xide_completion_view* v,const xvalue* value)
{
 completion_input* target=NULL;for(size_t i=0;i<v->inputCount;++i) if(v->inputs[i].document==v->document) target=v->inputs+i;
 if(!target) return 0;
 xls_text* text=xlsSyncRetainText(v->sync,target->uri,target->wireVersion);xvalue* message=xrtValueObject();
 int okay=text && message && cvput(message,"jsonrpc",xrtValueString(cvsv("2.0"))) && cvput(message,"id",xrtValueInt(v->request)) && cvput(message,"result",xrtValueRetain((xvalue*)value));
 xide_completion_result* decoded=okay ? xideCompletionDecode(text,message,(xide_completion_expected){v->request,target->wireVersion,v->offset},NULL,NULL) : NULL;
 xlsTextRelease(text);xrtValueRelease(message);
 uint64_t until=GetTickCount64()+5000;while(!cvflushResult(v) && GetTickCount64()<until) Sleep(1);
 okay=cvapply(v,decoded);if(!okay) xideCompletionResultRelease(decoded);return okay;
}
static xide_completion_result* holdDecoded(xide_completion_view* v,const xvalue* value)
{
 cvclearResult(v);uint64_t until=GetTickCount64()+5000;while(!cvflushResult(v) && GetTickCount64()<until) Sleep(1);
 if(v->decoded) return NULL;
 completion_input* target=NULL;for(size_t i=0;i<v->inputCount;++i) if(v->inputs[i].document==v->document) target=v->inputs+i;
 if(!target) return NULL;
 xls_text* text=xlsSyncRetainText(v->sync,target->uri,target->wireVersion);xvalue* message=xrtValueObject();
 int okay=text && message && cvput(message,"jsonrpc",xrtValueString(cvsv("2.0"))) && cvput(message,"id",xrtValueInt(v->request)) && cvput(message,"result",xrtValueRetain((xvalue*)value));
 v->ready=0;v->decodeToken=++v->decodeSerial;v->decodeSession=v->session;
 if(!okay || !xideCompletionDecoderSubmit(v->decoder,v->decodeToken,message,text,(xide_completion_expected){v->request,target->wireVersion,v->offset})) {xlsTextRelease(text);xrtValueRelease(message);return NULL;}
 v->decoding=1;until=GetTickCount64()+10000;
 do {xide_completion_result* r=xideCompletionDecoderTake(v->decoder);if(r) return r;Sleep(1);}while(GetTickCount64()<until);return NULL;
}
typedef struct fixture_retire {xatomic32 calls,thread,timedOut;HANDLE entered,release;} fixture_retire;
static void fixtureDropped(ptr handle,ptr user)
{
 fixture_retire* f=user;if(handle!=f) return;
 if(f->entered) {SetEvent(f->entered);if(WaitForSingleObject(f->release,10000)!=WAIT_OBJECT_0) xrtAtomic32Store(&f->timedOut,1,XMEMORY_RELEASE);}
 xrtAtomic32Store(&f->thread,(int32)GetCurrentThreadId(),XMEMORY_RELAXED);xrtAtomic32FetchAdd(&f->calls,1,XMEMORY_RELEASE);
}
static xvalue* fixtureOwned(fixture_retire* state)
{static const xvaluehandleops ops={.Drop=fixtureDropped};ptr owned=state;return xrtValueHandleTake(&owned,&ops,state);}
static int waitFixtureDrop(fixture_retire* state)
{uint64_t until=GetTickCount64()+10000;while(!xrtAtomic32Load(&state->calls,XMEMORY_ACQUIRE) && GetTickCount64()<until) Sleep(1);return xrtAtomic32Load(&state->calls,XMEMORY_ACQUIRE)==1;}
/* Complete a real plan without queue admission, so stale-context rejection is
   deterministic rather than depending on whether the server wins a race. */
static int holdSync(xide_completion_view* v)
{
 if(!v->pending || !v->wanted || !v->initialized || !cvretireSync(v)) return 0;
 v->pending=0;if(!cvbegin(v)) return 0;
 xide_sync_input inputs[64];for(size_t i=0;i<v->inputCount;++i) inputs[i]=(xide_sync_input){v->inputs[i].uri,v->inputs[i].textValue,v->inputs[i].id,v->inputs[i].version};
 if(xideSyncPlannerSubmit(v->syncPlanner,v->syncToken,v->sync,inputs,v->inputCount)!=1) return 0;
 v->syncSubmitted=1;v->syncDeadline=0;uint64_t until=GetTickCount64()+10000;
 do {v->syncReady=xideSyncPlannerTake(v->syncPlanner);if(v->syncReady) return v->syncReady->valid && v->syncReady->token==v->syncToken;Sleep(1);}while(GetTickCount64()<until);return 0;
}
static int prefixCases(xui_code_document document)
{
 char text[270];memset(text,'a',sizeof(text));
 for(int n=0;n<=260;++n) {
  text[n]=0;if(xuiCodeDocumentReplace(document,0,xuiCodeDocumentGetLength(document),text)!=XUI_OK) return 0;
  for(int at=0;at<=n;++at) if(cvprefixAllowed(document,at)!=(at<=255)) return 0;
  if(cvprefixAllowed(document,-1) || cvprefixAllowed(document,n+1)) return 0;
  text[n]='a';
 }
 for(int i=0;i<84;++i) memcpy(text+i*3,"中",3);
 memcpy(text+252,"abc",4);
 if(xuiCodeDocumentReplace(document,0,xuiCodeDocumentGetLength(document),text)!=XUI_OK || !cvprefixAllowed(document,255)) return 0;
 text[255]='d';text[256]=0;
 if(xuiCodeDocumentReplace(document,0,xuiCodeDocumentGetLength(document),text)!=XUI_OK || cvprefixAllowed(document,256)) return 0;
 text[255]='.';
 return xuiCodeDocumentReplace(document,0,xuiCodeDocumentGetLength(document),text)==XUI_OK && cvprefixAllowed(document,256);
}
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
 fixture* f=user; if(f->started) return XGE_OK; f->started=1; xide_app app={0}; xide_model_error error={0}; int began=0; xvalue *fake=NULL,*valid=NULL; xui_code_completion_item_t* composed=NULL; xide_completion_result* held=NULL;fixture_retire blocker={0},queued={0},delayed={0},oldMessage={0};
 char *root=NULL,*entry=NULL,*dep=NULL,*disk=NULL,*native=NULL,*light=NULL,*dark=NULL,*canonicalChinese=NULL;
 char* previousPackage=NULL; int packageTouched=0; CHECK(xideLanguageCheckPackagePath(&previousPackage,&error));
 char unique[80]; snprintf(unique,sizeof(unique),"completion-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64()); root=xidePathResolve(f->directory,unique); entry=xidePathResolve(root,"入口😀.xl"); dep=xidePathResolve(root,"dep.xl"); disk=xidePathResolve(root,"disk.xl"); native=xidePathResolve(root,"helper.c"); light=xidePathResolve(root,"completion-light.png"); dark=xidePathResolve(root,"completion-dark.png");
 const char *saved="func main(...) as int\n    return 0\nend\n",*local="func main(...) as int\n    int zzResult = 7\n    return zzROther\nend\n",*chinese="\xef\xbb\xbf" "func main(...) as int\r\n    string 文本 = \"😀\"; int 结果 = 7\r\n    return 结后缀\r\nend\r\n",*klass="class Box\n    int value\n    func getValue() as int\n        return self.value\n    end\nend\nfunc main(...) as int\n    auto box = Box()\n    return box\nend\n",*imported="import dep\nfunc main(...) as int\n    auto box = dep.Box()\n    return box.\nend\n",*helper="export class Box\n    int value\nend\n";
 CHECK(root && entry && dep && disk && native && light && dark && xrtDirCreateAll(root) && writeText(entry,saved) && writeText(dep,helper) && writeText(disk,helper) && writeText(native,"int helper(void) { return 1; }\n"));
 app.hidden=1; app.noSession=1; app.compilerPath=f->compiler; CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1;
 CHECK(xideEditorOpen(&app,entry,&error) && settle(&app)); xide_document* xl=app.activeDocument; xide_completion_view* v=app.completionView;
 CHECK(prepare(&app,xl,local,"zzROther",3) && invoke(&app) && v->count==1 && has(v,"zzResult") && !strcmp(v->items[0].sDetail,"int") && xuiCodeEditIsCompletionOpen(xl->editor));
 CHECK(v->start==(int)(strstr(local,"zzROther")-local) && v->end==v->start+8 && capture(&app,light));
 /* All commits in one input batch keep only the final caret intent. No
    source capture or protocol query runs inside the input handler. */
 const char* beforeBurst="func main(...) as int\n    int zzResult = 7\n    return zzOther\nend\n";
 CHECK(prepare(&app,xl,beforeBurst,"zzOther",2));cvdismiss(v);CHECK(xuiInputSetModifiers(app.context,0)==XUI_OK);
 uint64_t burstSerial=v->syncSerial;int burstQuery=v->nextId;
 CHECK(xuiInputTextEx(app.context,'R',NULL)==XUI_OK && xuiDispatchPendingEvents(app.context)==XUI_OK);
 CHECK(xuiInputTextEx(app.context,'e',NULL)==XUI_OK && xuiDispatchPendingEvents(app.context)==XUI_OK);
 CHECK(xuiInputKeyDownEx(app.context,XUI_KEY_BACKSPACE,0,NULL)==XUI_OK && xuiDispatchPendingEvents(app.context)==XUI_OK && xuiInputKeyUpEx(app.context,XUI_KEY_BACKSPACE,0,NULL)==XUI_OK && xuiDispatchPendingEvents(app.context)==XUI_OK);
 CHECK(!strcmp(xuiCodeEditGetText(xl->editor),local) && v->repeatEditor==xl->editor && v->repeatId==xl->id && v->repeatOffset==(int)(strstr(local,"zzROther")-local)+3 && !v->wanted && !v->pending && !v->request && v->syncSerial==burstSerial && v->nextId==burstQuery);
 CHECK(waitReady(&app) && has(v,"zzResult") && v->count==1 && v->syncSerial==burstSerial+1 && v->nextId==burstQuery+1 && v->version==xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xl->editor)));
 /* Escape before the first owner poll cancels that intent without analysis. */
 CHECK(prepare(&app,xl,beforeBurst,"zzOther",2));cvdismiss(v);CHECK(xuiInputSetModifiers(app.context,0)==XUI_OK);burstSerial=v->syncSerial;burstQuery=v->nextId;
 CHECK(xuiInputTextEx(app.context,'R',NULL)==XUI_OK && xuiDispatchPendingEvents(app.context)==XUI_OK && v->repeatEditor==xl->editor);
 CHECK(xuiInputKeyDownEx(app.context,XUI_KEY_ESCAPE,0,NULL)==XUI_OK && xuiDispatchPendingEvents(app.context)==XUI_OK && xuiInputKeyUpEx(app.context,XUI_KEY_ESCAPE,0,NULL)==XUI_OK && xuiDispatchPendingEvents(app.context)==XUI_OK);
 CHECK(settle(&app) && !v->wanted && !v->repeatEditor && !v->result && v->syncSerial==burstSerial && v->nextId==burstQuery);
 CHECK(prepare(&app,xl,local,"zzROther",3) && invoke(&app));
 /* Repeating a query keeps the connection, wire documents and captured body. */
 xls_sync_stats syncBefore={0},syncAfter={0}; xlsSyncStats(v->sync,&syncBefore);
 xide_language_client* retainedClient=v->client; const char* retainedText=v->inputs[0].text;
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
  CHECK(!applyFixtureResult(v,fake) && !v->result && !v->count); xrtValueRelease(fake); fake=NULL; CHECK(applyFixtureResult(v,valid) && has(v,"zzResult"));
 } xrtValueRelease(valid); valid=NULL;
 /* Already decoded data still cannot cross a real caret/dependency/session
    change. The owner acceptance path is the same one used by Poll. */
 valid=xrtValueDeepClone(v->result);CHECK(valid && (held=holdDecoded(v,valid)) && position(&app,xl,0));cvacceptDecoded(v,held,1);held=NULL;
 CHECK(!v->wanted && !v->result && !v->decoding);
 CHECK(prepare(&app,xl,local,"zzROther",3) && invoke(&app));xrtValueRelease(valid);valid=xrtValueDeepClone(v->result);
 CHECK(valid && (held=holdDecoded(v,valid)) && replace(dependency,"export class Box\n string value\nend\n"));cvacceptDecoded(v,held,1);held=NULL;
 CHECK(!v->wanted && !v->result && replace(dependency,helper));
 CHECK(prepare(&app,xl,local,"zzROther",3) && invoke(&app));xrtValueRelease(valid);valid=xrtValueDeepClone(v->result);
 CHECK(valid && (held=holdDecoded(v,valid)));cvstopClient(v);xideCompletionViewRequest(&app,xl->editor,(int)(strstr(local,"zzROther")-local)+3,1);cvacceptDecoded(v,held,1);held=NULL;
 CHECK(v->wanted && !v->ready && !v->result && waitReady(&app) && has(v,"zzResult"));
 /* A previous connection's deferred id=1 must be garbage, not a new init. */
 CHECK(!v->deferredMessage);fake=xrtJsonParse(cvsv("{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{\"capabilities\":{}}}"));
 CHECK(fake && cvput(fake,"testLifetime",fixtureOwned(&oldMessage)));v->deferredMessage=fake;fake=NULL;v->deferredDiscard=0;v->deferredSession=v->session;
 cvstopClient(v);CHECK(invoke(&app) && has(v,"zzResult") && waitFixtureDrop(&oldMessage) && (DWORD)xrtAtomic32Load(&oldMessage.thread,XMEMORY_RELAXED)!=GetCurrentThreadId());
 /* Hold the recycler while one more message fills its pending slot. Enter
    must consume no edit rather than skip unread/deferred invalidations. */
 CHECK(v->result && v->collectEligible && !v->deferredMessage);blocker.entered=CreateEventW(NULL,TRUE,FALSE,NULL);blocker.release=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(blocker.entered && blocker.release);
 fake=fixtureOwned(&blocker);CHECK(fake);uint64_t discardUntil=GetTickCount64()+5000;
 while(fake && GetTickCount64()<discardUntil) {if(xideCompletionDecoderDiscard(v->decoder,fake)) fake=NULL;else Sleep(1);}
 CHECK(!fake && WaitForSingleObject(blocker.entered,10000)==WAIT_OBJECT_0);
 fake=fixtureOwned(&queued);CHECK(fake && xideCompletionDecoderDiscard(v->decoder,fake));fake=NULL;
 v->deferredMessage=fixtureOwned(&delayed);v->deferredDiscard=1;v->deferredSession=v->session;CHECK(v->deferredMessage);
 CHECK(key(&app,XUI_KEY_ENTER,0) && !strcmp(xuiCodeEditGetText(xl->editor),local) && !xrtAtomic32Load(&blocker.timedOut,XMEMORY_ACQUIRE));
 SetEvent(blocker.release);CHECK(idle(&app,250) && waitFixtureDrop(&blocker) && waitFixtureDrop(&queued) && waitFixtureDrop(&delayed) && (DWORD)xrtAtomic32Load(&delayed.thread,XMEMORY_RELAXED)!=GetCurrentThreadId());
 CHECK(prepare(&app,xl,local,"zzROther",3) && invoke(&app));xrtValueRelease(valid);valid=NULL;
 /* A completed encoded batch must not update local wire state after an edit
    to either input or after a connection reset. Recovery uses the latest body. */
 const char* newer="func main(...) as int\n    int zzRecent = 7\n    return zzROther\nend\n";
 const char* rootUri=NULL;for(size_t i=0;i<v->inputCount;++i) if(v->inputs[i].document==xl) rootUri=v->inputs[i].uri;
 CHECK(rootUri);char* retainedUri=cvcopy(rootUri);CHECK(retainedUri);int priorWire=xlsSyncVersion(v->sync,retainedUri);
 CHECK(prepare(&app,xl,newer,"zzROther",3));xideCompletionViewRequest(&app,xl->editor,(int)(strstr(newer,"zzROther")-newer)+3,1);
 CHECK(holdSync(v) && !v->request && !v->ready && xlsSyncVersion(v->sync,retainedUri)==priorWire);
 CHECK(replace(xl,local));cvpollSync(v);CHECK(!v->wanted && !v->syncToken && !v->ready && xlsSyncVersion(v->sync,retainedUri)==priorWire);
 CHECK(prepare(&app,xl,newer,"zzROther",3));xideCompletionViewRequest(&app,xl->editor,(int)(strstr(newer,"zzROther")-newer)+3,1);
 CHECK(holdSync(v) && replace(dependency,"export class Box\n string value\nend\n"));cvpollSync(v);
 CHECK(!v->wanted && !v->syncToken && xlsSyncVersion(v->sync,retainedUri)==priorWire);
 CHECK(replace(dependency,helper) && prepare(&app,xl,newer,"zzROther",3));xideCompletionViewRequest(&app,xl->editor,(int)(strstr(newer,"zzROther")-newer)+3,1);
 CHECK(holdSync(v));uint64_t oldSession=v->session,oldToken=v->syncToken;cvstopClient(v);CHECK(v->session!=oldSession && !v->sync && !v->syncToken && !v->client);
 CHECK(invoke(&app) && v->syncSerial>oldToken && has(v,"zzRecent") && !has(v,"zzResult") && xlsSyncVersion(v->sync,retainedUri)==1);
 cvdismiss(v);CHECK(prefixCases(xuiCodeEditGetDocument(xl->editor)));free(retainedUri);
 CHECK(prepare(&app,xl,local,"zzROther",3) && invoke(&app));
 /* A valid-looking response cannot cross a caret, tab, version or identity. */
 fake=xrtValueDeepClone(v->result); CHECK(fake && position(&app,xl,0) && !applyFixtureResult(v,fake)); xrtValueRelease(fake); fake=NULL; CHECK(settle(&app) && !v->result);
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
 /* Native analysis is independent of the selected execution compiler. An
    older external LS still keeps its existing catalog fallback behavior. */
 xgeEnd(); began=0; xideAppDestroy(&app); memset(&app,0,sizeof(app)); app.hidden=1; app.noSession=1; app.compilerPath=f->legacy;
 CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1; CHECK(xideEditorOpen(&app,entry,&error) && settle(&app)); xl=app.activeDocument; v=app.completionView;
 CHECK(prepare(&app,xl,local,"zzROther",3) && invoke(&app));
 if(v->nativeBackend) { CHECK(has(v,"zzResult") && v->count==1); }
 else { CHECK(!v->count); }
 CHECK(prepare(&app,xl,"math.sq","math.sq",7) && invoke(&app) && !v->count && xuiCodeEditIsCompletionOpen(xl->editor) && key(&app,XUI_KEY_ENTER,0) && !strcmp(xuiCodeEditGetText(xl->editor),"math.sqrt"));
 printf("completion_view_test passed: %d native assertions; native-backend=%d; whole-token Enter/Tab and undo, BOM/CRLF/Unicode, dot trigger, real overloads and shadowing, unsaved dependencies, malformed responses, typing refresh, Esc/caret/focus/tab/edit/close/reuse/C rejection, modal/environment, static-name deduplication, concurrent diagnostics, source guard, restart and old-compiler compatibility; renders in %s.\n",f->checks,v->nativeBackend,root);
cleanup:
 if(blocker.release) SetEvent(blocker.release);
 if(packageTouched) { wchar_t restore[32768]; if(!previousPackage || MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,previousPackage,-1,restore,32768)) SetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",previousPackage ? restore : NULL); } free(previousPackage);
 free(composed); xrtValueRelease(fake); xrtValueRelease(valid);xideCompletionResultRelease(held); if(began) xgeEnd(); xideAppDestroy(&app);if(blocker.entered) CloseHandle(blocker.entered);if(blocker.release) CloseHandle(blocker.release); free(root); free(entry); free(dep); free(disk); free(native); free(light); free(dark); free(canonicalChinese); xgeQuit(); return XGE_OK;
}
static DWORD WINAPI worker(void* data) { fixture* f=data; if(!SetThreadDesktop(f->desktop)) return 1; xge_desc_t d={0}; d.iWidth=1600; d.iHeight=1000; d.sTitle="xlang completion verification"; d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; d.iRunMode=XGE_RUN_GAME_LOOP; int result=xgeInit(&d); if(result==XGE_OK) result=xgeRun(verify,f); xgeUnit(); return result==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv) { if(argc!=4 || !xrtDirCreateAll(argv[1])) return 2; fixture f={.directory=argv[1],.compiler=argv[2],.legacy=argv[3]}; wchar_t name[100]; swprintf(name,100,L"XlangCompletionVerification-%lu",GetCurrentProcessId()); f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) return 1; HANDLE t=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1; if(t) { if(WaitForSingleObject(t,120000)!=WAIT_OBJECT_0) { fputs("completion_view_test timed out\n",stderr); ExitProcess(1); } GetExitCodeThread(t,&code); CloseHandle(t); } CloseDesktop(f.desktop); return (int)code; }
