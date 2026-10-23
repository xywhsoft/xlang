#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "ui/task_view.h"
#include "ui/goto_view.h"
#include "services/file_sync.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* White-box acceptance checks augment actual server/CodeEdit integration. */
#include "../src/ui/language_view.c"
typedef struct fixture { const char *root,*compiler;HDESK desktop;int started,failed,checks,legacy; } fixture;
#define CHECK(e) do {++f->checks;if(!(e)){fprintf(stderr,"highlight_view:%d %s; %s; %s; request=%d decode=%d applied=%zu receipt=%d invalid=%d\n",__LINE__,#e,error.message,xideLanguageViewMessage(&app),app.languageView ? app.languageView->highlightRequest : 0,app.languageView ? app.languageView->highlightDecoding : 0,app.languageView ? app.languageView->highlightApplied : 0,app.languageView ? app.languageView->receiptReady : 0,app.languageView ? app.languageView->invalidated : 0);f->failed=1;goto cleanup;}}while(0)
static int writeText(const char* path,const char* text)
{ return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)}); }
static int settle(xide_app* app)
{ if(xuiDispatchPendingEvents(app->context)!=XUI_OK) return 0;xideEditorPoll(app);xideFileSyncPoll(app);xideTaskViewPoll(app);xideLanguageViewPoll(app);xideWorkbenchLayout(app);return xuiLayout(app->context)==XUI_OK && !app->frameError; }
static int point(xide_app* app,xide_document* d,int offset)
{ return xideEditorShow(app,d)==XUI_OK && xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(d->editor),xuiCodeEditGetDocument(d->editor),offset,0)==XUI_OK && xuiSetFocusWidget(app->context,d->editor)==XUI_OK; }
static int key(xide_app* app,int code)
{ return xuiInputKeyDownEx(app->context,code,0,NULL)==XUI_OK && xuiInputKeyUpEx(app->context,code,0,NULL)==XUI_OK && settle(app); }
static int waitHighlights(xide_app* app,size_t count)
{
 uint64_t until=GetTickCount64()+15000;
 do {if(!settle(app)) return 0;xide_language_view* v=app->languageView;
  if(v->highlightApplied==count && !v->highlightAt && !v->highlightRequest && !v->highlightDecoding && !v->highlightResult && v->highlight.document==app->activeDocument) return 1;
  Sleep(5);
 }while(GetTickCount64()<until);return 0;
}
static int marked(xide_document* d,int offset,int id)
{
 xui_code_indicator_t items[16];int count=0;
 if(xuiCodeAnnotationGetIndicatorsAt(xuiCodeEditGetAnnotations(d->editor),offset,items,16,&count)!=XUI_OK) return -1;
 int result=0;for(int i=0;i<count && i<16;++i) if(items[i].iIndicator==id) ++result;return result;
}
/* Inject a reply against the context established by a real completed request.
   Only request ownership/token are renewed; all receipt/model guards remain. */
