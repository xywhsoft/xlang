#include "analysis/worker.h"
#include "analysis_ast_same.h"
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_worker_entry_mode_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
typedef struct result {xls_semantic_check* check;xls_check_stats work;xls_worker_cache_stats cache;} result;
static int query(xls_worker* worker,xls_context* context,const xls_analysis_document* docs,size_t count,const char* target,result* out)
{
 xls_query* q=xlsWorkerCheckInContext(worker,context,docs,count,target);CHECK(q);enum xls_query_state state=XLS_QUERY_PENDING;char error[256]={0};
 for(unsigned i=0;i<30000 && state==XLS_QUERY_PENDING;++i){state=xlsQueryCheckResult(q,&out->check,&out->work,NULL,error,sizeof(error));if(state==XLS_QUERY_PENDING)xrtSleep(1);}
 if(state!=XLS_QUERY_READY)fprintf(stderr,"%s\n",error);
 int okay=state==XLS_QUERY_READY && xlsQuerySessionCache(q,&out->cache);xlsQueryDestroy(q);CHECK(okay);return 1;
}
static void release(result* r){xlsSemanticCheckDestroy(r->check);*r=(result){0};}
static int fresh(result* r,xls_context* context,const xls_analysis_document* docs,size_t count)
{
 xls_semantic_input inputs[2];CHECK(count<=2);for(size_t i=0;i<count;++i)inputs[i]=(xls_semantic_input){docs[i].path,xlsTextData(docs[i].text),xlsTextSize(docs[i].text)};
 xls_semantic_check* expected=xlsSemanticCheckCreateInContext(context,xlsContextSpec(context)->entry,inputs,count,1,NULL,NULL,NULL,0);CHECK(expected);char *a=xlsSemanticCheckJson(expected,NULL),*b=xlsSemanticCheckJson(r->check,NULL);CHECK(a && b && !strcmp(a,b));CHECK(sameAst(xlsSemanticCheckAst(expected),xlsSemanticCheckAst(r->check)));xlsSemanticStringFree(a);xlsSemanticStringFree(b);xlsSemanticCheckDestroy(expected);return 1;
}
static int run(void)
{
 char *directory=xrtPathAbs("build/worker-entry-mode"),*entry=xrtPathJoin(directory,"main.xl"),*other=xrtPathJoin(directory,"other.xl");CHECK(directory && entry && other && xrtDirCreateAll(directory));const char *disk="// no program entry yet\n",*body="func main(...) as int\n return 7\nend\n",*otherBody="func main(...) as int\n return 9\nend\n";
 CHECK(xrtFileWriteAtomic(entry,(xbytesview){(const unsigned char*)disk,strlen(disk)}) && xrtFileWriteAtomic(other,(xbytesview){(const unsigned char*)otherBody,strlen(otherBody)}));xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project="entry-mode.xlp",.configuration="Debug",.entry=entry,.directory=directory,.analysisProfile="xlang-ls/compiler-xrt-v1",.targetKind="exe"};xls_context* context=xlsContextCreate(&spec,16u*1024u*1024u,NULL);spec.configuration="Release";xls_context* alternate=xlsContextCreate(&spec,16u*1024u*1024u,NULL);spec.configuration="Debug";spec.entry=other;spec.project="unrelated.xlp";xls_context* unrelated=xlsContextCreate(&spec,16u*1024u*1024u,NULL);xls_worker* worker=xlsWorkerCreate(NULL,8);xls_text* text=xlsTextCreate(body,strlen(body),1024*1024,NULL);xls_analysis_document doc={"entry",entry,text,1,1};CHECK(context && alternate && unrelated && worker && text);
 result r={0};CHECK(query(worker,context,NULL,0,NULL,&r) && xlsSemanticCheckInfo(r.check).accepted && r.cache.sessions==1);CHECK(fresh(&r,context,NULL,0));xls_semantic_check* saved=xlsSemanticCheckRetain(r.check);release(&r);
 CHECK(query(worker,alternate,NULL,0,NULL,&r) && xlsSemanticCheckInfo(r.check).accepted && r.cache.sessions==2);CHECK(fresh(&r,alternate,NULL,0));release(&r);
 CHECK(query(worker,context,&doc,1,"entry",&r) && xlsSemanticCheckInfo(r.check).accepted);CHECK(r.cache.sessions==1 && r.cache.pruned==1 && r.cache.evictions==1);CHECK(fresh(&r,context,&doc,1));release(&r);
 CHECK(query(worker,context,&doc,1,"entry",&r) && r.cache.sessions==1 && r.work.hits>=1 && r.cache.pruned==1);release(&r);
 /* Closing/reopening transitions preserve caller-held results and revisions. */
 CHECK(query(worker,context,NULL,0,NULL,&r) && xlsSemanticCheckInfo(r.check).accepted && r.cache.sessions==1 && r.cache.pruned==1);CHECK(fresh(&r,context,NULL,0));release(&r);doc.identity=2;doc.revision=0;
 CHECK(query(worker,context,&doc,1,"entry",&r) && xlsSemanticCheckInfo(r.check).accepted && r.cache.sessions==1 && r.cache.pruned==1);CHECK(fresh(&r,context,&doc,1));release(&r);
 /* A closed different entry remains reusable while this entry is overlaid. */
 CHECK(query(worker,unrelated,&doc,1,NULL,&r) && xlsSemanticCheckInfo(r.check).accepted && r.cache.sessions==2 && r.cache.pruned==1);CHECK(fresh(&r,unrelated,&doc,1));release(&r);
 CHECK(query(worker,context,&doc,1,"entry",&r) && r.cache.sessions==2 && r.cache.pruned==1);release(&r);CHECK(query(worker,unrelated,&doc,1,NULL,&r) && r.cache.sessions==2 && r.cache.pruned==1 && r.work.hits>=2);release(&r);
 /* Even a NULL-target caller observes changes in the entry overlay identity. */
 CHECK(query(worker,context,&doc,1,NULL,&r) && xlsSemanticCheckInfo(r.check).accepted);CHECK(fresh(&r,context,&doc,1));release(&r);doc.identity=3;
 CHECK(query(worker,unrelated,&doc,1,NULL,&r) && r.cache.sessions==1 && r.cache.pruned>=2);release(&r);
 xlsWorkerDestroy(worker);CHECK(xlsSemanticCheckInfo(saved).accepted);xlsSemanticCheckDestroy(saved);xlsTextRelease(text);xlsContextRelease(context);xlsContextRelease(alternate);xlsContextRelease(unrelated);xrtFree(directory);xrtFree(entry);xrtFree(other);return 1;
}
int main(void){if(!run())return 1;printf("analysis_worker_entry_mode_test passed: %u checks;same-entry representation reuse,obsolete configuration/identity sessions pruned,close/reopen transitions,fresh full JSON/AST agreement,unrelated closed root and detached disk result preserved.\n",checks);return 0;}
