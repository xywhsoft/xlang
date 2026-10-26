/* Own the real adapter implementation to measure host hash input bytes.
   Original native compiler/source/probe validation remains linked normally. */
#include "app_xrt.h"
static size_t hashBytes;
static bool captureHash(const void* data,size_t size,void* digest)
{hashBytes+=size;return xrtSha256(data,size,digest);}
#define xrtSha256 captureHash
#include "../src/services/language_snapshot.c"
#undef xrtSha256
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"reference_capture_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static int writeText(const char* path,const char* text){return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)});}
static int ready(xide_language_check* check,int wanted)
{char message[512]={0};enum xide_check_state state;for(unsigned i=0;;++i){state=xideLanguageCheckState(check,message,sizeof(message));if(state!=XIDE_CHECK_RUNNING)break;if(i>15000)return 0;xrtSleep(1);}if(state!=(enum xide_check_state)wanted)fprintf(stderr,"capture state %d,wanted %d:%s\n",state,wanted,message);return state==(enum xide_check_state)wanted;}
static int run(void)
{
 char* dir=xrtPathAbs("build/reference-capture");CHECK(dir && xrtDirCreateAll(dir));char *entry=xrtPathJoin(dir,"main.xl"),*dep=xrtPathJoin(dir,"shared.xl"),*compiler=xrtPathJoin(dir,"absent-xl.exe");CHECK(entry && dep && compiler);
 const char* body="\xef\xbb\xbf" "import shared\r\nfunc main(...) as int\r\n return shared.compute(7)\r\nend\r\n";const char* shared="export func compute(int value) as int\n return value+1\nend\n";
 CHECK(writeText(entry,body) && writeText(dep,shared));xls_context_spec spec={.kind=XLS_CONTEXT_XLANG,.entry=entry,.directory=dir,.analysisProfile="xlang-ls/compiler-xrt-v1",.targetKind="exe"};xls_context* context=xlsContextCreate(&spec,16u*1024u*1024u,NULL);xide_toolchains* cache=xideToolchainsCreate();xls_worker* actor=xlsWorkerCreate(NULL,8);CHECK(context && cache && actor);
 xls_text* main=xlsTextCreate(body,strlen(body),strlen(body),NULL);char* unused=xrtCalloc(512u*1024u,1);CHECK(main && unused);memset(unused,' ',512u*1024u);xls_text* unusedText=xlsTextCreate(unused,512u*1024u,512u*1024u,NULL);xrtFree(unused);CHECK(unusedText);char* paths[63];xls_analysis_document docs[64]={{"entry",entry,main,1,1}};
 for(unsigned i=0;i<63;++i){char name[40];snprintf(name,sizeof(name),"unused-%u.xl",i);paths[i]=xrtPathJoin(dir,name);CHECK(paths[i]);docs[i+1]=(xls_analysis_document){paths[i],paths[i],unusedText,i+2,1};}
 int line,character;CHECK(xideLspOffsetToPosition(body,(int)(strstr(body,"compute(7)")-body),&line,&character));xide_language_analysis_options options={.nativeBackend=1};xide_model_error error={0};xls_check_stats work; xls_worker_cache_stats sessions;xide_navigation_work navigation;
 CHECK(!xideLanguageCheckReferenceWork(NULL,&work,&sessions,&navigation) && !work.builds && !sessions.sessions && !navigation.built);
 size_t referenceBytes=0;uint64_t builds=0,hits=0;
 for(unsigned i=0;i<2;++i){hashBytes=0;xide_language_check* q=xideLanguageCheckStartReferenceCapture(context,cache,compiler,actor,docs,64,"entry",entry,line,character,&options,&error);CHECK(q && ready(q,XIDE_CHECK_READY));xvalue* result=xideLanguageCheckResult(q);CHECK(result && !get(result,"analysis") && !get(result,"documents") && !get(result,"diagnostics"));xide_reference_shard* shard=NULL;xide_reference_key* key=NULL;CHECK(xideLanguageCheckCaptureReferences(q,&shard,&key) && shard && key);xide_reference_shard_view view;CHECK(xideSemanticReferenceShardQuery(shard,key,1,&view)==XIDE_REFERENCE_MATCH && view.count==2);CHECK(xideLanguageCheckReferenceWork(q,&work,&sessions,&navigation));
  if(!i){builds=work.builds;hits=work.hits;CHECK(builds==1 && navigation.built==1);}else CHECK(work.builds==builds && work.hits>hits && navigation.hit==1 && !navigation.built);
  xide_language_input_guard* guard=xideLanguageCheckCaptureInputGuard(q,&error);CHECK(guard);xideLanguageCheckDestroy(q);CHECK(hashBytes<1024u*1024u);referenceBytes=hashBytes;CHECK(xideLanguageInputGuardValidate(guard,NULL,NULL,&error));
  xideSemanticReferenceShardRelease(shard);xideSemanticReferenceKeyRelease(key);xideLanguageInputGuardRelease(guard);xrtValueRelease(result);
 }
 hashBytes=0;xide_language_check* ordinary=xideLanguageCheckStartConfigured(context,cache,compiler,actor,docs,64,"entry",NULL,0,0,&options,&error);CHECK(ordinary && ready(ordinary,XIDE_CHECK_READY));xvalue* full=xideLanguageCheckResult(ordinary);CHECK(full && get(full,"analysis") && get(full,"documents") && get(full,"diagnostics"));CHECK(!xideLanguageCheckReferenceWork(ordinary,&work,&sessions,&navigation));xideLanguageCheckDestroy(ordinary);CHECK(hashBytes>=63u*512u*1024u && hashBytes>referenceBytes);xrtValueRelease(full);
 xide_language_check* q=xideLanguageCheckStartReferenceCapture(context,cache,compiler,actor,docs,64,"entry",entry,line,character,&options,&error);CHECK(q && ready(q,XIDE_CHECK_READY));xide_language_input_guard* guard=xideLanguageCheckCaptureInputGuard(q,&error);CHECK(guard);xideLanguageCheckDestroy(q);CHECK(writeText(dep,"export func compute(int value) as int\n return value+2\nend\n"));CHECK(!xideLanguageInputGuardValidate(guard,NULL,NULL,&error));xideLanguageInputGuardRelease(guard);
 CHECK(writeText(dep,"func broken(\n"));q=xideLanguageCheckStartReferenceCapture(context,cache,compiler,actor,docs,64,"entry",entry,line,character,&options,&error);CHECK(q && ready(q,XIDE_CHECK_READY));xide_reference_shard* shard=NULL;xide_reference_key* key=NULL;CHECK(!xideLanguageCheckCaptureReferences(q,&shard,&key) && !shard && !key);xideLanguageCheckDestroy(q);CHECK(writeText(dep,shared));
 docs[63].path=entry;q=xideLanguageCheckStartReferenceCapture(context,cache,compiler,actor,docs,64,"entry",entry,line,character,&options,&error);CHECK(!q || ready(q,XIDE_CHECK_FAILED));xideLanguageCheckDestroy(q);docs[63].path=paths[62];
 q=xideLanguageCheckStartReferenceCapture(context,cache,compiler,actor,docs,64,"entry",entry,line,character,&options,&error);CHECK(q);xideLanguageCheckCancel(q);CHECK(!xideLanguageCheckCaptureReferences(q,&shard,&key));xideLanguageCheckDestroy(q);
 xlsWorkerDestroy(actor);xideToolchainsDestroy(cache);xlsContextRelease(context);xlsTextRelease(main);xlsTextRelease(unusedText);for(unsigned i=0;i<63;++i)xrtFree(paths[i]);xrtFree(entry);xrtFree(dep);xrtFree(compiler);xrtFree(dir);return 1;
}
int main(void){if(!run())return 1;printf("reference_capture_test passed: %u checks;actual retained capture,32MiB unrelated bodies without diagnostic hashes,ordinary diagnostics parity,real key work,source guard,errors/alias/cancel\n",checks);return 0;}