static int inject(xide_language_view* v,xvalue* message)
{
 xuiCodeAnnotationClearIndicators(xuiCodeEditGetAnnotations(v->highlight.document->editor),HIGHLIGHT_INDICATOR);
 v->highlightApplied=0;v->highlightRequest=++v->nextId;v->highlightToken=++v->highlightSerial;
 for(int i=0;i<300;++i) {int accepted=takeHighlights(v,message);if(accepted>=0) return accepted;Sleep(5);}return 0;
}
static int capture(xide_app* app,const char* root,const char* name)
{
 char* path=xidePathResolve(root,name);xui_rect_i_t clip={0,0,app->viewportWidth,app->viewportHeight};
 int okay=path && settle(app) && xuiUpdate(app->context,0)==XUI_OK && app->proxy.surfaceClear(&app->proxy,app->target,0xffeeeeeeu)==XUI_OK && xuiRender(app->context,app->target,&clip,1)==XUI_OK && xideAppCapture(app,path)==XGE_OK;free(path);return okay;
}
static int run(void* user)
{
 fixture* f=user;if(f->started) return XGE_OK;f->started=1;xide_app app={0};xide_model_error error={0};int began=0;char *entry=NULL,*native=NULL;xvalue* fake=NULL;xstrbuf large;xrtStrBufInit(&large);
 const char* saved="func main(...) as int\n return 0\nend\n";
 const char* body="\xef\xbb\xbf" "func other() as int\n int 结果 = 2\n return 结果\nend\nfunc main(...) as int\n string text = \"😀é\"\n int 结果 = 7\n return 结果\nend\n";
 entry=xidePathResolve(f->root,"入口😀.xl");native=xidePathResolve(f->root,"other.c");CHECK(entry && native && writeText(entry,saved) && writeText(native,"int main(void) {return 0;}\n"));
 app.hidden=1;app.noSession=1;app.compilerPath=f->compiler;CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK);began=1;
 CHECK(xideEditorOpen(&app,entry,&error));xide_document* xl=app.activeDocument;xui_code_document document=xuiCodeEditGetDocument(xl->editor);
 CHECK(xuiCodeDocumentReplace(document,0,xuiCodeDocumentGetLength(document),body)==XUI_OK);
 const char* main=strstr(body,"func main");const char* declaration=strstr(main,"结果");const char* use=strstr(declaration+6,"结果");CHECK(main && declaration && use);
 int start=(int)(declaration-body),end=(int)(use-body);CHECK(point(&app,xl,start) && settle(&app));
 xide_language_view* v=app.languageView;uint32_t version=xuiCodeDocumentGetVersion(document);int undo=xuiCodeDocumentCanUndo(document),redo=xuiCodeDocumentCanRedo(document);
 if(f->legacy) {
  uint64_t until=GetTickCount64()+6000;do {CHECK(settle(&app));if(v->initialized && !v->highlightAt) break;Sleep(5);}while(GetTickCount64()<until);
  CHECK(v->initialized && !v->highlightSupported && !v->highlightRequest && !v->highlightApplied && !v->highlightSerial && !v->busy);
  xide_language_client* client=v->client;uint64_t session=v->highlightSession;Sleep(180);CHECK(settle(&app) && v->client==client && v->highlightSession==session && !v->highlightSerial);goto preserved;
 }
 CHECK(waitHighlights(&app,2) && v->highlightSupported && v->receiptReady && !v->lastSnapshotId && !v->snapshotResult && !v->busy);
 CHECK(marked(xl,start,HIGHLIGHT_INDICATOR)==1 && marked(xl,end,HIGHLIGHT_INDICATOR)==1 && marked(xl,(int)(strstr(body,"结果")-body),HIGHLIGHT_INDICATOR)==0);
 xui_code_annotation_store annotations=xuiCodeEditGetAnnotations(xl->editor);
 xui_code_diagnostic_t diagnostic={.iSize=sizeof(diagnostic),.tRange={3,4},.iSeverity=2,.sMessage="Retained diagnostic"};
 xui_find_options_t find={.iSize=sizeof(find),.sPattern="other",.iFlags=XUI_CODE_SEARCH_CASE_SENSITIVE};int found=0;
 CHECK(xuiCodeAnnotationSetDiagnostics(annotations,&diagnostic,1)==XUI_OK && xuiCodeAnnotationSetIndicator(annotations,1008,XUI_CODE_INDICATOR_UNDERLINE,start,start+6,0,0)==XUI_OK && xuiCodeEditFindAll(xl->editor,&find,&found)==XUI_OK && found==1);
 for(int theme=0;theme<2;++theme) {
  CHECK(xideWorkbenchApplyTheme(&app,theme)==XUI_OK && point(&app,xl,end+3) && waitHighlights(&app,2));
  CHECK(capture(&app,f->root,theme ? "highlight-dark.png" : "highlight-light.png"));
  xui_rect_t viewport=xuiWidgetGetWorldRect(xl->editor);
  printf("{\"theme\":%d,\"editor\":[%d,%d,%d,%d],\"rects\":[",theme,viewport.fX,viewport.fY,viewport.fW,viewport.fH);
  int offsets[2]={start,end};for(int i=0;i<2;++i) {
   xui_rect_t a={0},b={0};CHECK(xuiCodeEditGetTextOffsetRect(xl->editor,offsets[i],&a)==XUI_OK && xuiCodeEditGetTextOffsetRect(xl->editor,offsets[i]+6,&b)==XUI_OK && a.fY==b.fY && b.fX>a.fX);
   printf("%s[%d,%d,%d,%d]",i ? "," : "",a.fX,a.fY,b.fX-a.fX,a.fH);
  }puts("]}");
  suppressHighlights(v);CHECK(settle(&app) && !v->highlightApplied && marked(xl,start,HIGHLIGHT_INDICATOR)==0);
  CHECK(capture(&app,f->root,theme ? "clear-dark.png" : "clear-light.png"));
  Sleep(150);CHECK(settle(&app) && !v->highlightRequest && !v->highlightApplied);
  CHECK(point(&app,xl,start) && waitHighlights(&app,2));
 }
 CHECK(xuiCodeAnnotationGetDiagnosticCount(annotations)==1 && xuiCodeEditGetFindResultCount(xl->editor)==1 && marked(xl,start,1008)==1);
 CHECK(key(&app,XUI_KEY_ESCAPE) && !v->highlightApplied);Sleep(180);CHECK(settle(&app) && !v->highlightRequest && !v->highlightApplied);
 CHECK(point(&app,xl,end) && waitHighlights(&app,2));
 xui_ime_composition_t composition={.iSize=sizeof(composition),.sText="",.bActive=1};
 CHECK(xuiInputImeCompositionEx(app.context,&composition)==XUI_OK && settle(&app) && !v->highlightApplied);Sleep(150);CHECK(settle(&app) && !v->highlightRequest);
 composition.bActive=0;CHECK(xuiInputImeCompositionEx(app.context,&composition)==XUI_OK && settle(&app) && waitHighlights(&app,2));
 CHECK(xideGotoViewOpen(&app) && settle(&app) && xideActionsHasModal(&app) && !v->highlightApplied);
 CHECK(key(&app,XUI_KEY_ESCAPE) && !xideGotoViewIsOpen(&app) && point(&app,xl,start) && waitHighlights(&app,2));
 CHECK(xuiSetFocusWidget(app.context,NULL)==XUI_OK && settle(&app) && !v->highlightApplied);
 CHECK(point(&app,xl,start) && waitHighlights(&app,2));
 xui_code_selection_model selection=xuiCodeEditGetSelection(xl->editor);
 CHECK(xuiCodeSelectionGotoOffset(selection,document,start+6,1)==XUI_OK && settle(&app) && !v->highlightApplied);
 CHECK(point(&app,xl,start) && waitHighlights(&app,2));
 xui_code_selection_t extra={.iSize=sizeof(extra),.iAnchorOffset=0,.iCaretOffset=0};CHECK(xuiCodeSelectionAdd(selection,&extra)==XUI_OK && settle(&app) && !v->highlightApplied);
 CHECK(xuiCodeSelectionGotoOffset(selection,document,start,0)==XUI_OK);
 /* The current snapshot/context is valid, but the user moved the caret before
    a response arrives. Rejected ownership stays with the caller. */
 CHECK(point(&app,xl,start) && waitHighlights(&app,2));
 CHECK(xuiCodeSelectionGotoOffset(selection,document,end,0)==XUI_OK);
 fake=xrtJsonParse(xrtStrView("{\"jsonrpc\":\"2.0\",\"id\":3,\"result\":[]}"));CHECK(fake && takeHighlights(v,fake)==0 && xrtValueType(fake)==XVALUE_OBJECT && !v->highlightApplied);xrtValueRelease(fake);fake=NULL;
 CHECK(point(&app,xl,start) && waitHighlights(&app,2) && xideEditorOpen(&app,native,&error) && settle(&app) && !v->highlightApplied && !v->highlight.document);
 CHECK(point(&app,xl,start) && waitHighlights(&app,2));
 fake=xrtJsonParse(xrtStrView("{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":[{\"range\":{\"start\":{\"line\":0,\"character\":0},\"end\":{\"line\":0,\"character\":1}}},{\"range\":{\"start\":{\"line\":999999,\"character\":0},\"end\":{\"line\":999999,\"character\":1}}}]}"));
 CHECK(fake && inject(v,fake));fake=NULL;CHECK(waitHighlights(&app,0) && marked(xl,0,HIGHLIGHT_INDICATOR)==0 && !v->highlightResult);
 CHECK(point(&app,xl,end) && waitHighlights(&app,2));
 CHECK(xideActionExecute(&app,XIDE_ACTION_CANCEL_CHECK)==XIDE_ACTION_DISPATCHED && settle(&app) && !v->highlightApplied);
 Sleep(180);CHECK(settle(&app) && !v->highlightRequest && !v->highlightApplied);
preserved:
 CHECK(xuiCodeDocumentGetVersion(document)==version && xuiCodeDocumentCanUndo(document)==undo && xuiCodeDocumentCanRedo(document)==redo && !strcmp(xuiCodeEditGetText(xl->editor),body));
 if(!f->legacy) {
  for(int i=0;i<65536;++i) CHECK(xrtStrBufAppend(&large,xrtStrView("//a\n")));
  CHECK(xrtStrBufAppend(&large,xrtStrView(saved)) && xuiCodeDocumentReplace(document,0,xuiCodeDocumentGetLength(document),large.Data)==XUI_OK && point(&app,xl,2) && waitHighlights(&app,0) && readySymbols(&app) && v->highlightToken);
  fake=xrtValueObject();xvalue* ranges=xrtValueArray();CHECK(fake && ranges && put(fake,"jsonrpc",xrtValueString(sv("2.0"))) && put(fake,"id",xrtValueInt(v->nextId+1)) && put(fake,"result",ranges));
  for(int i=0;i<65536;++i) {
   char item[180];snprintf(item,sizeof(item),"{\"range\":{\"start\":{\"line\":%d,\"character\":2},\"end\":{\"line\":%d,\"character\":3}}}",i,i);
   xvalue* range=xrtJsonParse(xrtStrView(item));CHECK(range && xrtValueArrayAppendNew(ranges,range));
  }
  CHECK(inject(v,fake));fake=NULL;version=xuiCodeDocumentGetVersion(document);
  uint64_t until=GetTickCount64()+15000;size_t polls=0,largestStep=0;double peak=0;
  while(v->highlightApplied<65536 && GetTickCount64()<until) {
   size_t before=v->highlightApplied;LARGE_INTEGER a,b;QueryPerformanceCounter(&a);xideLanguageViewPoll(&app);QueryPerformanceCounter(&b);
   double ms=1000.0*(double)(b.QuadPart-a.QuadPart)/(double)v->highlightClock.QuadPart;if(ms>peak) peak=ms;
   size_t step=v->highlightApplied>=before ? v->highlightApplied-before : 0;if(step>largestStep) largestStep=step;
   CHECK(step<=8192);++polls;Sleep(1);
  }
  CHECK(v->highlightApplied==65536 && !v->highlightResult && largestStep<=8192 && polls>=8 && marked(xl,2,HIGHLIGHT_INDICATOR)==1 && marked(xl,4*32768+2,HIGHLIGHT_INDICATOR)==1 && marked(xl,4*65535+2,HIGHLIGHT_INDICATOR)==1);
  CHECK(version==xuiCodeDocumentGetVersion(document) && !strcmp(xuiCodeEditGetText(xl->editor),large.Data) && capture(&app,f->root,"highlight-many.png"));
  printf("{\"capacity\":65536,\"polls\":%zu,\"largestStep\":%zu,\"peakPollMs\":%.6f,\"scope\":\"synthetic complete response against real sync receipt; controller poll only, initial source capture/JSON construction/analysis/render excluded\"}\n",polls,largestStep,peak);
  CHECK(xuiCodeDocumentUndo(document)==XUI_OK && !strcmp(xuiCodeEditGetText(xl->editor),body) && settle(&app) && !v->highlightApplied);
  /* Reconnection must retire a previously held reply instead of interpreting
     its old ID as initialization/a new request in the next session. */
  CHECK(!v->deferredMessage);v->deferredMessage=xrtJsonParse(xrtStrView("{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{}}"));CHECK(v->deferredMessage);v->deferredDiscard=0;v->deferredSession=v->highlightSession;
  /* Isolate retirement from default automatic probing/reconnection. */
   reset(&app);automaticPause(v);uint64_t session=v->highlightSession;CHECK(settle(&app) && v->highlightSession==session && !v->initialized && (!v->deferredMessage || v->deferredDiscard));
 }
 xide_text_file disk={0};CHECK(xideTextFileRead(entry,&disk,&error) && !strcmp(disk.text,saved));xideTextFileClear(&disk);
 printf("highlight_view_test passed: %d native assertions; legacy=%d; cold unsaved Unicode semantic identity, actual BACKGROUND markers, two themes, independent find/diagnostic/indicator ownership, suppression and selection/focus/C/caret guards, source/version/undo preservation.\n",f->checks,f->legacy);
cleanup:
 xrtValueRelease(fake);if(began) xgeEnd();xideAppDestroy(&app);xrtStrBufFree(&large);free(entry);free(native);xgeQuit();return XGE_OK;
}
static DWORD WINAPI worker(void* user)
{ fixture* f=user;if(!SetThreadDesktop(f->desktop)) return 1;xge_desc_t d={0};d.iWidth=1600;d.iHeight=1000;d.sTitle="Highlight integration verification";d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI;d.iRunMode=XGE_RUN_GAME_LOOP;int result=xgeInit(&d);if(result==XGE_OK) result=xgeRun(run,f);xgeUnit();return result==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv)
{
 if(argc<3 || argc>4 || (argc==4 && strcmp(argv[3],"legacy")) || !xrtDirCreateAll(argv[1])) return 2;
 fixture f={.root=argv[1],.compiler=argv[2],.legacy=argc==4};wchar_t name[100];swprintf(name,100,L"HighlightIntegration-%lu",GetCurrentProcessId());f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);if(!f.desktop) return 1;
 HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL);DWORD code=1;if(thread) {if(WaitForSingleObject(thread,120000)!=WAIT_OBJECT_0) ExitProcess(1);GetExitCodeThread(thread,&code);CloseHandle(thread);}CloseDesktop(f.desktop);return (int)code;
}
