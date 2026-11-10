#define main navigationCacheReferenceMain
#include "navigation_cache_test.c"
#undef main
#include "analysis/check_session.h"
#include "analysis/index_segments.h"
static int loanTracking;static size_t loans,loanReleases;static xls_projection* borrowedProjection;
xls_projection* __real_xlsProjectionAcquirePrevious(xls_projection_store*,xls_semantic_check*,const void*,size_t);
void __real_xlsProjectionRelease(xls_projection*);
xls_projection* __wrap_xlsProjectionAcquirePrevious(xls_projection_store* store,xls_semantic_check* check,const void* key,size_t size)
{ xls_projection* p=__real_xlsProjectionAcquirePrevious(store,check,key,size);if(loanTracking && p){++loans;borrowedProjection=p;}return p; }
void __wrap_xlsProjectionRelease(xls_projection* p)
{if(loanTracking && p && p==borrowedProjection){++loanReleases;borrowedProjection=NULL;}__real_xlsProjectionRelease(p);}
static int sourcesOf(xls_semantic_check* check,xide_semantic_source* sources,size_t* count,int prefix)
{
 xls_inputs inputs;CHECK(xlsSemanticCheckInputs(check,&inputs) && inputs.sources<=8);*count=inputs.sources;
 for(size_t i=0;i<*count;++i){xls_input_source row;CHECK(xlsSemanticCheckSource(check,i,&row));sources[i]=(xide_semantic_source){row.path,row.text,row.kind,prefix};}return 1;
}
static int comparePoints(xide_navigation_snapshot* snapshot,xls_semantic_check* check,const xide_semantic_source* sources,size_t count)
{
 xide_model_error error={0};xide_semantic_definitions* reference=xideSemanticDefinitionsCreateTyped(xlsSemanticCheckTypedIndex(check),sources,count,&error);
 CHECK(reference && !error.code && pointOracle(snapshot,reference,sources,count));xideSemanticDefinitionsDestroy(reference);return 1;
}
static int sequence(const char* const* bodies,size_t count,int prefix,int requireReuse,const char* dependency)
{
 char* path=xrtPathAbs("navigation-function-中文😀.xl");CHECK(path);
 xls_check_session* session=xlsCheckSessionCreate(NULL);xls_projection_store* store=xlsProjectionStoreCreate(XLS_PROJECTION_STORE_BYTES);CHECK(session && store);
 char* depPath=dependency ? xrtPathAbs("dep.xl") : NULL;xls_text* depText=dependency ? xlsTextCreate(dependency,strlen(dependency),8u*1024u*1024u,NULL) : NULL;
 CHECK(!dependency || (depPath && depText));xls_analysis_document documents[2]={{"entry",path,NULL,17001,0},{"dep",depPath,depText,17002,1}};xls_analysis_document* document=&documents[0];size_t reused=0;
 for(size_t step=0;step<count;++step){
  document->text=xlsTextCreate(bodies[step],strlen(bodies[step]),8u*1024u*1024u,NULL);++document->revision;CHECK(document->text);
  char error[256]={0};xls_semantic_check* check=xlsCheckSessionCheck(session,documents,dependency ? 2 : 1,"entry",1,NULL,NULL,error,sizeof(error));xlsTextRelease(document->text);document->text=NULL;
  if(!check || !xlsSemanticCheckInfo(check).accepted){char* json=check?xlsSemanticCheckJson(check,NULL):NULL;fprintf(stderr,"sequence %zu: %s %s\n",step,error,json?json:"");xrtFree(json);}
  CHECK(check && xlsSemanticCheckInfo(check).accepted);
  const xls_semantic_index* index=xlsSemanticCheckTypedIndex(check);size_t slots=xlsSemanticIndexSegmentCount(index),total=0;
  for(size_t slot=0;slot<slots;++slot){xls_index_segment segment;CHECK(xlsSemanticIndexSegment(index,slot,&segment));if(segment.count){CHECK(segment.first==total);total+=segment.count;}if(segment.reusable)CHECK(segment.identity && segment.count && segment.source && segment.proofFirst<=segment.proofEnd);}
  CHECK(!slots || total==xlsSemanticIndexCount(index));
  xide_semantic_source sources[8];size_t sourceCount;CHECK(sourcesOf(check,sources,&sourceCount,prefix));xide_model_error why={0};xide_navigation_snapshot* snapshot=xideSemanticNavigationCapture(check,sources,sourceCount,&why);CHECK(snapshot);
  loans=loanReleases=0;loanTracking=1;int prepared=xideSemanticNavigationBindAndPreparePoints(snapshot,store,&why);loanTracking=0;
  CHECK(prepared==1 && !why.code && loans==loanReleases && !borrowedProjection && comparePoints(snapshot,check,sources,sourceCount));
  xide_point_reuse_work work=xideSemanticNavigationPointReuseWork(snapshot);reused+=work.functions;
  printf("reuse step %zu: %zu functions / %zu facts / %zu cursors / %zu proof bytes; semantic %zu\n",step,work.functions,work.facts,work.cursors,work.proofBytes,xlsSemanticCheckWork(check).functionsReused);
  if(work.functions)CHECK(work.facts && work.cursors && work.proofBytes);
  xide_navigation_snapshot* hit=xideSemanticNavigationCapture(check,sources,sourceCount,&why);CHECK(hit && xideSemanticNavigationBindAndPreparePoints(hit,store,&why)==1 && xideSemanticNavigationPointWork(hit).hit);xideSemanticNavigationDestroy(hit);
  xideSemanticNavigationDestroy(snapshot);xlsSemanticCheckDestroy(check);
 }
 CHECK(!requireReuse || reused);xlsCheckSessionDestroy(session);xlsProjectionStoreDestroy(store);xlsTextRelease(depText);xrtFree(depPath);xrtFree(path);return 1;
}
#define STABLE "func helper(int 值) as int\r\n int 本地=值+1\r\n 本地=本地+1\r\n 本地=本地+1\r\n 本地=本地+1\r\n 本地=本地+1\r\n 本地=本地+1\r\n 本地=本地+1\r\n 本地=本地+1\r\n 本地=本地+1\r\n 本地=本地+1\r\n 本地=本地+1\r\n 本地=本地+1\r\n 本地=本地+1\r\n 本地=本地+1\r\n 本地=本地+1\r\n 本地=本地+1\r\n 本地=本地+1\r\n return 本地\r\nend\r\n"
#define MAIN1 "func main(...) as int\r\n return helper(1)\r\nend\r\n"
#define MAIN2 "func main(...) as int\r\n return helper(2)\r\nend\r\n"
static int reuseFaults(void)
{
 char* path=xrtPathAbs("navigation-function-faults.xl");CHECK(path);xls_check_session* session=xlsCheckSessionCreate(NULL);CHECK(session);
 const char* bodies[]={STABLE MAIN1,STABLE MAIN2};xls_semantic_check* owners[2]={0};xide_semantic_source sources[2][8];size_t counts[2];
 for(size_t i=0;i<2;++i){xls_text* text=xlsTextCreate(bodies[i],strlen(bodies[i]),8u*1024u*1024u,NULL);CHECK(text);xls_analysis_document doc={"entry",path,text,8800,i+1};owners[i]=xlsCheckSessionCheck(session,&doc,1,"entry",1,NULL,NULL,NULL,0);xlsTextRelease(text);CHECK(owners[i] && xlsSemanticCheckInfo(owners[i]).accepted && sourcesOf(owners[i],sources[i],&counts[i],0));}
 xide_model_error error={0};xide_semantic_definitions* reference=xideSemanticDefinitionsCreateTyped(xlsSemanticCheckTypedIndex(owners[1]),sources[1],counts[1],&error);CHECK(reference);
 for(unsigned kind=1;kind<=11;++kind){if(kind==2)continue;size_t total=0;
  for(size_t position=SIZE_MAX;;){
   uint64_t blocks=liveBlocks();xls_projection_store* store=xlsProjectionStoreCreate(XLS_PROJECTION_STORE_BYTES);CHECK(store);
   xide_navigation_snapshot* old=xideSemanticNavigationCapture(owners[0],sources[0],counts[0],&error);CHECK(old && xideSemanticNavigationBindAndPreparePoints(old,store,&error)==1);xideSemanticNavigationDestroy(old);
   xide_navigation_snapshot* current=xideSemanticNavigationCapture(owners[1],sources[1],counts[1],&error);CHECK(current);
   captureFaultKind=kind;captureFaultAt=position;captureVisits=captureRefusals=0;loans=loanReleases=0;
   captureFaultActive=pointTrackAlloc=loanTracking=1;int prepared=xideSemanticNavigationBindAndPreparePoints(current,store,&error);captureFaultActive=pointTrackAlloc=loanTracking=0;
   CHECK(prepared>=0 && !error.code && loans==loanReleases && !borrowedProjection);
   if(position==SIZE_MAX){total=captureVisits;CHECK(prepared && !captureRefusals && xideSemanticNavigationPointReuseWork(current).functions);}
   else CHECK(captureRefusals==1);
   if(prepared)CHECK(pointOracle(current,reference,sources[1],counts[1]));
   else {xls_projection* lease=NULL;xide_navigation_work work;xide_semantic_definitions* fallback=xideSemanticNavigationResolve(current,NULL,&lease,&work,&error);CHECK(fallback && !error.code && allQueries(fallback,reference,sources[1],counts[1]));xideSemanticDefinitionsReleaseTyped(fallback,lease);}
   xideSemanticNavigationDestroy(current);xlsProjectionStoreDestroy(store);CHECK(!pointCrtCount && liveBlocks()==blocks);
   if(position==SIZE_MAX)position=0;else ++position;if(position>=total)break;
  }
  printf("reuse fault API %u: %zu actual allocation positions\n",kind,total);
 }
 xideSemanticDefinitionsDestroy(reference);xlsSemanticCheckDestroy(owners[0]);xlsSemanticCheckDestroy(owners[1]);xlsCheckSessionDestroy(session);xrtFree(path);return 1;
}
static int runReuse(void)
{
 const char* stable[]={STABLE MAIN1,STABLE MAIN2,STABLE MAIN1};CHECK(sequence(stable,3,0,1,NULL));
 const char* bom[]={"\xef\xbb\xbf" STABLE MAIN1,"\xef\xbb\xbf" STABLE MAIN2};CHECK(sequence(bom,2,1,1,NULL));
 /* Same byte length, different UTF16 prefix. No old source coordinate may escape. */
 const char* unicode[]={"/*😀ab*/" STABLE MAIN1,"/*中文*/" STABLE MAIN2,"/*😀ab*/" STABLE MAIN1};CHECK(sequence(unicode,3,0,0,NULL));
 const char* shifted[]={STABLE MAIN1,"\n" STABLE MAIN2,"//abcdef\n" STABLE MAIN1,STABLE MAIN2};CHECK(sequence(shifted,4,0,0,NULL));
 /* New facts before an unchanged function move its preorder ordinal. */
 const char* ordinals[]={"func first(int x) as int\n return  x+1\nend\n" STABLE MAIN1,"func first(int x) as int\n return +x+1\nend\n" STABLE MAIN2};CHECK(sequence(ordinals,2,0,1,NULL));
 const char* endings[]={STABLE "func main(...) as int\r\n return helper(1)\r\nend",STABLE "func main(...) as int\r\n return helper(2)\r\nend\r\n",STABLE "func main(...) as int\r\n return helper(3)\r\nend\r"};CHECK(sequence(endings,3,0,0,NULL));
 const char* rich="typedef Count = int\r\nclass Box<T>\r\n T field\r\nend\r\nfunc choose(int value) as int\r\n if value>0 then\r\n return value\r\n end\r\n return 0\r\nend\r\nfunc choose(string value) as int\r\n return 1\r\nend\r\nfunc helper(int 值) as int\r\n Count local=choose(值)\r\n string text=\"😀\"\r\n int selected=choose(text)+local\r\n if true\r\n int local=2\r\n selected=selected+local\r\n end\r\n return selected+local\r\nend\r\n";
 xstrbuf a={0},b={0};CHECK(xrtStrBufAppend(&a,xrtStrView(rich)) && xrtStrBufAppend(&a,xrtStrView(MAIN1)) && xrtStrBufAppend(&b,xrtStrView(rich)) && xrtStrBufAppend(&b,xrtStrView(MAIN2)));const char* overloads[]={a.Data,b.Data,a.Data};CHECK(sequence(overloads,3,0,1,NULL));xrtStrBufFree(&a);xrtStrBufFree(&b);
 const char* imported[]={"import dep\r\nfunc main(...) as int\r\n return dep.helper(1)\r\nend\r\n","import dep\r\nfunc main(...) as int\r\n return dep.helper(2)\r\nend\r\n"};CHECK(sequence(imported,2,0,0,"export " STABLE));
 const char* internalImports[]={"import dep\r\nfunc main(...) as int\r\n return dep.compute(1)\r\nend\r\n","import dep\r\nfunc main(...) as int\r\n return dep.compute(2)\r\nend\r\n"};CHECK(sequence(internalImports,2,0,1,STABLE "export func compute(int value) as int\r\n return helper(value)\r\nend\r\n"));
 CHECK(!xlsSemanticIndexSegmentCount(NULL) && !xlsSemanticIndexSegment(NULL,0,NULL));return 1;
}
int main(void)
{ xrtMemStatsEnable(true);if(!runReuse() || !reuseFaults())return 1;printf("navigation_function_reuse_test passed: %u checks; unchanged final functions,current UTF16/source proof,full point parity,ordinal shifts,BOM/CRLF,scoped prior leases\n",checks);return 0; }
