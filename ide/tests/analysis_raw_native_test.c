#include "analysis/completion_session.h"
#include "analysis/frontend_internal.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_cursor_memo_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
#include "analysis_ast_same.h"

#include "analysis/frontend_raw_body.h"
#include "analysis/frontend_entry_cache.h"
#include "analysis/input_batch.h"
#include "src/sem/xl6_completion.h"
#include <stdlib.h>
static char* load(const char* name){FILE* f=fopen(name,"rb");if(!f)return NULL;fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);char* s=n>=0 ? xrtMalloc((size_t)n+1) : NULL;if(s && fread(s,1,(size_t)n,f)==(size_t)n)s[n]=0;else {xrtFree(s);s=NULL;}fclose(f);return s;}
static int verifyNativeContext(const char* beforePath,const char* afterPath,int minimum,int exact,int fallback,const char* depBeforePath,const char* depAfterPath,int configured){
 char* before=load(beforePath);char* after=load(afterPath);char* third=after ? xrtStrDup(after) : NULL;char* path=xrtPathAbs("native-raw.xl");CHECK(before && after && third && path);
 char* changed=strstr(third,"prefixValue+9");CHECK(changed);changed[strlen("prefixValue+")]='8';
 char* depPath=xrtPathAbs("dependency.xl");char* depBefore=depBeforePath ? load(depBeforePath) : NULL;char* depAfter=depAfterPath ? load(depAfterPath) : NULL;CHECK(depPath && (!depBeforePath || (depBefore && depAfter)));
 char* configuredEntry=xrtStrDup(path);CHECK(configuredEntry);
 for(char* at=configuredEntry;*at;++at)if((unsigned char)*at==92)*at='/';
 char* directory=xrtPathParent(configuredEntry);CHECK(directory);
 xls_context_spec spec={.kind=XLS_CONTEXT_XLANG,.entry=configuredEntry,.directory=directory,.analysisProfile="native-raw-context-proof",.targetKind="exe",.packagePath=""};
 xls_context* context=configured ? xlsContextCreate(&spec,1024u*1024u,NULL) : NULL;CHECK(!configured || context);
 const char* entry=context ? xlsContextSpec(context)->entry : path;
 xls_entry_cache* cache=xlsEntryCacheCreate(32u*1024u*1024u);CHECK(cache);
 xls_input_batch* batch=NULL;xls_syntax* syntax=NULL;xls_semantic_completion* previous=NULL;
 uint64_t firstNodes=0,warmNodes=0;
 for(size_t step=0;step<3;++step){
  char* body=step==2 ? third : step ? after : before;const char* marker=strstr(body,"return pref");CHECK(marker);size_t at=(size_t)(marker-body)+strlen("return pref")-(!memcmp(body,"\xef\xbb\xbf",3)?3:0);
  xls_text* text=xlsTextCreate(body,strlen(body),8u*1024u*1024u,NULL);CHECK(text);xls_text* dep=depBefore ? xlsTextCreate(step ? depAfter : depBefore,strlen(step ? depAfter : depBefore),8u*1024u*1024u,NULL) : NULL;CHECK(!depBefore || dep);
  xls_analysis_document docs[2]={{"entry",configured ? configuredEntry : path,text,1,step+1},{"dep",depPath,dep,2,step+1}};size_t count=dep ? 2 : 1;
  xls_input_batch* current=xlsInputBatchCreateInContext(context,NULL,batch,docs,count,configured ? NULL : "entry",NULL,NULL,NULL,0);CHECK(current);
  XL6_CompletionQuery q={.path=path,.offset=at};xls_parse_work parser={0};xls_syntax* next=xlsSyntaxBuildCursor(path,text,syntax,&q,&parser);xl6CompletionFree(&q);CHECK(next && xlsSyntaxAccepted(next));
  char error[256]={0};xls_semantic_completion* result=xlsSemanticCompletionCreateBatch(cache,current,"entry",at,next,previous,NULL,NULL,error,sizeof(error));
  if(!result)fprintf(stderr,"native raw creation: %s\n",error);
  CHECK(result);
  xls_raw_native_work raw=xlsSemanticCompletionRawBodyWork(result);xls_sem_work sem=xlsSemanticCompletionSemWork(result);xls_ast_materialize_work copies=xlsSemanticCompletionMaterializationWork(result);
  fprintf(stderr,"step=%zu registered=%llu raw=%llu restored=%llu retry=%llu checked=%llu reused=%llu proof=%llu physical=%llu\n",step,(unsigned long long)raw.registered,(unsigned long long)raw.deferred,(unsigned long long)raw.restored,(unsigned long long)raw.retries,(unsigned long long)sem.checked,(unsigned long long)sem.reused,(unsigned long long)sem.proofHits,(unsigned long long)copies.nodes);
  xls_semantic_input inputs[2]={{path,body,strlen(body)},{depPath,step ? depAfter : depBefore,dep ? xlsTextSize(dep) : 0}};xls_semantic_completion* fresh=xlsSemanticCompletionCreateInContext(context,entry,path,at,inputs,count,NULL,NULL,NULL,0);CHECK(fresh);
  char* a=xlsSemanticCompletionJson(result,NULL);char* b=xlsSemanticCompletionJson(fresh,NULL);if(a && b && strcmp(a,b))fprintf(stderr,"query differs %s\n%s\n",a,b);CHECK(a && b && !strcmp(a,b));xlsSemanticStringFree(a);xlsSemanticStringFree(b);
  if(!step){firstNodes=copies.nodes;CHECK(!raw.deferred && raw.registered>=(uint64_t)minimum && raw.memoBytes);}else if(step==1 && fallback){CHECK(raw.retries==1 && !raw.deferred);}else{warmNodes=copies.nodes;CHECK(raw.deferred>=(uint64_t)minimum && (!exact || raw.deferred==(uint64_t)minimum) && raw.restored==raw.deferred && !raw.retries && sem.proofHits>=(uint64_t)minimum && warmNodes<firstNodes/2);}
  xlsSemanticCompletionDestroy(previous);xlsInputBatchDestroy(batch);xlsSyntaxRelease(syntax);xlsTextRelease(text);xlsTextRelease(dep);
  batch=current;syntax=next;previous=result;
  CHECK(sameAst(xlsSemanticCompletionAst(result),xlsSemanticCompletionAst(fresh)));if(step && !(step==1 && fallback))CHECK(xlsSemanticCompletionRawBodyWork(result).fullMaterialized);
  xlsSemanticCompletionDestroy(fresh);
 }
 xlsInputBatchDestroy(batch);xlsSyntaxRelease(syntax);xlsEntryCacheDestroy(cache);CHECK(xlsSemanticCompletionValidate(previous));xlsSemanticCompletionDestroy(previous);xrtFree(before);xrtFree(after);xrtFree(third);xrtFree(path);xrtFree(depPath);xrtFree(depBefore);xrtFree(depAfter);xlsContextRelease(context);xrtFree(directory);xrtFree(configuredEntry);return 1;
}
static int verifyNative(const char* before,const char* after,int minimum,int exact,int fallback,const char* depBefore,const char* depAfter)
{return verifyNativeContext(before,after,minimum,exact,fallback,depBefore,depAfter,0);}
#include "analysis_raw_test_sources.h"
int main(int argc,char** argv){
 if(argc==1){char *before=NULL,*after=NULL;if(!rawTestSources("raw-native",1,&before,&after))return 1;int okay=verifyNative(before,after,32,1,0,NULL,NULL);if(okay)okay=verifyNativeContext(before,after,32,1,0,NULL,NULL,1);xrtFree(before);xrtFree(after);if(okay)printf("analysis_raw_native_test passed: %u checks\n",checks);return okay ? 0 : 1;}
 if((argc!=6 && argc!=8) || !verifyNative(argv[1],argv[2],atoi(argv[3]),atoi(argv[4]),atoi(argv[5]),argc==8 ? argv[6] : NULL,argc==8 ? argv[7] : NULL))return 1;
 printf("native raw body producer passed: %u checks;original query/full AST proof and actual compiler-copy nodes measured\n",checks);return 0;}
