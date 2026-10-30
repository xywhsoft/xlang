#include "analysis_test_clock.h"
#include "analysis/worker.h"
#include "analysis/frontend_internal.h"
#include "analysis/signature_internal.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_worker_cache_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
enum fixture_kind {CHECK_KIND,COMPLETION_KIND,SIGNATURE_KIND};
typedef struct fixture_result {
 xls_semantic_check* check;xls_semantic_completion* completion;xls_signature_help* signature;
 xls_check_stats checks;xls_completion_stats completions;xls_signature_stats signatures;
 xls_worker_cache_stats cache;enum xls_query_state state;char error[256];
} fixture_result;
static const char *sourceA="\xef\xbb\xbf#doc\r\ndesc: A 中文😀\r\n#end\r\nfunc pick_A(int left,string right) as int\r\n return left\r\nend\r\nfunc main(...) as int\r\n int unique_A=pick_A(7,\"hi\")\r\n return unique_A\r\nend\r\n",
 *sourceB="\xef\xbb\xbf#doc\r\ndesc: B 中文😀\r\n#end\r\nfunc pick_B(int left,string right) as int\r\n return left\r\nend\r\nfunc main(...) as int\r\n int unique_B=pick_B(9,\"hi\")\r\n return unique_B\r\nend\r\n";
