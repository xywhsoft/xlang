#include <xrt.h>
#include <stdio.h>
#include <string.h>
static int refuse;
static void* referenceAllocate(size_t bytes){return refuse ? NULL : xrtMalloc(bytes);}
#define XIDE_REFERENCE_ALLOC referenceAllocate
#include "services/semantic_definition.c"
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"semantic_reference_shard_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
typedef struct fixture {xls_semantic_check* check;xide_semantic_definitions* table;xide_semantic_source sources[8];size_t count;xide_reference_shard* shard;} fixture;
static int capture(fixture* f,const char* entry,const xls_semantic_input* inputs,size_t count)
{
 f->check=xlsSemanticCheckCreate(entry,inputs,count,1,NULL,NULL,NULL,0);CHECK(f->check && xlsSemanticCheckInfo(f->check).accepted);xls_inputs info;CHECK(xlsSemanticCheckInputs(f->check,&info) && info.sources<=8);f->count=info.sources;
 for(size_t i=0;i<f->count;++i){xls_input_source s;CHECK(xlsSemanticCheckSource(f->check,i,&s));f->sources[i]=(xide_semantic_source){s.path,s.text,s.kind,1};}
 f->table=xideSemanticDefinitionsCreateTyped(xlsSemanticCheckTypedIndex(f->check),f->sources,f->count,NULL);CHECK(f->table);f->shard=xideSemanticReferenceShardCreate(NULL,f->check,f->sources,f->count,XIDE_REFERENCE_BYTES,NULL);CHECK(f->shard && xideSemanticReferenceShardSources(f->shard)==f->count);return 1;
}
static int same(const xide_semantic_reference* a,const xide_semantic_reference* b)
{return !strcmp(a->path,b->path) && a->start.offset==b->start.offset && a->end.offset==b->end.offset && a->start.line==b->start.line && a->start.character==b->start.character && a->end.line==b->end.line && a->end.character==b->end.character && a->declaration==b->declaration;}
static int oracle(fixture* f,const char* path,int line,int character,const xide_reference_key* key)
{
 for(int include=0;include<2;++include){xide_semantic_reference_view original;xide_reference_shard_view compact;CHECK(xideSemanticReferences(f->table,path,line,character,include,&original));CHECK(xideSemanticReferenceShardQuery(f->shard,key,include,&compact)==XIDE_REFERENCE_MATCH && compact.count==original.count);
  for(size_t i=0;i<original.count;++i){xide_semantic_reference a,b;CHECK(xideSemanticReferenceAt(&original,i,&a) && xideSemanticReferenceShardAt(&compact,i,&b) && same(&a,&b));}xide_semantic_reference bad;CHECK(!xideSemanticReferenceShardAt(&compact,compact.count,&bad) && !bad.path);
 }return 1;
}
static xide_reference_key* keyAt(fixture* f,const char* path,const char* raw,const char* needle)
{const char* at=strstr(raw,needle);int line,character;if(!at || !xideLspOffsetToPosition(raw,(int)(at-raw),&line,&character))return NULL;return xideSemanticReferenceKeyCreate(NULL,f->check,f->sources,f->count,path,line,character,XIDE_REFERENCE_BYTES,NULL);}
static int allPositions(fixture* f)
{
 for(size_t source=0;source<f->count;++source){xstrbuf raw={0};CHECK(xrtStrBufAppend(&raw,xrtStrView("\xef\xbb\xbf")) && xrtStrBufAppend(&raw,xrtStrView(f->sources[source].text)));
  for(size_t offset=0;offset<=raw.Size;++offset){int line,character;if(!xideLspOffsetToPosition(raw.Data,(int)offset,&line,&character))continue;xide_model_error error={0};xide_reference_key* key=xideSemanticReferenceKeyCreate(NULL,f->check,f->sources,f->count,f->sources[source].path,line,character,XIDE_REFERENCE_BYTES,&error);CHECK(!error.code);
   if(key){CHECK(oracle(f,f->sources[source].path,line,character,key));xideSemanticReferenceKeyRelease(key);}else{xide_semantic_reference_view original;CHECK(xideSemanticReferences(f->table,f->sources[source].path,line,character,1,&original) && !original.count);}
  }xrtStrBufFree(&raw);
 }return 1;
}
typedef struct reader {xide_reference_shard* shard;xide_reference_key* key;size_t count;int okay;} reader;
static int32 readShard(void* user)
{reader* r=user;r->okay=1;for(unsigned i=0;i<128;++i){xide_reference_shard_view v;if(xideSemanticReferenceShardQuery(r->shard,r->key,1,&v)!=XIDE_REFERENCE_MATCH || v.count!=r->count)r->okay=0;for(size_t j=0;j<v.count;++j){xide_semantic_reference at;if(!xideSemanticReferenceShardAt(&v,j,&at) || !at.path || at.end.offset<=at.start.offset)r->okay=0;}}xideSemanticReferenceKeyRelease(r->key);xideSemanticReferenceShardRelease(r->shard);return 0;}
static void releaseOwner(fixture* f){xideSemanticDefinitionsDestroy(f->table);f->table=NULL;xlsSemanticCheckDestroy(f->check);f->check=NULL;}
static int run(void)
{
 char* directory=xrtPathAbs("build/reference-shards");CHECK(directory && xrtDirCreateAll(directory));char *a=xrtPathJoin(directory,"入口.xl"),*b=xrtPathJoin(directory,"other.xl"),*c=xrtPathJoin(directory,"unimported.xl"),*dep=xrtPathJoin(directory,"dep.xl");CHECK(a && b && c && dep);
 const char* aText="\xef\xbb\xbf" "import dep\r\ntypedef Count = int\r\nclass Box<T>\r\n T field\r\nend\r\nfunc choose(int value) as int\r\n return value\r\nend\r\nfunc choose(string value) as int\r\n return 0\r\nend\r\nfunc main(...) as int\r\n Count 值=dep.compute(7); string text=\"😀\"\r\n int selected=choose(值)+choose(text)\r\n if true\r\n int 值=2\r\n selected=selected+值\r\n end\r\n return selected+值\r\nend\r\n";
 const char* bText="\xef\xbb\xbf" "import dep\nfunc independent(int compute) as int\n return compute\nend\nfunc main(...) as int\n return dep.compute(8)+dep.compute(9)\nend\n";
 const char* editedB="\xef\xbb\xbf" "import dep\n\nfunc independent(int compute) as int\n return compute\nend\nfunc main(...) as int\n return dep.compute(8)\nend\n";
 const char* cText="\xef\xbb\xbf" "func compute(int value) as int\n return value\nend\nfunc main(...) as int\n return compute(7)\nend\n";
 const char* depText="\xef\xbb\xbf" "export func compute(int value) as int\r\n return value+1\r\nend\r\n";
 const char* changedDep="\xef\xbb\xbf" "export func compute(int value) as int\r\n return value+2\r\nend\r\n";
 xls_semantic_input inputs[]={{a,aText,strlen(aText)},{b,bText,strlen(bText)},{c,cText,strlen(cText)},{dep,depText,strlen(depText)}};fixture first={0},second={0},unimported={0},edited={0},changed={0};CHECK(capture(&first,a,inputs,4) && capture(&second,b,inputs,4) && capture(&unimported,c,inputs,4));CHECK(allPositions(&first) && allPositions(&second) && allPositions(&unimported));
 xide_reference_key* key=keyAt(&first,a,aText,"compute(7)");CHECK(key);int dl,dc;CHECK(xideLspOffsetToPosition(depText,(int)(strstr(depText,"compute")-depText),&dl,&dc));CHECK(oracle(&second,dep,dl,dc,key));xide_reference_shard_view view;CHECK(xideSemanticReferenceShardQuery(second.shard,key,1,&view)==XIDE_REFERENCE_MATCH && view.count==3);CHECK(xideSemanticReferenceShardQuery(unimported.shard,key,1,&view)==XIDE_REFERENCE_MATCH && !view.count);
 xide_reference_key* local=keyAt(&first,a,aText,"value) as int");CHECK(local && xideSemanticReferenceShardQuery(second.shard,local,1,&view)==XIDE_REFERENCE_MATCH && !view.count);xideSemanticReferenceKeyRelease(local);
 inputs[1].text=editedB;inputs[1].size=strlen(editedB);CHECK(capture(&edited,b,inputs,4) && oracle(&edited,dep,dl,dc,key));CHECK(xideSemanticReferenceShardQuery(edited.shard,key,1,&view)==XIDE_REFERENCE_MATCH && view.count==2);
 inputs[3].text=changedDep;inputs[3].size=strlen(changedDep);CHECK(capture(&changed,b,inputs,4));CHECK(xideSemanticReferenceShardQuery(changed.shard,key,1,&view)==XIDE_REFERENCE_SOURCE_CHANGED && !view.shard);xide_reference_key* newKey=keyAt(&changed,b,editedB,"compute(8)");CHECK(newKey && xideSemanticReferenceShardQuery(second.shard,newKey,1,&view)==XIDE_REFERENCE_SOURCE_CHANGED && !view.shard);xideSemanticReferenceKeyRelease(newKey);
 size_t bytes=xideSemanticReferenceShardBytes(first.shard),keyBytes=xideSemanticReferenceKeyBytes(key);CHECK(bytes>keyBytes && keyBytes>strlen(depText));xide_reference_shard* exact=xideSemanticReferenceShardCreate(NULL,first.check,first.sources,first.count,bytes,NULL);CHECK(exact && xideSemanticReferenceShardBytes(exact)==bytes);xideSemanticReferenceShardRelease(exact);
 size_t budgets[]={0,1,bytes-1};for(size_t i=0;i<3;++i)CHECK(!xideSemanticReferenceShardCreate(NULL,first.check,first.sources,first.count,budgets[i],NULL));
 CHECK(!xideSemanticReferenceShardCreate(NULL,first.check,first.sources,first.count,SIZE_MAX,NULL));refuse=1;CHECK(!xideSemanticReferenceShardCreate(NULL,first.check,first.sources,first.count,XIDE_REFERENCE_BYTES,NULL) && !keyAt(&first,a,aText,"compute(7)"));refuse=0;
 xide_model_error error={0};CHECK(!xideSemanticReferenceKeyCreate(NULL,first.check,first.sources,first.count,a,12,13,keyBytes-1,&error) && error.code);error=(xide_model_error){0};CHECK(!xideSemanticReferenceKeyCreate(NULL,first.check,first.sources,first.count,a,0,1,XIDE_REFERENCE_BYTES,&error) && !error.code);
 const char* saved=first.sources[0].text;first.sources[0].text="different";CHECK(!xideSemanticReferenceShardCreate(NULL,first.check,first.sources,first.count,XIDE_REFERENCE_BYTES,NULL) && !keyAt(&first,a,aText,"compute(7)"));first.sources[0].text=saved;
 CHECK(!xideSemanticReferenceShardCreate(NULL,first.check,first.sources,first.count-1,XIDE_REFERENCE_BYTES,NULL));int kind=first.sources[0].kind;first.sources[0].kind=2;CHECK(!xideSemanticReferenceShardCreate(NULL,first.check,first.sources,first.count,XIDE_REFERENCE_BYTES,NULL));first.sources[0].kind=kind;
 /* Cross-owner equality ignores only the removed BOM, while every returned
    position retains that shard's captured conversion convention. */
 second.sources[1].utf16Prefix=0;xide_semantic_definitions* oldTable=second.table;xide_reference_shard* oldShard=second.shard;second.table=xideSemanticDefinitionsCreateTyped(xlsSemanticCheckTypedIndex(second.check),second.sources,second.count,NULL);second.shard=xideSemanticReferenceShardCreate(NULL,second.check,second.sources,second.count,XIDE_REFERENCE_BYTES,NULL);CHECK(second.table && second.shard && oracle(&second,dep,dl,dc,key));xideSemanticDefinitionsDestroy(oldTable);xideSemanticReferenceShardRelease(oldShard);
 /* Different lexical spellings still refer to the compiler's captured file. */
 char* alias=xrtPathJoin(directory,"unused/../dep.xl");CHECK(alias);saved=second.sources[1].path;second.sources[1].path=alias;exact=xideSemanticReferenceShardCreate(NULL,second.check,second.sources,second.count,XIDE_REFERENCE_BYTES,NULL);CHECK(exact && xideSemanticReferenceShardQuery(exact,key,1,&view)==XIDE_REFERENCE_MATCH && view.count==3);xideSemanticReferenceShardRelease(exact);second.sources[1].path=saved;xrtFree(alias);
 xls_projection_store* cache=xlsProjectionStoreCreate(XLS_PROJECTION_STORE_BYTES);CHECK(cache);xls_projection_stats stats;xide_reference_shard* cached=xideSemanticReferenceShardCreate(cache,first.check,first.sources,first.count,XIDE_REFERENCE_BYTES,NULL);CHECK(cached);xlsProjectionStoreStats(cache,&stats);uint64_t hits=stats.hits;local=xideSemanticReferenceKeyCreate(cache,first.check,first.sources,first.count,a,12,13,XIDE_REFERENCE_BYTES,NULL);CHECK(local);xlsProjectionStoreStats(cache,&stats);CHECK(stats.hits==hits+1);xideSemanticReferenceKeyRelease(local);xideSemanticReferenceShardRelease(cached);xlsProjectionStoreDestroy(cache);
 CHECK(!xideSemanticReferenceShardQuery(NULL,key,1,&view) && !view.shard);CHECK(!xideSemanticReferenceShardQuery(first.shard,key,2,&view) && !view.shard);CHECK(!xideSemanticReferenceShardQuery(first.shard,key,1,NULL));CHECK(xideSemanticReferenceShardQuery(first.shard,key,1,&view)==XIDE_REFERENCE_MATCH);view.first=SIZE_MAX;xide_semantic_reference invalid;CHECK(!xideSemanticReferenceShardAt(&view,0,&invalid) && !invalid.path);xide_semantic_source source;CHECK(!xideSemanticReferenceShardSource(first.shard,first.count,&source) && !source.path);
 for(size_t i=0;i<first.count;++i){CHECK(xideSemanticReferenceShardSource(first.shard,i,&source));CHECK(!strcmp(source.path,first.sources[i].path) && !strcmp(source.text,first.sources[i].text) && source.kind==first.sources[i].kind && source.utf16Prefix==1);}
 releaseOwner(&first);releaseOwner(&second);releaseOwner(&unimported);releaseOwner(&edited);releaseOwner(&changed);CHECK(xideSemanticReferenceShardQuery(second.shard,key,1,&view)==XIDE_REFERENCE_MATCH && view.count==3);
 reader readers[4];xthread* threads[4];for(size_t i=0;i<4;++i){readers[i]=(reader){xideSemanticReferenceShardRetain(second.shard),xideSemanticReferenceKeyRetain(key),view.count,0};threads[i]=xrtThreadCreate(readShard,readers+i,0);CHECK(threads[i]);}xideSemanticReferenceKeyRelease(key);xideSemanticReferenceShardRelease(second.shard);for(size_t i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}
 xideSemanticReferenceShardRelease(first.shard);xideSemanticReferenceShardRelease(unimported.shard);xideSemanticReferenceShardRelease(edited.shard);xideSemanticReferenceShardRelease(changed.shard);xrtFree(a);xrtFree(b);xrtFree(c);xrtFree(dep);xrtFree(directory);return 1;
}
int main(void){if(!run())return 1;printf("semantic_reference_shard_test passed: %u checks;exact cross-owner declaration sources,original full cursor/reference oracle,edits,shadowing,quota/fault/cache and independent four-reader lifetime\n",checks);return 0;}
