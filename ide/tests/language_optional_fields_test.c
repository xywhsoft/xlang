#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "services/language_client.h"
#include "platform/path.h"
#include "catalog_ready.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Replace only the transport queue. The real owner poll, capability parser,
 * sync creation and GUI controllers consume the actual decoded envelopes. */
static xvalue* queued;
static unsigned sent;
static int fixtureSend(xide_language_client* client,const xvalue* message,xide_model_error* error)
{(void)client;(void)message;(void)error;++sent;return 1;}
static xvalue* fixtureNext(xide_language_client* client,size_t* bytes)
{(void)client;xvalue* message=queued;queued=NULL;if(bytes)*bytes=message ? 1024u:0;return message;}
#define xideLanguageClientSend fixtureSend
#define xideLanguageClientNextSized fixtureNext
#include "../src/ui/language_view.c"
#undef xideLanguageClientSend
#undef xideLanguageClientNextSized
typedef struct fixture {const char* suite;HDESK desktop;int started,failed;unsigned checks;} fixture;
#define CHECK(v) do{++f->checks;if(!(v)){fprintf(stderr,"language_optional_fields_test:%d: %s; XRT=%s\n",__LINE__,#v,xrtGetError() ? xrtErrorMessage(xrtGetError()):"none");return 0;}}while(0)
static void seed(int sentinel)
{xrtClearError();if(sentinel)xrtSetErrorInfo(XERR_STATE,"xlang-lsp-test",122,"optional field sentinel");}
static int unchanged(int sentinel)
{const xerror* e=xrtGetError();return sentinel ? e && xrtErrorCode(e)==122 && !strcmp(xrtErrorMessage(e),"optional field sentinel"):e==NULL;}
static xvalue* json(const char* text)
{return xrtJsonParse((xstrview){text,strlen(text)});}
static int probes(fixture* f)
{
 xide_language_view view={0};struct xide_reference_view references={0};view.references=&references;
 const char* caps[]={"{}","{\"referencesProvider\":null}","{\"referencesProvider\":false,\"experimental\":{\"referenceInputProof\":true}}","{\"referencesProvider\":true}","{\"referencesProvider\":true,\"experimental\":{\"referenceInputProof\":true}}","{\"referencesProvider\":{},\"experimental\":{\"referenceInputProof\":true}}","{\"referencesProvider\":[],\"experimental\":{\"referenceInputProof\":true}}","{\"referencesProvider\":\"true\",\"experimental\":{\"referenceInputProof\":true}}"};
 for(int sentinel=0;sentinel<2;++sentinel)for(size_t i=0;i<sizeof(caps)/sizeof(*caps);++i){
  xvalue* cap=json(caps[i]);CHECK(cap);seed(sentinel);referencesCapabilities(&view,cap);CHECK(references.supported==(i==4 || i==5) && unchanged(sentinel));xrtValueRelease(cap);
 }
 const char* groups[]={"{}","{\"documents\":null}","{\"documents\":{\"path\":\"D:/file.xl\"}}","{\"documents\":true}","{\"documents\":[]}","{\"documents\":[{\"path\":\"D:/file.xl\"}]}"};
 for(int sentinel=0;sentinel<2;++sentinel)for(size_t i=0;i<sizeof(groups)/sizeof(*groups);++i){
  view.snapshotResult=json(groups[i]);CHECK(view.snapshotResult);seed(sentinel);const xvalue* group=snapshotGroup(&view,"D:/file.xl");CHECK((group!=NULL)==(i==5) && unchanged(sentinel));xrtValueRelease(view.snapshotResult);view.snapshotResult=NULL;
 }
 for(int sentinel=0;sentinel<2;++sentinel){int n=17;seed(sentinel);CHECK(!boundedString(NULL,16) && !automaticText(NULL) && !number(NULL,&n) && n==17 && unchanged(sentinel));}
 xrtClearError();return 1;
}
static int receipts(fixture* f)
{
 const char* cases[]={NULL,"null","17","[]","{}","{\"available\":0,\"attributes\":0,\"device\":\"0\",\"identity\":\"0\",\"modifiedUs\":6001001,\"changedUs\":\"7001002\"}","{\"available\":0,\"attributes\":0,\"device\":\"0\",\"identity\":\"0\",\"modifiedUs\":\"6001001\",\"changedMs\":\"62135596807001\"}","{\"available\":0,\"attributes\":0,\"device\":\"0\",\"identity\":\"0\",\"modifiedUs\":\"6001001\",\"changedUs\":\"7001002\"}","{\"available\":0,\"attributes\":0,\"device\":\"0\",\"identity\":\"0\",\"modifiedMs\":\"62135596806001\",\"changedMs\":\"62135596807001\"}"};
 xfileinfo info={0};info.Available=XFILE_INFO_MODIFY_TIME|XFILE_INFO_CHANGE_TIME;info.Modified=6001001;info.Changed=7001002;
 for(int sentinel=0;sentinel<2;++sentinel)for(size_t i=0;i<sizeof(cases)/sizeof(*cases);++i){
  xvalue* value=cases[i] ? json(cases[i]):NULL;CHECK(!cases[i] || value);seed(sentinel);
  CHECK(xideReceiptMetadataSchema(value)==(i==7 ? 1:i==8 ? 2:0) && unchanged(sentinel));
  CHECK(xideReceiptMetadataTimesMatch(value,&info)==(i==7 || i==8) && unchanged(sentinel));xrtValueRelease(value);
 }
 seed(1);CHECK(!xideReceiptField(NULL,"missing") && unchanged(1));xrtClearError();return 1;
}
static int polls(fixture* f,xide_app* app)
{
 const char* extra[]={"",",\"textDocumentSync\":1",",\"textDocumentSync\":{\"change\":2},\"referencesProvider\":{},\"documentHighlightProvider\":{}",",\"textDocumentSync\":null",",\"textDocumentSync\":\"wrong\"",",\"textDocumentSync\":{\"change\":2},\"referencesProvider\":true,\"documentHighlightProvider\":true",",\"textDocumentSync\":{\"change\":2},\"referencesProvider\":\"true\",\"documentHighlightProvider\":[]"};
 for(size_t i=0;i<sizeof(extra)/sizeof(*extra);++i){
  reset(app);char body[2048];int n=snprintf(body,sizeof(body),"{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{\"capabilities\":{\"positionEncoding\":\"utf-16\",\"experimental\":{\"savedEntryDiagnostics\":true,\"referenceInputProof\":true}%s}}}",extra[i]);
  CHECK(n>0 && (size_t)n<sizeof(body));queued=json(body);CHECK(queued);sent=0;xrtClearError();xideLanguageViewPoll(app);
  CHECK(!queued && !xrtGetError());xide_language_view* v=app->languageView;
  CHECK(v->initialized==(i!=3 && i!=4));
  if(v->initialized){CHECK(sent==1 && v->syncKind==(i<2 ? 1:2));CHECK(v->references->supported==(i==2 || i==5) && v->highlightSupported==(i==2 || i==5));}
 }
 return 1;
}
static int verify(void* user)
{
 fixture* f=user;if(f->started)return XGE_OK;f->started=1;xide_app app={0};int began=0,okay=1;
 if(!strcmp(f->suite,"all") || !strcmp(f->suite,"probes"))okay=probes(f);
 if(okay && (!strcmp(f->suite,"all") || !strcmp(f->suite,"receipts")))okay=receipts(f);
 if(okay && (!strcmp(f->suite,"all") || !strcmp(f->suite,"polls"))){
  app.hidden=1;app.noSession=1;app.disableAutomaticDiagnostics=1;
  okay=xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK;began=okay;
  if(okay)okay=xideTestCatalogReady(&app) && polls(f,&app);
 }
 if(began){xgeEnd();}xideAppDestroy(&app);xrtValueRelease(queued);queued=NULL;xrtClearError();f->failed=!okay;
 if(okay)printf("language_optional_fields_test passed: %u checks; %s, actual SDK errors, capabilities, snapshot groups and timestamp receipts\n",f->checks,f->suite);
 xgeQuit();return XGE_OK;
}
static DWORD WINAPI worker(void* user)
{
 fixture* f=user;if(!SetThreadDesktop(f->desktop))return 1;xge_desc_t d={0};d.iWidth=1280;d.iHeight=800;d.sTitle="xlang optional fields";d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI;d.iRunMode=XGE_RUN_GAME_LOOP;
 int result=xgeInit(&d);if(result==XGE_OK)result=xgeRun(verify,f);xgeUnit();return result==XGE_OK && f->started && !f->failed ? 0:1;
}
int main(int argc,char** argv)
{
 if(argc!=2 || (strcmp(argv[1],"all") && strcmp(argv[1],"probes") && strcmp(argv[1],"receipts") && strcmp(argv[1],"polls"))){return 2;}
 fixture f={.suite=argv[1]};wchar_t name[100];swprintf(name,100,L"XlangOptionalFields-%lu",GetCurrentProcessId());
 f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);if(!f.desktop)return 1;
 HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL);DWORD code=1;if(thread){if(WaitForSingleObject(thread,60000)!=WAIT_OBJECT_0)ExitProcess(1);GetExitCodeThread(thread,&code);CloseHandle(thread);}CloseDesktop(f.desktop);return (int)code;
}