static void notify(void* event){xrtEventSet(event);}
static void releaseResult(fixture_result* r)
{xlsSemanticCheckDestroy(r->check);xlsSemanticCompletionDestroy(r->completion);xlsSignatureRelease(r->signature);*r=(fixture_result){0};}
static uint64_t builds(const fixture_result* r,enum fixture_kind k){return k==CHECK_KIND ? r->checks.builds : k==COMPLETION_KIND ? r->completions.builds : r->signatures.builds;}
static uint64_t hits(const fixture_result* r,enum fixture_kind k){return k==CHECK_KIND ? r->checks.hits : k==COMPLETION_KIND ? r->completions.hits : r->signatures.hits;}
static int sameResult(const fixture_result* a,const fixture_result* b,enum fixture_kind k)
{return k==CHECK_KIND ? a->check==b->check : k==COMPLETION_KIND ? a->completion==b->completion : a->signature==b->signature;}
static fixture_result query(xls_worker* w,enum fixture_kind kind,xls_context* context,const xls_analysis_document* docs,size_t count,const char* entry,const char* target)
{
 fixture_result result={0};const xls_analysis_document* d=NULL;for(size_t i=0;i<count;++i)if(!strcmp(docs[i].key,target))d=&docs[i];
 if(!d)return result;
 const char* body=xlsTextData(d->text);const char* marker=strstr(body,kind==SIGNATURE_KIND ? "\"hi\"" : "return unique_");if(!marker)return result;
 size_t bom=xlsTextSize(d->text)>=3 && !memcmp(body,"\xef\xbb\xbf",3) ? 3 : 0;
 size_t offset=(size_t)(marker-body)+(kind==SIGNATURE_KIND ? 2 : strlen("return unique_"))-bom;
 xevent event;if(!xrtEventInit(&event,false,false))return result;
 xls_query* q=kind==CHECK_KIND ? xlsWorkerCheckInContext(w,context,docs,count,entry) : kind==COMPLETION_KIND ?
  xlsWorkerCompletionInContext(w,context,docs,count,entry,target,offset) : xlsWorkerSignatureInContext(w,context,docs,count,entry,target,offset);
 if(q && xlsQuerySetWake(q,notify,&event) && xrtEventWaitFor(&event,15000000)==XWAIT_OK){
  if(kind==CHECK_KIND)result.state=xlsQueryCheckResult(q,&result.check,&result.checks,NULL,result.error,sizeof(result.error));
  else if(kind==COMPLETION_KIND)result.state=xlsQueryCompletionResult(q,&result.completion,&result.completions,NULL,result.error,sizeof(result.error));
  else result.state=xlsQuerySignatureResult(q,&result.signature,&result.signatures,NULL,result.error,sizeof(result.error));
  int found=xlsQuerySessionCache(q,&result.cache);if(result.state==XLS_QUERY_READY && !found)result.state=XLS_QUERY_FAILED;
 }
 xlsQueryDestroy(q);xrtEventUnit(&event);return result;
}
static int sameString(const char* a,const char* b){return a && b ? !strcmp(a,b) : a==b;}
static int sameAst(const XL6_AstNode* a,const XL6_AstNode* b)
{
 if(!a || !b)return a==b;
 XL6_AstNode x=*a,y=*b;x.text=y.text=x.doc=y.doc=NULL;x.literalData=y.literalData=NULL;
 x.children=y.children=NULL;x.inferredType=y.inferredType=x.semanticDeclaredType=y.semanticDeclaredType=NULL;
 x.xlsStorage=y.xlsStorage=NULL;x.xlsLeafReferences=y.xlsLeafReferences=0;x.source=y.source=NULL;x.childCapacity=y.childCapacity=0;
 if(memcmp(&x,&y,sizeof(x)) || !sameString(a->text,b->text) || !sameString(a->doc,b->doc) || !!a->literalData!=!!b->literalData ||
    (a->literalSize && memcmp(a->literalData,b->literalData,a->literalSize)) || !!a->source!=!!b->source)return 0;
 if(a->source && (a->source==b->source || !sameString(a->source->path,b->source->path) || a->source->completionQuery!=b->source->completionQuery ||
    a->source->completionStart!=b->source->completionStart || a->source->completionEnd!=b->source->completionEnd))return 0;
 if(!sameAst(a->inferredType,b->inferredType) || !sameAst(a->semanticDeclaredType,b->semanticDeclaredType))return 0;
 for(size_t i=0;i<a->childCount;++i)if(!sameAst(a->children[i],b->children[i]))return 0;
 return 1;
}
static int parity(const fixture_result* r,enum fixture_kind kind,xls_context* context,const xls_analysis_document* docs,size_t count,const char* entry,const char* target)
{
 const xls_analysis_document *e=NULL,*d=NULL;xls_semantic_input inputs[64];
 for(size_t i=0;i<count;++i){inputs[i]=(xls_semantic_input){docs[i].path,xlsTextData(docs[i].text),xlsTextSize(docs[i].text)};if(entry && !strcmp(docs[i].key,entry))e=&docs[i];if(!strcmp(docs[i].key,target))d=&docs[i];}
 CHECK(d && (e || context));const char* entryPath=e ? e->path : xlsContextSpec(context)->entry;
 const char* body=xlsTextData(d->text);size_t bom=xlsTextSize(d->text)>=3 && !memcmp(body,"\xef\xbb\xbf",3) ? 3 : 0;
 const char* marker=strstr(body,kind==SIGNATURE_KIND ? "\"hi\"" : "return unique_");CHECK(marker);
 size_t offset=(size_t)(marker-body)+(kind==SIGNATURE_KIND ? 2 : strlen("return unique_"))-bom;
 if(kind==CHECK_KIND){
  xls_semantic_check* fresh=xlsSemanticCheckCreateInContext(context,entryPath,inputs,count,1,NULL,NULL,NULL,0);CHECK(fresh && r->check);
  char *a=xlsSemanticCheckJson(fresh,NULL),*b=xlsSemanticCheckJson(r->check,NULL);CHECK(a && b && !strcmp(a,b));xlsSemanticStringFree(a);xlsSemanticStringFree(b);
  CHECK(sameAst(xlsSemanticCheckAst(fresh),xlsSemanticCheckAst(r->check)));
  CHECK(xlsSemanticCheckInfo(fresh).accepted==xlsSemanticCheckInfo(r->check).accepted);
  if(xlsSemanticCheckInfo(fresh).accepted)CHECK(xlsSemanticCheckIr(fresh) && xlsSemanticCheckIr(r->check));
  xlsSemanticCheckDestroy(fresh);
 }else if(kind==COMPLETION_KIND){
  xls_semantic_completion* fresh=xlsSemanticCompletionCreateInContext(context,entryPath,d->path,offset,inputs,count,NULL,NULL,NULL,0);CHECK(fresh && r->completion);
  char *a=xlsSemanticCompletionJson(fresh,NULL),*b=xlsSemanticCompletionJson(r->completion,NULL);CHECK(a && b && !strcmp(a,b));xlsSemanticStringFree(a);xlsSemanticStringFree(b);
  CHECK(sameAst(xlsSemanticCompletionAst(fresh),xlsSemanticCompletionAst(r->completion)));xlsSemanticCompletionDestroy(fresh);
 }else{
  xls_signature_help* fresh=xlsSignatureCreateInContext(context,entryPath,d->path,offset,inputs,count,NULL,NULL,NULL,0);CHECK(fresh && r->signature);
  xls_signature_info a=xlsSignatureInfo(fresh),b=xlsSignatureInfo(r->signature);CHECK(a.available==b.available && a.count==b.count && a.activeSignature==b.activeSignature);
  for(size_t i=0;i<a.count;++i){xls_signature_item x,y;CHECK(xlsSignatureItem(fresh,i,&x) && xlsSignatureItem(r->signature,i,&y));CHECK(sameString(x.label,y.label) && x.symbol==y.symbol && x.activeParameter==y.activeParameter);}
  CHECK(sameAst(xlsSemanticCompletionAst(xlsSignatureSemantic(fresh)),xlsSemanticCompletionAst(xlsSignatureSemantic(r->signature))));xlsSignatureRelease(fresh);
 }
 return 1;
}
typedef struct fixture {char *root,*paths[3];xls_text* text[3];xls_analysis_document docs[3];} fixture;
static int setup(fixture* f)
{
 char relative[128];snprintf(relative,sizeof(relative),"build/worker-cache-%llu-%llu",(unsigned long long)xrtThreadCurrentId(),(unsigned long long)xlsTestClockMicros());CHECK(xrtDirCreateAll(relative));f->root=xrtPathAbs(relative);CHECK(f->root);
 const char* names[]={"源A.xl","源B.xl","third.xl"};const char* bodies[]={sourceA,sourceB,sourceA};const char* keys[]={"a","b","c"};
 for(size_t i=0;i<3;++i){f->paths[i]=xrtPathJoin(f->root,names[i]);f->text[i]=xlsTextCreate(bodies[i],strlen(bodies[i]),1024*1024,NULL);CHECK(f->paths[i] && f->text[i]);f->docs[i]=(xls_analysis_document){keys[i],f->paths[i],f->text[i],i+10,1};}
 return 1;
}
static void clear(fixture* f){for(size_t i=0;i<3;++i){xlsTextRelease(f->text[i]);xrtFree(f->paths[i]);}xrtFree(f->root);}
static int switching(void)
{
 fixture f={0};CHECK(setup(&f));xls_worker* w=xlsWorkerCreate(NULL,8);CHECK(w);fixture_result first[3][2]={0};
 for(unsigned cycle=0;cycle<5;++cycle)for(unsigned kind=0;kind<3;++kind)for(size_t i=0;i<2;++i){
  fixture_result r=query(w,(enum fixture_kind)kind,NULL,f.docs,3,f.docs[i].key,f.docs[i].key);CHECK(r.state==XLS_QUERY_READY && builds(&r,(enum fixture_kind)kind)==(cycle ? 2 : i+1));
  CHECK(r.cache.sessionLimit==8 && r.cache.inputStorageLimit==64u*1024u*1024u && !r.cache.evictions && !r.cache.inputBudgetExceeded);
  CHECK(parity(&r,(enum fixture_kind)kind,NULL,f.docs,3,f.docs[i].key,f.docs[i].key));
  if(!cycle)first[kind][i]=r;else{CHECK(sameResult(&r,&first[kind][i],(enum fixture_kind)kind) && hits(&r,(enum fixture_kind)kind)==cycle*2+i-1);releaseResult(&r);}
 }
 xlsWorkerDestroy(w);for(unsigned kind=0;kind<3;++kind)for(size_t i=0;i<2;++i){CHECK(parity(&first[kind][i],(enum fixture_kind)kind,NULL,f.docs,3,f.docs[i].key,f.docs[i].key));releaseResult(&first[kind][i]);}clear(&f);return 1;
}
static int evictionAndBudget(void)
{
 CHECK(!xlsWorkerCreateWithCache(NULL,1,&(xls_worker_cache_limits){0,1}));CHECK(!xlsWorkerCreateWithCache(NULL,1,&(xls_worker_cache_limits){33,1}));CHECK(!xlsWorkerCreateWithCache(NULL,1,&(xls_worker_cache_limits){2,0}));
 fixture f={0};CHECK(setup(&f));
 for(unsigned kind=0;kind<3;++kind){
  xls_worker* w=xlsWorkerCreateWithCache(NULL,2,&(xls_worker_cache_limits){2,64u*1024u*1024u});CHECK(w);
  fixture_result a=query(w,kind,NULL,f.docs,3,"a","a"),b=query(w,kind,NULL,f.docs,3,"b","b"),r=query(w,kind,NULL,f.docs,3,"a","a");CHECK(a.state==1 && b.state==1 && r.state==1 && sameResult(&a,&r,kind));releaseResult(&r);
  r=query(w,kind,NULL,f.docs,3,"c","c");CHECK(r.state==1 && builds(&r,kind)==3 && r.cache.sessions==2 && r.cache.evictions==1);releaseResult(&r);
  r=query(w,kind,NULL,f.docs,3,"a","a");CHECK(r.state==1 && sameResult(&a,&r,kind) && builds(&r,kind)==3);releaseResult(&r);
  r=query(w,kind,NULL,f.docs,3,"b","b");CHECK(r.state==1 && !sameResult(&b,&r,kind) && builds(&r,kind)==4 && r.cache.evictions==2);CHECK(parity(&r,kind,NULL,f.docs,3,"b","b"));releaseResult(&r);xlsWorkerDestroy(w);
  CHECK(parity(&a,kind,NULL,f.docs,3,"a","a") && parity(&b,kind,NULL,f.docs,3,"b","b"));releaseResult(&a);releaseResult(&b);
  w=xlsWorkerCreateWithCache(NULL,2,&(xls_worker_cache_limits){8,1});CHECK(w);a=query(w,kind,NULL,f.docs,3,"a","a");CHECK(a.state==1 && a.cache.sessions==1 && a.cache.inputBudgetExceeded && a.cache.inputStorageBytes>1);
  r=query(w,kind,NULL,f.docs,3,"b","b");CHECK(r.state==1 && r.cache.sessions==1 && r.cache.evictions==1 && r.cache.inputBudgetExceeded);releaseResult(&r);
  r=query(w,kind,NULL,f.docs,3,"a","a");CHECK(r.state==1 && builds(&r,kind)==3 && !sameResult(&a,&r,kind));releaseResult(&r);releaseResult(&a);xlsWorkerDestroy(w);
 }
 clear(&f);return 1;
}
static int history(void)
{
 fixture f={0};CHECK(setup(&f));
 for(unsigned kind=0;kind<3;++kind){
  f.docs[1].revision=1;xls_worker* w=xlsWorkerCreate(NULL,4);CHECK(w);fixture_result a=query(w,kind,NULL,f.docs,3,"a","a");CHECK(a.state==1);
  f.docs[1].revision=20;fixture_result b=query(w,kind,NULL,f.docs,3,"b","b");CHECK(b.state==1 && builds(&b,kind)==2);
  f.docs[1].revision=19;fixture_result r=query(w,kind,NULL,f.docs,3,"a","a");CHECK(r.state==XLS_QUERY_FAILED && strstr(r.error,"revision"));releaseResult(&r);
  f.docs[1].revision=20;f.docs[1].text=f.text[2];r=query(w,kind,NULL,f.docs,3,"a","a");CHECK(r.state==XLS_QUERY_FAILED && strstr(r.error,"revision"));releaseResult(&r);f.docs[1].text=f.text[1];
  r=query(w,kind,NULL,f.docs,3,"b","b");CHECK(r.state==1 && sameResult(&b,&r,kind) && builds(&r,kind)==2);releaseResult(&r);
  xls_analysis_document closed[]={f.docs[0],f.docs[2]};r=query(w,kind,NULL,closed,2,"c","c");CHECK(r.state==1 && r.cache.pruned==1 && r.cache.sessions==2);releaseResult(&r);
  /* This stream observed B's absence. The retained A's old B revision20 is
   * not the current revision authority after reappearance;even reused caller
   * identity must match the old single-session accepted-stream behavior. */
  f.docs[1].revision=1;f.docs[1].text=f.text[2];r=query(w,kind,NULL,f.docs,3,"a","a");CHECK(r.state==1);CHECK(parity(&r,kind,NULL,f.docs,3,"a","a"));releaseResult(&r);
  f.docs[1].text=f.text[1];f.docs[1].identity+=100;r=query(w,kind,NULL,f.docs,3,"b","b");CHECK(r.state==1 && !sameResult(&b,&r,kind));CHECK(parity(&r,kind,NULL,f.docs,3,"b","b"));releaseResult(&r);
  xlsWorkerDestroy(w);releaseResult(&a);releaseResult(&b);
 }
 clear(&f);return 1;
}
static int contexts(void)
{
 fixture f={0};CHECK(setup(&f));char* project=xrtPathJoin(f.root,"fixture.xlp");CHECK(project);
 xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=project,.configuration="debug",.entry=f.paths[0],.directory=f.root,.analysisProfile="worker-cache-fixture",.targetKind="exe"};
 xls_context *debug=xlsContextCreate(&spec,1024*1024,NULL),*twin=xlsContextCreate(&spec,1024*1024,NULL);spec.configuration="release";xls_context* optimized=xlsContextCreate(&spec,1024*1024,NULL);CHECK(debug && twin && optimized && debug!=twin && xlsContextEqual(debug,twin));
 xls_worker* w=xlsWorkerCreate(NULL,4);CHECK(w);
 for(unsigned kind=0;kind<3;++kind){
  fixture_result a=query(w,kind,debug,f.docs,3,"a","a"),b=query(w,kind,optimized,f.docs,3,"a","a"),r=query(w,kind,twin,f.docs,3,"a","a");
  CHECK(a.state==1 && b.state==1 && r.state==1 && builds(&r,kind)==2 && hits(&r,kind)==1 && sameResult(&a,&r,kind) && !sameResult(&a,&b,kind));
  CHECK(parity(&r,kind,twin,f.docs,3,"a","a"));releaseResult(&r);releaseResult(&a);releaseResult(&b);
 }
 xlsWorkerDestroy(w);xlsContextRelease(debug);xlsContextRelease(twin);xlsContextRelease(optimized);xrtFree(project);clear(&f);return 1;
}
static int writeSource(const char* path,const char* text)
{return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)});}
static int replaceBody(fixture* f,size_t index,const char* import,const char* suffix)
{
 const char* old=index==1 ? sourceB : sourceA;const char* returned=strstr(old,"return unique_");CHECK(returned);
 xstrbuf text;xrtStrBufInit(&text);CHECK(xrtStrBufAppend(&text,xrtStrView("\xef\xbb\xbf")) && xrtStrBufAppend(&text,xrtStrView(import)) &&
  xrtStrBufAppend(&text,(xstrview){old+3,(size_t)(returned-old)-3}) && xrtStrBufAppend(&text,xrtStrView(index==1 ? "return unique_B" : "return unique_A")) &&
  xrtStrBufAppend(&text,xrtStrView(suffix)) && xrtStrBufAppend(&text,xrtStrView("\r\nend\r\n")));
 xls_text* next=xlsTextCreate(text.Data,text.Size,1024*1024,NULL);xrtStrBufFree(&text);CHECK(next);xlsTextRelease(f->text[index]);f->text[index]=next;f->docs[index].text=next;return 1;
}
static int diskGuards(void)
{
 fixture f={0};CHECK(setup(&f));char* shared=xrtPathJoin(f.root,"shared_dep.xl");CHECK(shared);
 const char *good="export func value() as int\n return 7\nend\n",*bad="export func value() as int\n return \"wrong\"\nend\n";
 CHECK(replaceBody(&f,0,"import shared_dep\r\n","+shared_dep.value()") && replaceBody(&f,1,"import shared_dep\r\n","+shared_dep.value()"));
 for(unsigned kind=0;kind<3;++kind){
  CHECK(writeSource(shared,good));xls_worker* w=xlsWorkerCreate(NULL,4);CHECK(w);fixture_result a=query(w,kind,NULL,f.docs,3,"a","a"),b=query(w,kind,NULL,f.docs,3,"b","b"),c=query(w,kind,NULL,f.docs,3,"c","c");CHECK(a.state==1 && b.state==1 && c.state==1);
  CHECK(writeSource(shared,bad));fixture_result r=query(w,kind,NULL,f.docs,3,"a","a");CHECK(r.state==1 && !sameResult(&a,&r,kind) && builds(&r,kind)==4);CHECK(parity(&r,kind,NULL,f.docs,3,"a","a"));releaseResult(&r);
  r=query(w,kind,NULL,f.docs,3,"b","b");CHECK(r.state==1 && !sameResult(&b,&r,kind) && builds(&r,kind)==5);CHECK(parity(&r,kind,NULL,f.docs,3,"b","b"));releaseResult(&r);
  r=query(w,kind,NULL,f.docs,3,"c","c");CHECK(r.state==1 && sameResult(&c,&r,kind) && builds(&r,kind)==5);releaseResult(&r);
  CHECK(writeSource(shared,good));r=query(w,kind,NULL,f.docs,3,"a","a");CHECK(r.state==1 && builds(&r,kind)==6);CHECK(parity(&r,kind,NULL,f.docs,3,"a","a"));releaseResult(&r);
  r=query(w,kind,NULL,f.docs,3,"b","b");CHECK(r.state==1 && builds(&r,kind)==7);CHECK(parity(&r,kind,NULL,f.docs,3,"b","b"));releaseResult(&r);
  xlsWorkerDestroy(w);releaseResult(&a);releaseResult(&b);releaseResult(&c);
 }
 xrtFree(shared);clear(&f);
 fixture missing={0};CHECK(setup(&missing));char* late=xrtPathJoin(missing.root,"late_dep.xl");CHECK(late && replaceBody(&missing,0,"import late_dep\r\n","+late_dep.value()"));
 for(unsigned kind=0;kind<3;++kind){
  CHECK(!xrtFileExists(late));xls_worker* w=xlsWorkerCreate(NULL,4);CHECK(w);fixture_result a=query(w,kind,NULL,missing.docs,3,"a","a"),b=query(w,kind,NULL,missing.docs,3,"b","b");CHECK(a.state==1 && b.state==1);
  CHECK(writeSource(late,good));fixture_result r=query(w,kind,NULL,missing.docs,3,"a","a");CHECK(r.state==1 && builds(&r,kind)==3 && !sameResult(&a,&r,kind));CHECK(parity(&r,kind,NULL,missing.docs,3,"a","a"));releaseResult(&r);
  xlsWorkerDestroy(w);releaseResult(&a);releaseResult(&b);CHECK(xrtFileDelete(late));
 }
 xrtFree(late);clear(&missing);return 1;
}
static int closedEntries(void)
{
 fixture f={0};CHECK(setup(&f));char *dep=xrtPathJoin(f.root,"querymod.xl"),*project=xrtPathJoin(f.root,"disk.xlp");CHECK(dep && project);
 const char* disk="import querymod\nfunc main(...) as int\n return querymod.value()\nend\n";
 const char* body="\xef\xbb\xbf\r\nfunc pick_dep(int left,string right) as int\r\n return left\r\nend\r\nexport func value() as int\r\n int unique_dep=pick_dep(7,\"hi\")\r\n return unique_dep\r\nend\r\n";
 xls_text* text=xlsTextCreate(body,strlen(body),1024*1024,NULL);CHECK(text && writeSource(f.paths[0],disk) && writeSource(f.paths[2],disk));
 xls_analysis_document doc={"dep",dep,text,99,1};xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=project,.configuration="debug",.entry=f.paths[0],.directory=f.root,.analysisProfile="worker-disk-fixture",.targetKind="exe"};
 xls_context* a=xlsContextCreate(&spec,1024*1024,NULL);spec.entry=f.paths[2];xls_context* b=xlsContextCreate(&spec,1024*1024,NULL);CHECK(a && b);
 for(unsigned kind=0;kind<3;++kind){
  CHECK(writeSource(f.paths[0],disk));xls_worker* w=xlsWorkerCreate(NULL,4);CHECK(w);fixture_result first=query(w,kind,a,&doc,1,NULL,"dep"),second=query(w,kind,b,&doc,1,NULL,"dep"),r=query(w,kind,a,&doc,1,NULL,"dep");
  CHECK(first.state==1 && second.state==1 && r.state==1 && sameResult(&first,&r,kind) && builds(&r,kind)==2);CHECK(parity(&r,kind,a,&doc,1,NULL,"dep"));releaseResult(&r);
  CHECK(writeSource(f.paths[0],"import querymod\nfunc main(...) as int\n return querymod.value()\nend\n// different entry source\n"));r=query(w,kind,a,&doc,1,NULL,"dep");CHECK(r.state==1 && !sameResult(&first,&r,kind) && builds(&r,kind)==3);CHECK(parity(&r,kind,a,&doc,1,NULL,"dep"));releaseResult(&r);
  r=query(w,kind,b,&doc,1,NULL,"dep");CHECK(r.state==1 && sameResult(&second,&r,kind) && builds(&r,kind)==3);releaseResult(&r);xlsWorkerDestroy(w);releaseResult(&first);releaseResult(&second);
 }
 xlsContextRelease(a);xlsContextRelease(b);xlsTextRelease(text);xrtFree(dep);xrtFree(project);clear(&f);return 1;
}
int main(void)
{
 if(!switching() || !evictionAndBudget() || !history() || !contexts() || !diskGuards() || !closedEntries())return 1;
 printf("analysis_worker_cache_test passed: %u checks;three semantic kinds,full fresh AST/typed IR and JSON parity,switching/LRU/budget/current-stream versions,close/reopen,context equality and held lifetime.\n",checks);return 0;
}
