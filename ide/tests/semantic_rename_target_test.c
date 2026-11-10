#include "services/semantic_rename.h"
#include "analysis/text.h"
#include <stdio.h>
#include <string.h>
static int refuseAllocation;
static size_t allocationCalls;
static void* renameAllocate(size_t bytes){++allocationCalls;return refuseAllocation?NULL:xrtCalloc(1,bytes);}
#define XIDE_RENAME_ALLOC renameAllocate
#include "services/semantic_rename.c"
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"semantic_rename_target_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
typedef struct fixture {xls_semantic_check* check;xls_projection_store* cache;xide_semantic_source sources[8];size_t count;} fixture;
static int capture(fixture* f,const char* entry,const xls_semantic_input* inputs,size_t count)
{
 f->check=xlsSemanticCheckCreate(entry,inputs,count,1,NULL,NULL,NULL,0);CHECK(f->check);
 xls_check_info ci=xlsSemanticCheckInfo(f->check);if(!ci.accepted){for(size_t i=0;i<ci.diagnostics;++i){xls_check_diagnostic d;xlsSemanticCheckDiagnostic(f->check,i,&d);fprintf(stderr,"%s:%d:%d %s\n",d.path,d.line,d.column,d.message);}}
 CHECK(ci.accepted);xls_inputs info;CHECK(xlsSemanticCheckInputs(f->check,&info) && info.sources<=8);f->count=info.sources;
 for(size_t i=0;i<f->count;++i){xls_input_source s;CHECK(xlsSemanticCheckSource(f->check,i,&s));int prefix=0;for(size_t j=0;j<count;++j)if(!strcmp(s.path,inputs[j].path))prefix=inputs[j].size>=3 && !memcmp(inputs[j].text,"\xef\xbb\xbf",3);f->sources[i]=(xide_semantic_source){s.path,s.text,s.kind,prefix};}
 f->cache=xlsProjectionStoreCreate(XLS_PROJECTION_STORE_BYTES);CHECK(f->cache);return 1;
}
static void release(fixture* f){xlsProjectionStoreDestroy(f->cache);xlsSemanticCheckDestroy(f->check);f->cache=NULL;f->check=NULL;}
static xide_rename_target* at(fixture* f,const char* path,const char* raw,size_t offset,size_t budget,enum xide_rename_target_error* error)
{xls_text* text=xlsTextCreate(raw,strlen(raw),1024*1024,NULL);xls_position p;if(!text || !xlsTextPosition(text,offset,&p)){xlsTextRelease(text);return NULL;}xlsTextRelease(text);return xideRenameTargetCreate(f->cache,f->check,f->sources,f->count,path,(int)p.line,(int)p.character,budget,NULL,NULL,error);}
static int target(fixture* f,const char* path,const char* raw,const char* needle,size_t inside,const char* name,xide_rename_target** save)
{
 const char* found=strstr(raw,needle);CHECK(found);enum xide_rename_target_error error;
 xide_rename_target* t=at(f,path,raw,(size_t)(found-raw)+inside,0,&error);CHECK(t && !error);xide_rename_target_info info;CHECK(xideRenameTargetInfo(t,&info));CHECK(!strcmp(info.path,path) && info.name.Size==strlen(name) && !memcmp(info.name.Data,name,info.name.Size));
 size_t bom=!memcmp(raw,"\xef\xbb\xbf",3)?3:0;size_t first=(size_t)(found-raw),last=first+strlen(name);xls_text* text=xlsTextCreate(raw,strlen(raw),1024*1024,NULL);xls_position a,b;CHECK(text && xlsTextPosition(text,first,&a) && xlsTextPosition(text,last,&b));CHECK(info.start.offset==(int)(first-bom) && info.end.offset==(int)(last-bom) && info.start.line==(int)a.line && info.start.character==(int)a.character && info.end.line==(int)b.line && info.end.character==(int)b.character);xlsTextRelease(text);
 CHECK(info.declarationEnd>info.declarationStart && info.declarationEnd-info.declarationStart==info.name.Size && !memcmp(info.declaration.text+info.declarationStart,name,strlen(name)));
 xide_reference_key* key=xideRenameTargetRetainKey(t);xide_semantic_source decl;size_t start,end;CHECK(key && xideSemanticReferenceKeyTarget(key,&decl,&start,&end));CHECK(!strcmp(decl.path,info.declaration.path) && start==info.declarationStart && end==info.declarationEnd && !strcmp(decl.text,info.declaration.text));xideSemanticReferenceKeyRelease(key);
 xide_rename_target* exact=at(f,path,raw,(size_t)(found-raw)+inside,info.bytes,&error);CHECK(exact && !error);xideRenameTargetRelease(exact);CHECK(!at(f,path,raw,(size_t)(found-raw)+inside,info.bytes-1,&error) && error==XIDE_RENAME_TARGET_LIMIT);
 if(save)*save=t;else xideRenameTargetRelease(t);return 1;
}
typedef struct cancel {int calls,stop;} cancel;
static int cancelled(void* user){cancel* c=user;return ++c->calls>=c->stop;}
static int run(void)
{
 char* directory=xrtPathAbs("build/rename-target");CHECK(directory && xrtDirCreateAll(directory));char *a=xrtPathJoin(directory,"入口.xl"),*b=xrtPathJoin(directory,"other.xl"),*dep=xrtPathJoin(directory,"dep.xl");CHECK(a && b && dep);
 const char* raw="\xef\xbb\xbf" "import dep\r\ntypedef Count = int\r\nclass Box<T>\r\n T field\r\nend\r\nfunc choose(int 参数) as int\r\n int 值=参数\r\n if true\r\n int 值=7\r\n print(值)\r\n end\r\n return 值\r\nend\r\nfunc main(...) as int\r\n string text=\"😀\"; Count 😀结果=choose(3)\r\n return dep.compute(😀结果)\r\nend\r\n";
 const char* other="import dep\nfunc main(...) as int\n return dep.compute(8)\nend\n";
 const char* library="export func compute(int value) as int\r\n return value+1\r\nend\r\n";
 xls_semantic_input inputs[]={{a,raw,strlen(raw)},{b,other,strlen(other)},{dep,library,strlen(library)}};fixture f={0},g={0};CHECK(capture(&f,a,inputs,3) && capture(&g,b,inputs,3));
 xide_rename_target *outer=NULL,*inner=NULL,*compute=NULL,*emoji=NULL;
 CHECK(target(&f,a,raw,"choose(3)",2,"choose",NULL));CHECK(target(&f,a,raw,"参数\r",3,"参数",NULL));CHECK(target(&f,a,raw,"值=参数",0,"值",&outer));CHECK(target(&f,a,raw,"值=7",0,"值",&inner));CHECK(target(&f,a,raw,"值)\r",0,"值",NULL));CHECK(target(&f,a,raw,"值\r\nend",0,"值",NULL));
 CHECK(target(&f,a,raw,"😀结果=",4,"😀结果",&emoji));CHECK(target(&f,a,raw,"😀结果)",0,"😀结果",NULL));CHECK(target(&f,a,raw,"compute(😀",3,"compute",&compute));CHECK(target(&f,a,raw,"T field",0,"T",NULL));CHECK(target(&f,a,raw,"Box<T>",1,"Box",NULL));CHECK(target(&f,a,raw,"Count 😀",1,"Count",NULL));
 /* The actual native frontend has no declaration identity for this unused
    generic field. Fail closed, and retain this explicit pending coverage. */
 const xls_semantic_index* index=xlsSemanticCheckTypedIndex(f.check);int missingField=0;
 for(size_t i=0;i<xlsSemanticIndexCount(index);++i){xls_index_fact fact;CHECK(xlsSemanticIndexFact(index,i,&fact));if(fact.source && !strcmp(fact.kind,"field") && fact.tokenRange.startOffset==50){CHECK(!fact.declaredSymbol && !fact.targetSymbol && !fact.selectionRange.known);missingField=1;}}
 CHECK(missingField);
 xide_rename_target_info oi,ii,ci;CHECK(xideRenameTargetInfo(outer,&oi) && xideRenameTargetInfo(inner,&ii) && oi.declarationStart!=ii.declarationStart);CHECK(xideRenameTargetInfo(compute,&ci) && !strcmp(ci.declaration.path,dep));
 xide_reference_shard* shard=xideSemanticReferenceShardCreate(g.cache,g.check,g.sources,g.count,XIDE_REFERENCE_BYTES,NULL);CHECK(shard);xide_reference_key* key=xideRenameTargetRetainKey(compute);xide_reference_shard_view v;CHECK(xideSemanticReferenceShardQuery(shard,key,1,&v)==XIDE_REFERENCE_MATCH && v.count==2);xideSemanticReferenceKeyRelease(key);
 xide_reference_shard* local=xideSemanticReferenceShardCreate(f.cache,f.check,f.sources,f.count,XIDE_REFERENCE_BYTES,NULL);CHECK(local);key=xideRenameTargetRetainKey(outer);CHECK(xideSemanticReferenceShardQuery(local,key,1,&v)==XIDE_REFERENCE_MATCH && v.count==2);xideSemanticReferenceKeyRelease(key);key=xideRenameTargetRetainKey(inner);CHECK(xideSemanticReferenceShardQuery(local,key,1,&v)==XIDE_REFERENCE_MATCH && v.count==2);xideSemanticReferenceKeyRelease(key);
 const char* absent[]={"func choose","print(值)","dep.compute","string text","\"😀\"","field"};for(size_t i=0;i<sizeof(absent)/sizeof(*absent);++i){enum xide_rename_target_error e;const char* p=strstr(raw,absent[i]);CHECK(p && !at(&f,a,raw,(size_t)(p-raw),0,&e) && e==XIDE_RENAME_TARGET_ABSENT);}
 const char* ep=strstr(raw,"😀结果=");xls_text* text=xlsTextCreate(raw,strlen(raw),1024*1024,NULL);xls_position pos;CHECK(text && xlsTextPosition(text,(size_t)(ep-raw),&pos));enum xide_rename_target_error e;CHECK(!xideRenameTargetCreate(f.cache,f.check,f.sources,f.count,a,(int)pos.line,(int)pos.character+1,0,NULL,NULL,&e) && (e==XIDE_RENAME_TARGET_COORDINATE || e==XIDE_RENAME_TARGET_ABSENT));xlsTextRelease(text);
 const char* saved=f.sources[0].text;f.sources[0].text="forged";CHECK(!at(&f,a,raw,(size_t)(ep-raw),0,&e) && e==XIDE_RENAME_TARGET_CAPTURE);f.sources[0].text=saved;
 CHECK(!xideRenameTargetCreate(f.cache,f.check,f.sources,f.count,a,-1,0,0,NULL,NULL,&e) && e==XIDE_RENAME_TARGET_ARGUMENT);CHECK(!xideRenameTargetCreate(f.cache,f.check,f.sources,f.count,a,0,0,SIZE_MAX,NULL,NULL,&e) && e==XIDE_RENAME_TARGET_LIMIT);
 for(int stop=1;stop<=3;++stop){cancel c={0,stop};xide_rename_target* t=xideRenameTargetCreate(f.cache,f.check,f.sources,f.count,a,(int)pos.line,(int)pos.character,0,cancelled,&c,&e);CHECK(!t && e==XIDE_RENAME_TARGET_CANCELLED && c.calls==stop);}
 allocationCalls=0;refuseAllocation=1;CHECK(!at(&f,a,raw,(size_t)(ep-raw),0,&e) && e==XIDE_RENAME_TARGET_MEMORY && allocationCalls==1);refuseAllocation=0;CHECK(target(&f,a,raw,"😀结果=",4,"😀结果",NULL));
 xide_semantic_reference selected={.path="sentinel"};xide_model_error me={0};CHECK(!xideSemanticReferenceKeyCreateAt(f.cache,f.check,f.sources,f.count,a,0,1,XIDE_REFERENCE_BYTES,&selected,&me) && !selected.path && !me.code);
 size_t first=9,last=9;xide_semantic_source cleared={.path="sentinel"};CHECK(!xideSemanticReferenceKeyTarget(NULL,&cleared,&first,&last) && !cleared.path && !first && !last);
 release(&f);release(&g);CHECK(xideRenameTargetInfo(compute,&ci) && !strcmp(ci.declaration.path,dep) && !memcmp(ci.name.Data,"compute",7));key=xideRenameTargetRetainKey(compute);xideRenameTargetRelease(compute);CHECK(xideSemanticReferenceShardQuery(shard,key,1,&v)==XIDE_REFERENCE_MATCH && v.count==2);xideSemanticReferenceKeyRelease(key);CHECK(xideRenameTargetInfo(emoji,&ci) && ci.name.Size==strlen("😀结果"));CHECK(xideRenameTargetRetain(emoji)==emoji);xideRenameTargetRelease(emoji);xideRenameTargetRelease(emoji);xideRenameTargetRelease(outer);xideRenameTargetRelease(inner);xideSemanticReferenceShardRelease(shard);xideSemanticReferenceShardRelease(local);
 xrtFree(a);xrtFree(b);xrtFree(dep);xrtFree(directory);return 1;
}
int main(void){if(!run())return 1;printf("semantic_rename_target_test passed: %u checks; exact native binding, Unicode UTF16/BOM/CRLF, shadowing, cross-owner identity, cache, bounds, cancellation and independent lifetime\n",checks);return 0;}
