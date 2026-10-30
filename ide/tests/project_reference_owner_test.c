#include "analysis_test_clock.h"
#include "services/project_references.h"
#include "protocol/lsp_wire.h"
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"project_reference_owner_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
typedef struct row {char* uri;int sl,sc,el,ec;} row;
static xstrview sv(const char* s){return(xstrview){s,strlen(s)};}
static const xvalue* get(const xvalue* v,const char* k){return xrtValueObjectGet(v,sv(k));}
static int number(const xvalue* v){int64 n=-1;return xrtValueGetInt(v,&n)?(int)n:-1;}
static int order(const void* a,const void* b){const row*x=a,*y=b;int r=strcmp(x->uri,y->uri);if(!r)r=(x->sl>y->sl)-(x->sl<y->sl);if(!r)r=(x->sc>y->sc)-(x->sc<y->sc);if(!r)r=(x->el>y->el)-(x->el<y->el);return r?r:(x->ec>y->ec)-(x->ec<y->ec);}
static int writeText(const char* p,const char* s){return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,strlen(s)});}
static void clearRows(xarray* a){for(size_t i=0;i<a->Count;++i)free(((row*)a->Data)[i].uri);xrtArrayUnit(a);}
/* Explicit intended compilation owners are fixture data. Every expected range
 * comes from independent original full semantic/IR checks,not source scanning
 * or the candidate coordinator/cache. Missing query sources contribute zero. */
static int oracle(const char* const* roots,size_t count,const xls_semantic_input* inputs,size_t inputCount,const char* query,int line,int column,int include,xarray* expected)
{
 CHECK(xrtArrayInit(expected,sizeof(row)));
 for(size_t r=0;r<count;++r){xls_semantic_check* full=xlsSemanticCheckCreate(roots[r],inputs,inputCount,1,NULL,NULL,NULL,0);CHECK(full && xlsSemanticCheckInfo(full).accepted);xls_check_inputs info;CHECK(xlsSemanticCheckInputs(full,&info) && info.sources<=16);xide_semantic_source sources[16];int owned=0;
  for(size_t i=0;i<info.sources;++i){xls_check_source s;CHECK(xlsSemanticCheckSource(full,i,&s));sources[i]=(xide_semantic_source){s.path,s.text,s.kind,0};owned|=!strcmp(s.path,query);}
  if(owned){xide_semantic_definitions* definitions=xideSemanticDefinitionsCreateTyped(xlsSemanticCheckTypedIndex(full),sources,info.sources,NULL);CHECK(definitions);xide_semantic_reference_view view;CHECK(xideSemanticReferences(definitions,query,line,column,include,&view));
   for(size_t i=0;i<view.count;++i){xide_semantic_reference ref;CHECK(xideSemanticReferenceAt(&view,i,&ref));row value={xideLspPathToUri(ref.path,NULL),ref.start.line,ref.start.character,ref.end.line,ref.end.character};CHECK(value.uri);int found=0;for(size_t j=0;j<expected->Count;++j)found|=!order(xrtArrayConstGet(expected,j),&value);if(found)free(value.uri);else CHECK(xrtArrayPush(expected,&value));}
   xideSemanticDefinitionsDestroy(definitions);
  }xlsSemanticCheckDestroy(full);
 }CHECK(xrtArraySort(expected,order));return 1;
}
static int compare(const xvalue* result,const xarray* wanted,int proof)
{
 CHECK(result && xrtValueCount(result)==wanted->Count);xarray actual={0};CHECK(xrtArrayInit(&actual,sizeof(row)));
 for(size_t i=0;i<xrtValueCount(result);++i){const xvalue* item=xrtValueArrayGet(result,i);xstrview uri={0};CHECK(xrtValueGetString(get(item,"uri"),&uri));const xvalue* range=get(item,"range"),*a=get(range,"start"),*b=get(range,"end");row value={malloc(uri.Size+1),number(get(a,"line")),number(get(a,"character")),number(get(b,"line")),number(get(b,"character"))};CHECK(value.uri);memcpy(value.uri,uri.Data,uri.Size);value.uri[uri.Size]=0;CHECK(xrtArrayPush(&actual,&value));CHECK(!proof || get(item,"xlang"));}
 CHECK(xrtArraySort(&actual,order));for(size_t i=0;i<wanted->Count;++i)CHECK(!order(xrtArrayConstGet(&actual,i),xrtArrayConstGet(wanted,i)));clearRows(&actual);return 1;
}

