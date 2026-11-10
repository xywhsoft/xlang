#include <xrt.h>
#include <stdio.h>
#include <string.h>
static _Thread_local size_t fault=SIZE_MAX,allocations,pathFault=SIZE_MAX,pathCalls;
static void* bindingAllocate(size_t n){return allocations++==fault?NULL:xrtMalloc(n);}
#include "platform/path.h"
static wchar_t* bindingPath(const char* p){return pathCalls++==pathFault?NULL:xidePathNativeWide(p);}
#define XIDE_BINDING_ALLOC bindingAllocate
#define XIDE_BINDING_PATH bindingPath
#include "../src/services/semantic_definition.c"
#include "services/language_check.h"
#include "analysis/text.h"
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"semantic_binding_check_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
typedef struct binding_owner {xls_semantic_check* check;xls_projection_store* cache;xide_reference_shard* shard;xide_semantic_source sources[512];size_t count;} binding_owner;
static uint64_t blocks(void){xmemstats s;xrtMemStatsGet(&s);return s.BlockAllocCalls-s.BlockFreeCalls;}
static int captureBinding(binding_owner* o,const char* entry,const xls_semantic_input* inputs,size_t count)
{
 o->check=xlsSemanticCheckCreate(entry,inputs,count,1,NULL,NULL,NULL,0);CHECK(o->check);
 xls_check_info ci=xlsSemanticCheckInfo(o->check);
 if(!ci.accepted)for(size_t i=0;i<ci.diagnostics;++i){xls_check_diagnostic d;xlsSemanticCheckDiagnostic(o->check,i,&d);fprintf(stderr,"%s:%d:%d %s\n",d.path,d.line,d.column,d.message);}
 CHECK(ci.accepted);xls_check_inputs si;CHECK(xlsSemanticCheckInputs(o->check,&si) && si.sources<=512);o->count=si.sources;
 for(size_t i=0;i<o->count;++i){xls_input_source s;CHECK(xlsSemanticCheckSource(o->check,i,&s));int prefix=0;
  for(size_t j=0;j<count;++j)if(xidePathLexicalEqual(inputs[j].path,s.path))prefix=inputs[j].size>=3 && !memcmp(inputs[j].text,"\xef\xbb\xbf",3);
  o->sources[i]=(xide_semantic_source){s.path,s.text,s.kind,prefix};}
 o->cache=xlsProjectionStoreCreate(XLS_PROJECTION_STORE_BYTES);CHECK(o->cache);
 o->shard=xideSemanticReferenceShardCreate(o->cache,o->check,o->sources,o->count,XIDE_REFERENCE_BYTES,NULL);CHECK(o->shard);return 1;
}
static void detachBinding(binding_owner* o)
{xlsProjectionStoreDestroy(o->cache);o->cache=NULL;xlsSemanticCheckDestroy(o->check);o->check=NULL;}
static void clearBinding(binding_owner* o)
{detachBinding(o);xideSemanticReferenceShardRelease(o->shard);memset(o,0,sizeof(*o));}
static xide_rename_plan* bindingPlan(binding_owner* o,const char* path,const char* raw,const char* needle,const char* newName)
{
 const char* at=strstr(raw,needle);if(!at)return NULL;int line,column;
 if(!xideLspOffsetToPosition(raw,(int)(at-raw),&line,&column))return NULL;
 xide_reference_key* key=xideSemanticReferenceKeyCreate(o->cache,o->check,o->sources,o->count,path,line,column,XIDE_REFERENCE_BYTES,NULL);
 xide_reference_identifier_view view; xstrview name;int units;
 if(!key || !xideSemanticReferenceKeyIdentifier(key,&name,&units) || xideSemanticReferenceShardIdentifiers(o->shard,key,1,&view)!=XIDE_REFERENCE_MATCH){xideSemanticReferenceKeyRelease(key);return NULL;}
 xide_rename_plan_input* docs=xrtCalloc(o->count,sizeof(*docs));
 xide_rename_plan_change* changes=xrtCalloc(view.references.count,sizeof(*changes));xide_rename_plan* plan=NULL;size_t count=0;int okay=docs && changes;
 for(size_t i=0;okay && i<view.references.count;++i){xide_semantic_reference r;okay=xideSemanticReferenceShardIdentifierAt(&view,i,&r);if(!okay)break;
  size_t doc=0;while(doc<count && !xidePathLexicalEqual(docs[doc].source.path,r.path))++doc;
  if(doc==count){size_t source=0;while(source<o->count && !xidePathLexicalEqual(o->sources[source].path,r.path))++source;if(source==o->count){okay=0;break;}
   docs[count].source=o->sources[source];docs[count].document=xideLanguageReferenceDocument(o->sources+source,21,NULL);okay=docs[count++].document!=NULL;}
  changes[i]=(xide_rename_plan_change){doc,r.start,r.end};
 }
 if(okay)plan=xideRenamePlanCreate(docs,count,changes,view.references.count,name,xrtStrView(newName),0,NULL,NULL,NULL);
 for(size_t i=0;i<count;++i)xrtValueRelease((xvalue*)docs[i].document);
 xrtFree(docs);xrtFree(changes);xideSemanticReferenceKeyRelease(key);return plan;
}
static int recheckBinding(binding_owner* after,const char* entry,const xls_semantic_input* inputs,size_t count,const xide_rename_plan* plan)
{
 xls_semantic_input copy[64];CHECK(count<=64);memcpy(copy,inputs,count*sizeof(*copy));xide_rename_plan_info info;CHECK(xideRenamePlanInfo(plan,&info));
 for(size_t i=0;i<info.documents;++i){xide_rename_plan_document d;CHECK(xideRenamePlanDocumentAt(plan,i,&d));size_t j=0;while(j<count && !xidePathLexicalEqual(copy[j].path,d.path))++j;
  CHECK(j<64);if(j==count)++count;copy[j]=(xls_semantic_input){d.path,d.after.Data,d.after.Size};}
 return captureBinding(after,entry,copy,count);
}
typedef struct binding_cancel {size_t calls,stop;} binding_cancel;
static int bindingCancel(void* p){binding_cancel* c=p;return c->calls++==c->stop;}
typedef struct binding_reader {const xide_reference_shard *before,*after;const xide_rename_plan* plan;int okay;} binding_reader;
static int32 readBinding(void* p)
{binding_reader* r=p;r->okay=1;for(size_t i=0;i<20;++i)if(xideSemanticBindingsPreserved(r->before,r->after,r->plan,0,NULL,NULL,NULL)!=XIDE_BINDING_PRESERVED)r->okay=0;return 0;}
static xide_reference_shard* cloneBindingShard(const xide_reference_shard* original)
{
 size_t bytes=sizeof(*original)+original->sources*sizeof(reference_source)+original->count*sizeof(reference_item);
 xide_reference_shard* s=xrtMalloc(bytes);if(!s)return NULL;*s=*original;xrtAtomic32Init(&s->references,1);s->bytes=bytes;s->source=(reference_source*)(s+1);s->items=(reference_item*)(s->source+s->sources);
 memcpy(s->source,original->source,s->sources*sizeof(*s->source));memcpy(s->items,original->items,s->count*sizeof(*s->items));return s;
}
static int bindingPositive(const char* dir)
{
 char *entry=xrtPathJoin(dir,"入口😀.xl"),*dep=xrtPathJoin(dir,"dep.xl");CHECK(entry && dep);
 const char* raw="\xef\xbb\xbf" "import dep\r\nfunc main(...) as int\r\n int kept=3\r\n int value=kept+1\r\n return dep.compute(value)+kept\r\nend\r\n";
 const char* lib="export func compute(int x) as int\r\n return x+1\r\nend\r\n";
 xls_semantic_input inputs[]={{entry,raw,strlen(raw)},{dep,lib,strlen(lib)}};binding_owner before={0},after={0};CHECK(captureBinding(&before,entry,inputs,2));
 xide_rename_plan* plan=bindingPlan(&before,entry,raw,"value=","新值😀");CHECK(plan && recheckBinding(&after,entry,inputs,2,plan));
 xide_binding_check_info info;CHECK(xideSemanticBindingsPreserved(before.shard,after.shard,plan,0,NULL,NULL,&info)==XIDE_BINDING_PRESERVED);
 CHECK(info.sources==2 && info.beforeReferences>4 && info.beforeReferences==info.afterReferences && info.mappedReferences==info.beforeReferences && info.comparedReferences==info.beforeReferences && info.sourceBytesCompared==2*strlen(raw+3)+strlen(lib)+2*(strlen("新值😀")-strlen("value")));
 size_t minimum=info.bytes;CHECK(xideSemanticBindingsPreserved(before.shard,after.shard,plan,minimum,NULL,NULL,NULL)==XIDE_BINDING_PRESERVED);
 CHECK(xideSemanticBindingsPreserved(before.shard,after.shard,plan,minimum-1,NULL,NULL,NULL)==XIDE_BINDING_LIMIT);
 CHECK(xideSemanticBindingsPreserved(NULL,after.shard,plan,0,NULL,NULL,&info)==XIDE_BINDING_ARGUMENT && !info.sources);
 CHECK(xideSemanticBindingsPreserved(before.shard,after.shard,NULL,0,NULL,NULL,NULL)==XIDE_BINDING_ARGUMENT);
 CHECK(xideSemanticBindingsPreserved(before.shard,after.shard,plan,SIZE_MAX,NULL,NULL,NULL)==XIDE_BINDING_ARGUMENT);
 CHECK(xideSemanticBindingsPreserved(before.shard,before.shard,plan,0,NULL,NULL,NULL)==XIDE_BINDING_SOURCE_CHANGED);
 uint64_t live=blocks();allocations=0;fault=0;CHECK(xideSemanticBindingsPreserved(before.shard,after.shard,plan,0,NULL,NULL,NULL)==XIDE_BINDING_MEMORY && allocations==1 && blocks()==live);fault=SIZE_MAX;
 pathCalls=0;CHECK(xideSemanticBindingsPreserved(before.shard,after.shard,plan,0,NULL,NULL,NULL)==XIDE_BINDING_PRESERVED);size_t paths=pathCalls;CHECK(paths==5);
 for(size_t i=0;i<paths;++i){pathCalls=0;pathFault=i;CHECK(xideSemanticBindingsPreserved(before.shard,after.shard,plan,0,NULL,NULL,NULL)==XIDE_BINDING_MEMORY && pathCalls==i+1 && blocks()==live);}pathFault=SIZE_MAX;
 binding_cancel cancel={0,SIZE_MAX};CHECK(xideSemanticBindingsPreserved(before.shard,after.shard,plan,0,bindingCancel,&cancel,NULL)==XIDE_BINDING_PRESERVED);size_t polls=cancel.calls;CHECK(polls>8);
 for(size_t i=0;i<polls;++i){cancel=(binding_cancel){0,i};CHECK(xideSemanticBindingsPreserved(before.shard,after.shard,plan,0,bindingCancel,&cancel,NULL)==XIDE_BINDING_CANCELLED && cancel.calls==i+1 && blocks()==live);}
 /* Reorder independent source ordinals, then alter case/separators. Neither
  * compilation-local source IDs nor path spellings are semantic identities. */
 xide_reference_shard* swapped=cloneBindingShard(after.shard);CHECK(swapped && swapped->sources==2);reference_source saved=swapped->source[0];swapped->source[0]=swapped->source[1];swapped->source[1]=saved;
 for(size_t i=0;i<swapped->count;++i){swapped->items[i].row.source=3-swapped->items[i].row.source;swapped->items[i].row.targetSource=3-swapped->items[i].row.targetSource;}
 qsort(swapped->items,swapped->count,sizeof(*swapped->items),compareReference); /* row is the first field */
 CHECK(xideSemanticBindingsPreserved(before.shard,swapped,plan,0,NULL,NULL,NULL)==XIDE_BINDING_PRESERVED);
 char* alternate=xrtMalloc(strlen(swapped->source[0].path)+1);CHECK(alternate);strcpy(alternate,swapped->source[0].path);for(size_t i=0;alternate[i];++i){if(alternate[i]=='/')alternate[i]='\\';else if(alternate[i]>='a' && alternate[i]<='z')alternate[i]-='a'-'A';}swapped->source[0].path=alternate;
 CHECK(xideSemanticBindingsPreserved(before.shard,swapped,plan,0,NULL,NULL,NULL)==XIDE_BINDING_PRESERVED);
 swapped->source[1].path=alternate;CHECK(xideSemanticBindingsPreserved(before.shard,swapped,plan,0,NULL,NULL,NULL)==XIDE_BINDING_ARGUMENT);xrtFree(alternate);xideSemanticReferenceShardRelease(swapped);
 xide_reference_shard* forged=cloneBindingShard(after.shard);CHECK(forged);--forged->count;
 CHECK(xideSemanticBindingsPreserved(before.shard,forged,plan,0,NULL,NULL,NULL)==XIDE_BINDING_CHANGED);++forged->count;
 reference_item item=forged->items[0];forged->items[0]=forged->items[forged->count-1];
 CHECK(xideSemanticBindingsPreserved(before.shard,forged,plan,0,NULL,NULL,NULL)==XIDE_BINDING_CHANGED);forged->items[0]=item;
 int kind=forged->source[0].kind;forged->source[0].kind=2;CHECK(xideSemanticBindingsPreserved(before.shard,forged,plan,0,NULL,NULL,NULL)==XIDE_BINDING_SOURCE_CHANGED);forged->source[0].kind=kind;
 size_t n=forged->source[0].size;--forged->source[0].size;CHECK(xideSemanticBindingsPreserved(before.shard,forged,plan,0,NULL,NULL,NULL)==XIDE_BINDING_SOURCE_CHANGED);forged->source[0].size=n;
 const char* original=forged->source[1].text;char* altered=xrtMalloc(forged->source[1].size+1);CHECK(altered);memcpy(altered,original,forged->source[1].size+1);altered[forged->source[1].size-1]=' ';
 forged->source[1].text=altered;CHECK(xideSemanticBindingsPreserved(before.shard,forged,plan,0,NULL,NULL,NULL)==XIDE_BINDING_SOURCE_CHANGED);forged->source[1].text=(char*)original;xrtFree(altered);
 /* Host BOM conventions do not change the post-BOM byte identity. */
 int prefix=forged->source[0].prefix;forged->source[0].prefix=!prefix;
 CHECK(xideSemanticBindingsPreserved(before.shard,forged,plan,0,NULL,NULL,NULL)==XIDE_BINDING_PRESERVED);forged->source[0].prefix=prefix;
 --forged->sources;CHECK(xideSemanticBindingsPreserved(before.shard,forged,plan,0,NULL,NULL,NULL)==XIDE_BINDING_SOURCE_CHANGED);xideSemanticReferenceShardRelease(forged);
 xide_reference_shard* badBefore=cloneBindingShard(before.shard);CHECK(badBefore);xide_rename_plan_edit edit;CHECK(xideRenamePlanEditAt(plan,0,&edit));size_t index=0;
 while(index<badBefore->count && badBefore->items[index].row.start!=(uint32_t)edit.start.offset)++index;
 CHECK(index<badBefore->count);++badBefore->items[index].row.start;
 CHECK(xideSemanticBindingsPreserved(badBefore,after.shard,plan,0,NULL,NULL,NULL)==XIDE_BINDING_UNMAPPABLE);xideSemanticReferenceShardRelease(badBefore);
 /* Comparison results do not rely on a live native check or projection cache. */
 detachBinding(&before);detachBinding(&after);binding_reader readers[4];xthread* threads[4];
 for(size_t i=0;i<4;++i){readers[i]=(binding_reader){before.shard,after.shard,plan,0};threads[i]=xrtThreadCreate(readBinding,readers+i,0);CHECK(threads[i]);}
 for(size_t i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}
 clearBinding(&before);clearBinding(&after);xideRenamePlanRelease(plan);
 /* Qualified native tokens include dep.; the edited IDENT is their suffix.
  * Remapping must preserve the whole bound token and its separate target. */
 CHECK(captureBinding(&before,entry,inputs,2));plan=bindingPlan(&before,entry,raw,"compute(value)","calc");CHECK(plan && recheckBinding(&after,entry,inputs,2,plan));
 CHECK(xideSemanticBindingsPreserved(before.shard,after.shard,plan,0,NULL,NULL,&info)==XIDE_BINDING_PRESERVED && info.sources==2);
 clearBinding(&before);clearBinding(&after);xideRenamePlanRelease(plan);xrtFree(entry);xrtFree(dep);return 1;
}
static int bindingProfile(const char* dir,const char* raw,const char* needle,const char* name,enum xide_binding_check_result expected)
{
 char* entry=xrtPathJoin(dir,"profile.xl");CHECK(entry);xls_semantic_input input={entry,raw,strlen(raw)};binding_owner before={0},after={0};CHECK(captureBinding(&before,entry,&input,1));
 xide_rename_plan* plan=bindingPlan(&before,entry,raw,needle,name);CHECK(plan && recheckBinding(&after,entry,&input,1,plan));
 xide_binding_check_info info;CHECK(xideSemanticBindingsPreserved(before.shard,after.shard,plan,0,NULL,NULL,&info)==expected && info.sources==1 && info.beforeReferences>0);
 clearBinding(&before);clearBinding(&after);xideRenamePlanRelease(plan);xrtFree(entry);return 1;
}
static int bindingNegative(const char* dir)
{
 /* Both native semantic/typed-IR analyses accept this program. Renaming value
  * captures the untouched kept occurrence inside the nested scope. */
 CHECK(bindingProfile(dir,"func main(...) as int\n int kept=7\n if true\n int value=1\n print(kept)\n print(value)\n end\n return kept\nend\n","value=","kept",XIDE_BINDING_CHANGED));
 /* The untouched call changes overload after renaming an unrelated function. */
 CHECK(bindingProfile(dir,"func first(int x) as int\n return x\nend\nfunc second(float x) as int\n return 2\nend\nfunc main(...) as int\n return second(1)\nend\n","first(","second",XIDE_BINDING_CHANGED));
 CHECK(bindingProfile(dir,"func main(...) as int\n int longName=1\n return longName\nend\n","longName=","x",XIDE_BINDING_PRESERVED));
 return 1;
}
static int bindingWide(const char* dir)
{
 char* entry=xrtPathJoin(dir,"wide.xl");CHECK(entry);char* raw=xrtMalloc(160000);CHECK(raw);
 size_t length=0;memcpy(raw,"//",2);length=2;memset(raw+length,' ',130000);length+=130000;
 length+=(size_t)sprintf(raw+length,"\nfunc main(...) as int\n int value=1\n");
 for(size_t i=0;i<200;++i)length+=(size_t)sprintf(raw+length," print(value)\n");
 length+=(size_t)sprintf(raw+length," return value\nend\n");xls_semantic_input input={entry,raw,length};binding_owner before={0},after={0};CHECK(captureBinding(&before,entry,&input,1));
 xide_rename_plan* plan=bindingPlan(&before,entry,raw,"value=","short");CHECK(plan && recheckBinding(&after,entry,&input,1,plan));
 xide_binding_check_info info;CHECK(xideSemanticBindingsPreserved(before.shard,after.shard,plan,0,NULL,NULL,&info)==XIDE_BINDING_PRESERVED && info.mappedReferences>=202);
 binding_cancel cancel={0,SIZE_MAX};CHECK(xideSemanticBindingsPreserved(before.shard,after.shard,plan,0,bindingCancel,&cancel,NULL)==XIDE_BINDING_PRESERVED);size_t polls=cancel.calls;uint64_t live=blocks();
 for(size_t i=0;i<polls;++i){cancel=(binding_cancel){0,i};CHECK(xideSemanticBindingsPreserved(before.shard,after.shard,plan,0,bindingCancel,&cancel,NULL)==XIDE_BINDING_CANCELLED && blocks()==live);}
 /* Independent comparator capacity test, not a >64-overlay native-check claim.
  * Extend the already checked bank with unchanged source rows. */
 const size_t files=150;char (*names)[512]=xrtCalloc(files,sizeof(*names));CHECK(names);xide_reference_shard *wide[2]={0};
 for(size_t side=0;side<2;++side){const xide_reference_shard* s=side?after.shard:before.shard;
  size_t bytes=sizeof(*s)+files*sizeof(reference_source)+s->count*sizeof(reference_item);wide[side]=xrtMalloc(bytes);CHECK(wide[side]);
  *wide[side]=*s;xrtAtomic32Init(&wide[side]->references,1);wide[side]->bytes=bytes;wide[side]->sources=files;wide[side]->source=(reference_source*)(wide[side]+1);wide[side]->items=(reference_item*)(wide[side]->source+files);
  memcpy(wide[side]->source,s->source,s->sources*sizeof(reference_source));memcpy(wide[side]->items,s->items,s->count*sizeof(reference_item));
  for(size_t i=s->sources;i<files;++i){snprintf(names[i],512,"%s/unchanged-%zu.xl",dir,i);wide[side]->source[i]=(reference_source){names[i],"//unchanged\r\n",13,1,0};}
 }
 CHECK(xideSemanticBindingsPreserved(wide[0],wide[1],plan,0,NULL,NULL,&info)==XIDE_BINDING_PRESERVED && info.sources==150 && info.comparedReferences==before.shard->count);
 /* A plan for a different owner is legitimately absent from this bank. */
 xide_reference_shard one={0};reference_source source={names[149],"//unchanged\r\n",13,1,0};one.sources=1;one.source=&source;
 CHECK(xideSemanticBindingsPreserved(&one,&one,plan,0,NULL,NULL,&info)==XIDE_BINDING_PRESERVED && info.sources==1 && !info.beforeReferences);
 xideSemanticReferenceShardRelease(wide[0]);xideSemanticReferenceShardRelease(wide[1]);xrtFree(names);
 clearBinding(&before);clearBinding(&after);xideRenamePlanRelease(plan);xrtFree(entry);xrtFree(raw);return 1;
}
int main(void)
{
 char* dir=xrtPathAbs("build/binding-check");if(!dir || !xrtDirCreateAll(dir) || !bindingPositive(dir) || !bindingNegative(dir) || !bindingWide(dir))return 1;xrtFree(dir);
 printf("semantic_binding_check_test passed: %u checks; real post-edit native checks, complete mapped binding rows, unchanged-symbol capture and overload changes, reordered sources, Unicode/BOM/CRLF, quotas, faults, cancellation and detached readers\n",checks);return 0;
}
