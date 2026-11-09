/* Exercise the actual background adapter and observe its host hash work.
 * Native compiler validation remains linked normally and still verifies all
 * source, resolution and context inputs. */
#include "app_xrt.h"
static size_t hashBytes;
static bool captureHash(const void* data,size_t size,void* digest)
{hashBytes+=size;return xrtSha256(data,size,digest);}
#define xrtSha256 captureHash
#include "../src/services/language_snapshot.c"
#undef xrtSha256
#include "platform/text_file.h"
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"declaration_capture_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static int writeText(const char* path,const char* text)
{return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)});}
static int ready(xide_language_check* q)
{char message[512]={0};for(unsigned i=0;i<15000;++i){enum xide_check_state state=xideLanguageCheckState(q,message,sizeof(message));if(state!=XIDE_CHECK_RUNNING){if(state!=XIDE_CHECK_READY)fprintf(stderr,"capture state %d: %s\n",state,message);return state==XIDE_CHECK_READY;}xrtSleep(1);}return 0;}
static int named(xide_semantic_symbols* symbols,const char* path,const char* name,int kind)
{for(size_t i=0;i<xideSemanticSymbolsCount(symbols);++i){xide_semantic_symbol_view row;if(!xideSemanticSymbolAt(symbols,i,&row))return 0;if(xidePathLexicalEqual(row.path,path) && !strcmp(row.name,name))return row.kind==kind;}return 0;}
static int positions(xide_semantic_symbols* symbols)
{
 for(size_t i=0;i<xideSemanticSymbolsCount(symbols);++i){xide_semantic_symbol_view row;CHECK(xideSemanticSymbolAt(symbols,i,&row));xide_text_file file={0};xide_model_error error={0};CHECK(xideTextFileRead(row.path,&file,&error));int a=0,b=0;CHECK(xideLspPositionToOffset(file.text,row.selectionStart.line,row.selectionStart.character,&a) && xideLspPositionToOffset(file.text,row.selectionEnd.line,row.selectionEnd.character,&b));CHECK((size_t)(b-a)==strlen(row.name) && !memcmp(file.text+a,row.name,(size_t)(b-a)));xideTextFileClear(&file);}
 return 1;
}
static DWORD WINAPI reader(void* data)
{xide_semantic_symbols* symbols=data;for(unsigned n=0;n<1000;++n)for(size_t i=0;i<xideSemanticSymbolsCount(symbols);++i){xide_semantic_symbol_view row;if(!xideSemanticSymbolAt(symbols,i,&row) || !row.name || !*row.name)return 1;}xideSemanticSymbolsDestroy(symbols);return 0;}
static int run(void)
{
 char* dir=xrtPathAbs("build/declaration-capture");CHECK(dir && xrtDirCreateAll(dir));char *entry=xrtPathJoin(dir,"main.xl"),*dep=xrtPathJoin(dir,"shared.xl"),*compiler=xrtPathJoin(dir,"absent-xl.exe");CHECK(entry && dep && compiler);
 const char* body="\xef\xbb\xbfimport shared\r\nfunc main(...) as int\r\n return shared.compute(7)\r\nend\r\n";
 const char* shared="\xef\xbb\xbf/* 中文😀 */ class Payload\r\n int number\r\nend\r\nexport func compute(int value) as int\r\n return value+1\r\nend\r\n";
 const char* changed="\xef\xbb\xbf/* 中文😀 */ class Payload\r\n int total\r\nend\r\nexport func compute(int value) as int\r\n return value+2\r\nend\r\n";
 CHECK(writeText(entry,body) && writeText(dep,shared));
 xls_context_spec spec={.kind=XLS_CONTEXT_XLANG,.entry=entry,.directory=dir,.analysisProfile="xlang-ls/compiler-xrt-v1",.targetKind="exe"};
 xls_context* context=xlsContextCreate(&spec,16u*1024u*1024u,NULL);xide_toolchains* cache=xideToolchainsCreate();xls_worker* actor=xlsWorkerCreate(NULL,8);CHECK(context && cache && actor);
 xide_language_analysis_options options={.nativeBackend=1};xide_model_error error={0};xide_declaration_capture_work work={0};xide_semantic_symbols *symbols=NULL,*saved=NULL;
 CHECK(!xideLanguageCheckCaptureDeclarations(NULL,&symbols,&work) && !symbols && !work.workerThread && !xideSemanticSymbolsRetain(NULL));
 CHECK(!xideLanguageCheckStartDeclarationCapture(NULL,cache,compiler,actor,NULL,0,&options,&error));
 CHECK(!xideLanguageCheckStartDeclarationCapture(context,cache,compiler,NULL,NULL,0,&options,&error));
 CHECK(!xideLanguageCheckStartDeclarationCapture(context,cache,compiler,actor,NULL,1,&options,&error));
 CHECK(!xideLanguageCheckStartConfigured(context,cache,compiler,actor,NULL,0,NULL,NULL,0,0,&options,&error));
 uint64_t builds=0,hits=0;
 for(unsigned pass=0;pass<2;++pass){
  hashBytes=0;xide_language_check* q=xideLanguageCheckStartDeclarationCapture(context,cache,compiler,actor,NULL,0,&options,&error);CHECK(q && ready(q));
  CHECK(q->snapshotCount==0 && !q->semanticTarget && !q->definitions && !q->deferredNavigation && !q->symbolNavigation && !q->referenceShard);
  CHECK(xideLanguageCheckGuardState(q,NULL,0)==XIDE_GUARD_RUNNING && !xideLanguageCheckWorkerFinished(q));
  CHECK(xideLanguageCheckCaptureDeclarations(q,&symbols,&work) && work.workerThread && work.workerThread!=xrtThreadCurrentId() && work.projection.reusedValidation && !work.projection.fullValidationFallback);
  CHECK(named(symbols,entry,"main",12) && named(symbols,dep,"compute",12) && named(symbols,dep,"Payload",5) && named(symbols,dep,"number",8) && positions(symbols));
  if(!pass){builds=work.analysis.builds;hits=work.analysis.hits;CHECK(builds==1);saved=xideSemanticSymbolsRetain(symbols);}else CHECK(work.analysis.builds==builds && work.analysis.hits>hits);
  xvalue* result=xideLanguageCheckResult(q);CHECK(result && !get(result,"documents") && !get(result,"analysis") && !get(result,"diagnostics"));bool open=true,navigation=true;CHECK(xrtValueGetBool(get(result,"entryOpen"),&open) && !open && xrtValueGetBool(get(result,"semanticDefinitions"),&navigation) && !navigation);xrtValueRelease(result);
  xide_language_input_guard* guard=xideLanguageCheckCaptureInputGuard(q,&error);CHECK(guard);xideLanguageCheckDestroy(q);CHECK(xideLanguageInputGuardValidate(guard,NULL,NULL,&error));xideLanguageInputGuardRelease(guard);xideSemanticSymbolsDestroy(symbols);symbols=NULL;
 }
 /* An unopened entry uses real dependency overlays, with no dummy trigger.
    Sixty-three unrelated bodies must not acquire diagnostic hashes/copies. */
 xls_text* overlay=xlsTextCreate(changed,strlen(changed),strlen(changed),NULL);char* unused=xrtCalloc(512u*1024u,1);CHECK(overlay && unused);memset(unused,' ',512u*1024u);xls_text* unusedText=xlsTextCreate(unused,512u*1024u,512u*1024u,NULL);xrtFree(unused);CHECK(unusedText);
 xls_analysis_document docs[64]={{"dependency",dep,overlay,1,1}};char* paths[63];
 for(unsigned i=0;i<63;++i){char name[40];snprintf(name,sizeof(name),"unused-%u.xl",i);paths[i]=xrtPathJoin(dir,name);CHECK(paths[i]);docs[i+1]=(xls_analysis_document){paths[i],paths[i],unusedText,i+2,1};}
 hashBytes=0;xide_language_check* q=xideLanguageCheckStartDeclarationCapture(context,cache,compiler,actor,docs,64,&options,&error);CHECK(q && ready(q) && xideLanguageCheckCaptureDeclarations(q,&symbols,&work));
 CHECK(!q->semanticTarget && named(symbols,dep,"total",8) && !named(symbols,dep,"number",8) && named(saved,dep,"number",8) && hashBytes<1024u*1024u);size_t observedHashBytes=hashBytes;
 xideLanguageCheckDestroy(q);xideSemanticSymbolsDestroy(symbols);symbols=NULL;
 xide_text_file file={0};CHECK(xideTextFileRead(dep,&file,&error) && strstr(file.text,"number") && !strstr(file.text,"total"));xideTextFileClear(&file);
 /* Zero-overlay producers must keep their actual disk guard alive. */
 q=xideLanguageCheckStartDeclarationCapture(context,cache,compiler,actor,NULL,0,&options,&error);CHECK(q && ready(q));xide_language_input_guard* guard=xideLanguageCheckCaptureInputGuard(q,&error);CHECK(guard && writeText(dep,changed));
 for(unsigned i=0;i<1200 && xideLanguageCheckGuardState(q,NULL,0)==XIDE_GUARD_RUNNING;++i)xrtSleep(5);
 CHECK(xideLanguageCheckGuardState(q,NULL,0)==XIDE_GUARD_INVALID && !xideLanguageCheckCaptureDeclarations(q,&symbols,&work) && !symbols && !work.workerThread && !xideLanguageCheckDeclarationSourceRejected(q));
 CHECK(!xideLanguageInputGuardValidate(guard,NULL,NULL,&error) && writeText(dep,shared) && !xideLanguageInputGuardValidate(guard,NULL,NULL,&error));xideLanguageInputGuardRelease(guard);xideLanguageCheckDestroy(q);
 CHECK(writeText(dep,"func broken(\n"));q=xideLanguageCheckStartDeclarationCapture(context,cache,compiler,actor,NULL,0,&options,&error);CHECK(q && ready(q) && xideLanguageCheckDeclarationSourceRejected(q) && !xideLanguageCheckCaptureDeclarations(q,&symbols,&work) && !symbols);xideLanguageCheckDestroy(q);CHECK(writeText(dep,shared));
 q=xideLanguageCheckStartDeclarationCapture(context,cache,compiler,actor,NULL,0,&options,&error);CHECK(q);xideLanguageCheckCancel(q);CHECK(!xideLanguageCheckCaptureDeclarations(q,&symbols,&work) && !symbols && !xideLanguageCheckDeclarationSourceRejected(q));xideLanguageCheckDestroy(q);
 /* Immutable rows outlive the check, context, worker and compiler cache. */
 xlsWorkerDestroy(actor);xlsContextRelease(context);xideToolchainsDestroy(cache);CHECK(positions(saved));HANDLE threads[4];for(size_t i=0;i<4;++i){threads[i]=CreateThread(NULL,0,reader,xideSemanticSymbolsRetain(saved),0,NULL);CHECK(threads[i]);}
 CHECK(WaitForMultipleObjects(4,threads,TRUE,10000)==WAIT_OBJECT_0);for(size_t i=0;i<4;++i){DWORD code=1;CHECK(GetExitCodeThread(threads[i],&code) && !code);CloseHandle(threads[i]);}xideSemanticSymbolsDestroy(saved);
 printf("Headless capture: 0 open entries; 64 dependency/unrelated overlays; adapter hashed %zu bytes; exact semantic hit; disk guard invalidated; 4 detached readers.\n",observedHashBytes);
 xlsTextRelease(overlay);xlsTextRelease(unusedText);for(unsigned i=0;i<63;++i)xrtFree(paths[i]);xrtFree(entry);xrtFree(dep);xrtFree(compiler);xrtFree(dir);return 1;
}
int main(void){if(!run())return 1;printf("declaration_capture_test passed: %u checks; disk entry/imports, dirty dependency without trigger, source roles/positions, exact reuse, guards, source rejection, cancellation, minimal result and detached concurrent ownership.\n",checks);return 0;}