typedef struct observed {enum xide_reference_query_state state;xvalue* locations;xide_project_reference_stats stats;char error[512];} observed;
static observed query(xide_project_references* store,xls_context* context,const xls_analysis_document* docs,size_t count,const char* path,int line,int col,int include,int proof)
{
 observed r={0};xide_reference_query* q=xideProjectReferencesSubmitWithProof(store,context,docs,count,docs[0].key,path,line,col,include,proof);const xvalue* locations=NULL;uint64_t until=xlsTestClockMicros()+15000000;
 do{r.state=xideProjectReferencesResult(q,&locations,&r.stats,r.error,sizeof(r.error));if(r.state!=XIDE_REFERENCES_PENDING)break;xlsTestSleepMicros(100);}while(xlsTestClockMicros()<until);
 if(r.state==XIDE_REFERENCES_READY)r.locations=xideProjectReferencesTake(q);
 xideProjectReferencesQueryDestroy(q);return r;
}
static int run(int mode)
{
 char name[128];snprintf(name,sizeof(name),"归属-cases-%d-%llu",mode,(unsigned long long)xlsTestClockMicros());char* dir=xrtPathAbs(name);CHECK(dir && xrtDirCreateAll(dir));
 char *entry=xrtPathJoin(dir,"middle.xl"),*lib=xrtPathJoin(dir,"a_library.xl"),*first=xrtPathJoin(dir,"z_importer.xl"),*other=xrtPathJoin(dir,"zz_importer.xl"),*project=xrtPathJoin(dir,"project.xlp"),*compiler=xrtPathJoin(dir,"absent.exe");CHECK(entry && lib && first && other && project && compiler);
 const char *main="func main(...) as int\n return 0\nend\n",*library="export func main(int value) as int\n return value+1\nend\n",*owner="import a_library as api\nfunc main(...) as int\n return api.main(7)\nend\n",*second="import a_library as api\nfunc main(...) as int\n return api.main(8)\nend\n";
 CHECK(writeText(entry,main) && writeText(lib,library) && writeText(first,owner) && writeText(other,second));
 const char* members[]={entry,lib,first,other};xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=project,.configuration="debug",.entry=entry,.directory=dir,.targetKind="exe",.analysisProfile="xlang-ls/compiler-xrt-v1"};spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){members,4};xls_context* context=xlsContextCreate(&spec,1024*1024,NULL);xide_toolchains* cache=xideToolchainsCreate();xide_project_references* store=xideProjectReferencesCreate(cache,compiler,NULL);CHECK(context && cache && store);
 const char* path=mode==1 ? entry : lib;const char* body=mode==1?main:library;int col=mode==1?6:13;const char* owners[]={entry,first,other};xls_semantic_input inputs[]={ {path,body,strlen(body)} };xls_text* text=xlsTextCreate(body,strlen(body),1024*1024,NULL);CHECK(text);xls_analysis_document docs[]={{"query",path,text,1,1}};
 for(unsigned include=0;include<2;++include){xarray wanted={0};CHECK(oracle(owners,3,inputs,1,path,0,col,(int)include,&wanted));CHECK(wanted.Count==(mode==1?include:2u+include));
  for(unsigned round=0;round<2;++round){observed r=query(store,context,docs,1,path,0,col,(int)include,round==1);CHECK(r.state==XIDE_REFERENCES_READY && compare(r.locations,&wanted,round==1));
   CHECK(r.stats.roots==3 && r.stats.locations==wanted.Count && r.stats.thread!=xrtThreadCurrentId());
   if(mode==1)CHECK(!r.stats.ownerProbes && r.stats.sourceRejections==(!include && !round ? 1u : 0u));
   else if(!include && !round)CHECK(r.stats.ownerProbes>=2);
   else CHECK(!r.stats.ownerProbes && r.stats.ownerRouteHits>=2);
   printf("{\"mode\":%d,\"include\":%u,\"round\":%u,\"locations\":%zu,\"roots\":%zu,\"captured\":%zu,\"reused\":%zu,\"ownerProbes\":%zu,\"routeHits\":%zu,\"sourceRejections\":%zu}\n",mode,include,round,wanted.Count,r.stats.roots,r.stats.captured,r.stats.reused,r.stats.ownerProbes,r.stats.ownerRouteHits,r.stats.sourceRejections);fflush(stdout);xrtValueRelease(r.locations);
  }clearRows(&wanted);
 }
 /* A genuine uncovered type error still rejects the project,including a
    warmed cache. A covered library's type error invalidates its real owners. */
 CHECK(writeText(other,"func main(...) as int\n return \"bad\"\nend\n"));observed bad=query(store,context,docs,1,path,0,col,1,0);CHECK(bad.state==XIDE_REFERENCES_FAILED && !bad.locations && *bad.error);
 CHECK(writeText(other,second));bad=query(store,context,docs,1,path,0,col,1,0);CHECK(bad.state==XIDE_REFERENCES_READY);xrtValueRelease(bad.locations);
 if(mode==1){CHECK(writeText(lib,"export func main(int value) as int\n return \"bad\"\nend\n"));bad=query(store,context,docs,1,path,0,col,1,0);CHECK(bad.state==XIDE_REFERENCES_FAILED && !bad.locations);}
 /* Configured primary errors remain errors;the mutation matrix also proves
    this when another member successfully imports that primary as a library. */
 if(mode==0){const char* invalid="export func main(int value) as int\n return value\nend\n";CHECK(writeText(entry,invalid));bad=query(store,context,docs,1,path,0,col,1,0);CHECK(bad.state==XIDE_REFERENCES_FAILED && !bad.locations);}
 xideProjectReferencesDestroy(store);xideToolchainsDestroy(cache);xlsContextRelease(context);xlsTextRelease(text);xrtFree(dir);xrtFree(entry);xrtFree(lib);xrtFree(first);xrtFree(other);xrtFree(project);xrtFree(compiler);return 1;
}
static int mutationCheck(xide_project_references* store,xls_context* context,const char* const* owners,const xls_analysis_document* docs,size_t count,const char* path,int line)
{
 xls_semantic_input inputs[4];for(size_t i=0;i<count;++i)inputs[i]=(xls_semantic_input){docs[i].path,xlsTextData(docs[i].text),xlsTextSize(docs[i].text)};
 xarray expected={0};CHECK(oracle(owners,3,inputs,count,path,line,13,1,&expected));CHECK(expected.Count==3);
 observed r=query(store,context,docs,count,path,line,13,1,1);if(r.state!=XIDE_REFERENCES_READY)fprintf(stderr,"mutation query:%s\n",r.error);CHECK(r.state==XIDE_REFERENCES_READY && compare(r.locations,&expected,1));xrtValueRelease(r.locations);clearRows(&expected);return 1;
}
static int mutations(void)
{
 char name[128];snprintf(name,sizeof(name),"变更-%llu",(unsigned long long)xlsTestClockMicros());char* dir=xrtPathAbs(name);CHECK(dir && xrtDirCreateAll(dir));char *entry=xrtPathJoin(dir,"middle.xl"),*lib=xrtPathJoin(dir,"a_library.xl"),*first=xrtPathJoin(dir,"z_importer.xl"),*second=xrtPathJoin(dir,"zz_importer.xl"),*project=xrtPathJoin(dir,"project.xlp"),*compiler=xrtPathJoin(dir,"absent.exe"),*unused=xrtPathJoin(dir,"unused.xl");CHECK(entry && lib && first && second && project && compiler && unused);
 const char *main="func main(...) as int\n return 0\nend\n",*library="export func main(int value) as int\n return value+1\nend\n",*shifted="\nexport func main(int value) as int\n return value+2\nend\n",*owner="import a_library as api\nfunc main(...) as int\n return api.main(7)\nend\n";CHECK(writeText(entry,main) && writeText(lib,library) && writeText(first,owner) && writeText(second,owner));const char* members[]={entry,lib,first,second};const char* owners[]={entry,first,second};xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=project,.configuration="debug",.entry=entry,.directory=dir,.targetKind="exe",.analysisProfile="xlang-ls/compiler-xrt-v1"};spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){members,4};xls_context* context=xlsContextCreate(&spec,1024*1024,NULL);xide_toolchains* cache=xideToolchainsCreate();xide_project_references* store=xideProjectReferencesCreate(cache,compiler,NULL);xls_text *text=xlsTextCreate(library,strlen(library),1024*1024,NULL),*edit=xlsTextCreate(shifted,strlen(shifted),1024*1024,NULL),*entryText=xlsTextCreate(main,strlen(main),1024*1024,NULL),*unread=xlsTextCreate("broken unused",13,1024*1024,NULL);CHECK(context && cache && store && text && edit && entryText && unread);xls_analysis_document docs[2]={{"query",lib,text,1,1},{"unused",unused,unread,2,1}};
 CHECK(mutationCheck(store,context,owners,docs,1,lib,0));CHECK(mutationCheck(store,context,owners,docs,2,lib,0));
 docs[0].text=edit;docs[0].revision=2;CHECK(mutationCheck(store,context,owners,docs,2,lib,1));
 /* An unsaved authoritative library can differ from disk;both original fresh
    checks and actual-owner discovery must use that same current text. */
 CHECK(writeText(lib,"export func main(int value) as int\n return value+99\nend\n"));CHECK(mutationCheck(store,context,owners,docs,2,lib,1));
 const char* unlisted[]={entry,first,second};spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){unlisted,3};xls_context* changed=xlsContextCreate(&spec,1024*1024,NULL);CHECK(changed && mutationCheck(store,changed,owners,docs,2,lib,1));xlsContextRelease(context);context=changed;
 spec.entry=first;changed=xlsContextCreate(&spec,1024*1024,NULL);CHECK(changed && mutationCheck(store,changed,owners,docs,2,lib,1));xlsContextRelease(context);context=changed;
 spec.configuration="release";changed=xlsContextCreate(&spec,1024*1024,NULL);CHECK(changed && mutationCheck(store,changed,owners,docs,2,lib,1));xlsContextRelease(context);context=changed;
 /* Closed query source is real disk with another actual open trigger. No
    synthetic editor input is introduced. Zero-open service requests remain
    outside this existing adapter contract and still reject explicitly. */
 docs[0]=(xls_analysis_document){"entry",entry,entryText,9,1};CHECK(writeText(lib,library));CHECK(mutationCheck(store,context,owners,docs,1,lib,0));
 CHECK(!xideProjectReferencesSubmit(store,context,NULL,0,"entry",lib,0,13,1));
 docs[0]=(xls_analysis_document){"query",lib,edit,100,1};CHECK(mutationCheck(store,context,owners,docs,1,lib,1));
 /* Wrong current content at an already observed revision must fail even when
    an old owner route exists. Reopen identity and later revision recover. */
 docs[0].text=text;observed stale=query(store,context,docs,1,lib,0,13,1,0);CHECK(stale.state==XIDE_REFERENCES_FAILED && !stale.locations);docs[0].revision=2;CHECK(mutationCheck(store,context,owners,docs,1,lib,0));
 /* Real program role remains required for the configured primary even when
    another member successfully imports that same file as a library. */
 const char* primaryExport="export func main(int value) as int\n return value\nend\n";const char* importingPrimary="import z_importer as program\nimport a_library as api\nfunc main(...) as int\n return program.main(7)+api.main(8)\nend\n";CHECK(writeText(first,primaryExport) && writeText(second,importingPrimary));xls_semantic_input input={lib,library,strlen(library)};xls_semantic_check* importer=xlsSemanticCheckCreate(second,&input,1,1,NULL,NULL,NULL,0);CHECK(importer && xlsSemanticCheckInfo(importer).accepted);xlsSemanticCheckDestroy(importer);stale=query(store,context,docs,1,lib,0,13,1,0);CHECK(stale.state==XIDE_REFERENCES_FAILED && !stale.locations);
 xideProjectReferencesDestroy(store);xideToolchainsDestroy(cache);xlsContextRelease(context);xlsTextRelease(text);xlsTextRelease(edit);xlsTextRelease(entryText);xlsTextRelease(unread);xrtFree(dir);xrtFree(entry);xrtFree(lib);xrtFree(first);xrtFree(second);xrtFree(project);xrtFree(compiler);xrtFree(unused);return 1;
}
int main(void){if(!run(0) || !run(1) || !mutations())return 1;printf("project_reference_owner_test passed: %u checks;complete original fresh Location sets,alternative importers,source coverage,real errors,Unicode/proofs,overlay/disk/context/close/reopen/version and primary program role\n",checks);return 0;}
