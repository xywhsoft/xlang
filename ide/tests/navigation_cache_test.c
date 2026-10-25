#include "services/semantic_definition.h"
#include "analysis/projection.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"navigation_cache_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static int equalValue(xvalue* a,xvalue* b)
{int same=a && b && xrtValueEqual(a,b);xrtValueRelease(a);xrtValueRelease(b);return same;}
static int sameReferences(xide_semantic_definitions* a,xide_semantic_definitions* b,const char* path,int line,int character)
{
 for(int declaration=0;declaration<2;++declaration){xide_semantic_reference_view x={0},y={0};CHECK(xideSemanticReferences(a,path,line,character,declaration,&x) && xideSemanticReferences(b,path,line,character,declaration,&y) && x.count==y.count);
  for(size_t i=0;i<x.count;++i){xide_semantic_reference p,q;CHECK(xideSemanticReferenceAt(&x,i,&p) && xideSemanticReferenceAt(&y,i,&q));CHECK(!strcmp(p.path,q.path) && p.start.offset==q.start.offset && p.end.offset==q.end.offset && p.start.line==q.start.line && p.start.character==q.start.character && p.end.line==q.end.line && p.end.character==q.end.character && p.declaration==q.declaration);}}
 return 1;
}
static int allQueries(xide_semantic_definitions* a,xide_semantic_definitions* b,const xide_semantic_source* sources,size_t count)
{
 for(size_t i=0;i<count;++i){CHECK(equalValue(xideSemanticDocumentSymbols(a,sources[i].path),xideSemanticDocumentSymbols(b,sources[i].path)));xstrbuf text={0};if(sources[i].utf16Prefix)CHECK(xrtStrBufAppend(&text,xrtStrView("\xef\xbb\xbf")));CHECK(xrtStrBufAppend(&text,xrtStrView(sources[i].text)));
  for(size_t offset=0;offset<=text.Size;++offset){int line,character;if(!xideLspOffsetToPosition(text.Data,(int)offset,&line,&character))continue;
   CHECK(equalValue(xideSemanticDefinition(a,sources[i].path,line,character),xideSemanticDefinition(b,sources[i].path,line,character)));
   CHECK(equalValue(xideSemanticHover(a,sources[i].path,line,character),xideSemanticHover(b,sources[i].path,line,character)));
   CHECK(equalValue(xideSemanticDocumentHighlights(a,sources[i].path,line,character),xideSemanticDocumentHighlights(b,sources[i].path,line,character)));
   CHECK(sameReferences(a,b,sources[i].path,line,character));
  }xrtStrBufFree(&text);
 }return 1;
}
typedef struct reader {xide_semantic_definitions* table;const char* path;int okay;} reader;
static int32 readTable(void* user)
{reader* r=user;r->okay=1;for(unsigned i=0;i<32;++i){xvalue* symbols=xideSemanticDocumentSymbols(r->table,r->path);if(!symbols || !xrtValueCount(symbols))r->okay=0;xrtValueRelease(symbols);}return 0;}
static int run(void)
{
 char* directory=xrtPathAbs("build/navigation-cache");CHECK(directory && xrtDirCreateAll(directory));char *entry=xrtPathJoin(directory,"入口.xl"),*dep=xrtPathJoin(directory,"dep.xl");CHECK(entry && dep);
 const char* mainText="\xef\xbb\xbf" "import dep\r\ntypedef Count = int\r\nclass Box<T>\r\n T field\r\nend\r\nfunc choose(int value) as int\r\n return value\r\nend\r\nfunc choose(string value) as int\r\n return 0\r\nend\r\nfunc main(...) as int\r\n Count 值=dep.compute(7); string text=\"😀\"\r\n int selected=choose(值)+choose(text)\r\n if true\r\n int 值=2\r\n selected=selected+值\r\n end\r\n return selected+值\r\nend\r\n";
 const char* depText="\xef\xbb\xbf" "export func compute(int value) as int\r\n return value+1\r\nend\r\n";xls_semantic_input inputs[2]={{entry,mainText,strlen(mainText)},{dep,depText,strlen(depText)}};xls_semantic_check* checked=xlsSemanticCheckCreate(entry,inputs,2,1,NULL,NULL,NULL,0);CHECK(checked && xlsSemanticCheckInfo(checked).accepted);
 xls_inputs info;CHECK(xlsSemanticCheckInputs(checked,&info) && info.sources==2);xide_semantic_source sources[2];for(size_t i=0;i<2;++i){xls_input_source row;CHECK(xlsSemanticCheckSource(checked,i,&row));sources[i]=(xide_semantic_source){row.path,row.text,row.kind,1};}
 xide_model_error error={0};xide_semantic_definitions* reference=xideSemanticDefinitionsCreateTyped(xlsSemanticCheckTypedIndex(checked),sources,2,&error);xls_projection_store* store=xlsProjectionStoreCreate(XLS_PROJECTION_STORE_BYTES);CHECK(reference && store);xls_projection *first=NULL,*hit=NULL,*changed=NULL,*restored=NULL;xide_navigation_work work;
 xide_semantic_definitions* a=xideSemanticDefinitionsAcquireTyped(store,checked,sources,2,&first,&work,&error);CHECK(a && first && work.built==1 && !work.hit && work.bytes>0 && allQueries(a,reference,sources,2));
 xide_semantic_definitions* b=xideSemanticDefinitionsAcquireTyped(store,checked,sources,2,&hit,&work,&error);CHECK(b==a && hit==first && work.hit==1 && !work.built && allQueries(b,reference,sources,2));
 /* Same check,changed BOM convention requires a different converted view. */
 sources[1].utf16Prefix=0;xide_semantic_definitions* changedReference=xideSemanticDefinitionsCreateTyped(xlsSemanticCheckTypedIndex(checked),sources,2,&error);xide_semantic_definitions* c=xideSemanticDefinitionsAcquireTyped(store,checked,sources,2,&changed,&work,&error);CHECK(changedReference && c && changed && c!=a && work.built && !work.hit && allQueries(c,changedReference,sources,2));xideSemanticDefinitionsDestroy(changedReference);
 sources[1].utf16Prefix=1;xide_semantic_definitions* d=xideSemanticDefinitionsAcquireTyped(store,checked,sources,2,&restored,&work,&error);CHECK(d && restored && work.built && !work.hit && allQueries(d,reference,sources,2));
 /* A matching key cannot bless forged source bytes or paths. */
 const char* saved=sources[0].text;char* forged=xrtStrDup(saved);CHECK(forged);char* at=strstr(forged,"selected+值");CHECK(at);at[0]='x';sources[0].text=forged;xls_projection* rejected=NULL;CHECK(!xideSemanticDefinitionsAcquireTyped(store,checked,sources,2,&rejected,&work,&error) && !rejected && !work.hit);sources[0].text=saved;xrtFree(forged);
 const char* savedPath=sources[0].path;sources[0].path=dep;CHECK(!xideSemanticDefinitionsAcquireTyped(store,checked,sources,2,&rejected,&work,&error) && !rejected);sources[0].path=savedPath;
 xls_projection_store* small=xlsProjectionStoreCreate(1);CHECK(small);xls_projection* refused=NULL;xide_semantic_definitions* e=xideSemanticDefinitionsAcquireTyped(small,checked,sources,2,&refused,&work,&error);CHECK(e && refused && work.built && allQueries(e,reference,sources,2));xls_projection_stats stats;xlsProjectionStoreStats(small,&stats);CHECK(!stats.entries && stats.declined==1);xideSemanticDefinitionsReleaseTyped(e,refused);xlsProjectionStoreDestroy(small);
 e=xideSemanticDefinitionsAcquireTyped(NULL,checked,sources,2,&refused,&work,&error);CHECK(e && !refused && work.built && allQueries(e,reference,sources,2));xideSemanticDefinitionsReleaseTyped(e,refused);
 xls_semantic_check* other=xlsSemanticCheckCreate(entry,inputs,2,1,NULL,NULL,NULL,0);CHECK(other && other!=checked && xlsSemanticCheckInfo(other).accepted);xls_projection* otherLease=NULL;e=xideSemanticDefinitionsAcquireTyped(store,other,sources,2,&otherLease,&work,&error);CHECK(e && otherLease && work.built && !work.hit && allQueries(e,reference,sources,2));xideSemanticDefinitionsReleaseTyped(e,otherLease);xlsSemanticCheckDestroy(other);
 /* Tables and their exact original compilation survive all original owners. */
 xideSemanticDefinitionsDestroy(reference);xlsSemanticCheckDestroy(checked);xlsProjectionStoreDestroy(store);reader readers[4];xthread* threads[4];for(size_t i=0;i<4;++i){readers[i]=(reader){a,entry,0};threads[i]=xrtThreadCreate(readTable,&readers[i],0);CHECK(threads[i]);}for(size_t i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}
 xideSemanticDefinitionsReleaseTyped(a,first);xideSemanticDefinitionsReleaseTyped(b,hit);xideSemanticDefinitionsReleaseTyped(c,changed);xideSemanticDefinitionsReleaseTyped(d,restored);xrtFree(entry);xrtFree(dep);xrtFree(directory);return 1;
}
int main(void){if(!run())return 1;printf("navigation_cache_test passed: %u checks;all valid UTF8 endpoints/full definition-hover-symbol-highlight-reference parity,overloads/types/imports/shadowing,complete owner/source/BOM fences,quota fallback and independent four-reader leases\n",checks);return 0;}
