#include <xrt.h>
#include <stdio.h>
#include <string.h>
typedef struct graph_allocation {void* pointer;size_t bytes;int live;} graph_allocation;
static _Thread_local graph_allocation observed[16];
static _Thread_local size_t observedCount,operations,failOperation,liveBytes,peakBytes;
static _Thread_local int tracking;
void* __real_xrtMalloc(size_t);
void __real_xrtFree(void*);
void* __wrap_xrtMalloc(size_t bytes)
{
 if(tracking && ++operations==failOperation)return NULL;
 void* p=__real_xrtMalloc(bytes);
 if(tracking && p){if(observedCount==16)return p;observed[observedCount++]=(graph_allocation){p,bytes,1};liveBytes+=bytes;if(liveBytes>peakBytes)peakBytes=liveBytes;}
 return p;
}
void __wrap_xrtFree(void* p)
{if(tracking && p)for(size_t i=0;i<observedCount;++i)if(observed[i].live && observed[i].pointer==p){observed[i].live=0;liveBytes-=observed[i].bytes;break;}__real_xrtFree(p);}
static void observe(size_t fault){memset(observed,0,sizeof(observed));observedCount=operations=liveBytes=peakBytes=0;failOperation=fault;tracking=1;}
#define main dispatchOriginalMain
#include "semantic_dispatch_family_test.c"
#undef main
#undef CHECK
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"semantic_reference_graph_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static int equalFamily(xide_reference_family* a,xide_reference_family* b)
{
 xide_reference_family_info x,y;CHECK(xideSemanticReferenceFamilyInfo(a,&x) && xideSemanticReferenceFamilyInfo(b,&y));CHECK(x.members==y.members && x.sources==y.sources && x.relations==y.relations && x.kinds==y.kinds);
 for(size_t i=0;i<x.members;++i){xide_reference_key *ka=xideSemanticReferenceFamilyKey(a,i),*kb=xideSemanticReferenceFamilyKey(b,i);xide_semantic_source sa,sb;size_t af,al,bf,bl;
  CHECK(ka && kb && xideSemanticReferenceKeyTarget(ka,&sa,&af,&al) && xideSemanticReferenceKeyTarget(kb,&sb,&bf,&bl));CHECK(af==bf && al==bl && sa.kind==sb.kind && sa.utf16Prefix==sb.utf16Prefix && !strcmp(sa.path,sb.path) && !strcmp(sa.text,sb.text));xideSemanticReferenceKeyRelease(ka);xideSemanticReferenceKeyRelease(kb);
 }return 1;
}
static xide_reference_graph* graphCapture(family_owner* o,size_t budget,int (*cancelled)(void*),void* user,enum xide_reference_family_error* why)
{return xideSemanticReferenceGraphCreate(o->cache,o->check,o->sources,o->count,budget,cancelled,user,why);}
static int graphAllKeys(family_owner* o,xide_reference_graph* graph)
{
 xls_projection* lease=NULL;xide_semantic_definitions* nav=xideSemanticDefinitionsAcquireTyped(o->cache,o->check,o->sources,o->count,&lease,NULL,NULL);CHECK(nav);size_t tested=0;
 for(size_t i=0;i<nav->declCount+nav->typeCount;++i){const declaration* d=i<nav->declCount ? nav->declarations+i : nav->typeDeclarations+i-nav->declCount;if(d->node==SIZE_MAX)continue;const span* s=&nav->nodes[d->node].selection;if(!s->source || s->endOffset<=s->startOffset)continue;
  const xide_semantic_source* source=o->sources+s->source-1;xide_reference_key* key=xideSemanticReferenceKeyCreate(o->cache,o->check,o->sources,o->count,source->path,s->start.line,s->start.character,XIDE_REFERENCE_BYTES,NULL);if(!key)continue;
  enum xide_reference_family_error aWhy,bWhy;xide_reference_family* a=makeFamily(o,key,0,NULL,NULL,&aWhy),*b=xideSemanticReferenceGraphFamily(graph,key,0,NULL,NULL,&bWhy);CHECK(aWhy==bWhy && (!!a)==(!!b));if(a)CHECK(equalFamily(a,b));xideSemanticReferenceFamilyRelease(a);xideSemanticReferenceFamilyRelease(b);xideSemanticReferenceKeyRelease(key);++tested;
 }
 CHECK(tested>3);xideSemanticDefinitionsReleaseTyped(nav,lease);return 1;
}
static int graphControls(family_owner* o,xide_reference_graph* graph,xide_reference_key* seed)
{
 enum xide_reference_family_error why;observe(SIZE_MAX);xide_reference_graph* g=graphCapture(o,0,NULL,NULL,&why);tracking=0;
 xide_reference_graph_info info;CHECK(g && xideSemanticReferenceGraphInfo(g,&info) && operations==5 && observedCount==5 && liveBytes==info.bytes);size_t peak=peakBytes;
 xideSemanticReferenceGraphRelease(g);CHECK(peak>info.bytes && peak<XIDE_REFERENCE_BYTES);CHECK(!graphCapture(o,1,NULL,NULL,&why) && why==XIDE_REFERENCE_FAMILY_LIMIT);
 size_t low=info.bytes,high=peak;while(low<high){size_t mid=low+(high-low)/2;g=graphCapture(o,mid,NULL,NULL,&why);if(g){high=mid;xideSemanticReferenceGraphRelease(g);}else{CHECK(why==XIDE_REFERENCE_FAMILY_LIMIT);low=mid+1;}}
 CHECK(low==peak);g=graphCapture(o,peak,NULL,NULL,&why);CHECK(g);xideSemanticReferenceGraphRelease(g);CHECK(!graphCapture(o,peak-1,NULL,NULL,&why) && why==XIDE_REFERENCE_FAMILY_LIMIT);
 for(size_t i=1;i<=5;++i){uint64_t before=blocks();observe(i);g=graphCapture(o,0,NULL,NULL,&why);tracking=0;CHECK(!g && why==XIDE_REFERENCE_FAMILY_MEMORY && operations==i && !liveBytes && blocks()==before);}
 cancellation cancel={0,SIZE_MAX};g=graphCapture(o,0,cancelFamily,&cancel,&why);CHECK(g && cancel.polls>12);xideSemanticReferenceGraphRelease(g);size_t polls=cancel.polls;
 for(size_t i=0;i<polls;++i){uint64_t before=blocks();cancel=(cancellation){0,i};g=graphCapture(o,0,cancelFamily,&cancel,&why);CHECK(!g && why==XIDE_REFERENCE_FAMILY_CANCELLED && cancel.polls==i+1 && blocks()==before);}
 observe(SIZE_MAX);xide_reference_family* f=xideSemanticReferenceGraphFamily(graph,seed,0,NULL,NULL,&why);tracking=0;CHECK(f && operations==1 && observedCount==1 && liveBytes==observed[0].bytes);size_t own=observed[0].bytes;xide_reference_family_info fi;CHECK(xideSemanticReferenceFamilyInfo(f,&fi) && xideSemanticReferenceGraphInfo(graph,&info) && fi.bytes==own+info.bytes);xideSemanticReferenceFamilyRelease(f);
 f=xideSemanticReferenceGraphFamily(graph,seed,own,NULL,NULL,&why);CHECK(f);xideSemanticReferenceFamilyRelease(f);CHECK(!xideSemanticReferenceGraphFamily(graph,seed,own-1,NULL,NULL,&why) && why==XIDE_REFERENCE_FAMILY_LIMIT);
 uint64_t before=blocks();observe(1);f=xideSemanticReferenceGraphFamily(graph,seed,0,NULL,NULL,&why);tracking=0;CHECK(!f && why==XIDE_REFERENCE_FAMILY_MEMORY && !liveBytes && blocks()==before);
 cancel=(cancellation){0,SIZE_MAX};f=xideSemanticReferenceGraphFamily(graph,seed,0,cancelFamily,&cancel,&why);CHECK(f && cancel.polls);xideSemanticReferenceFamilyRelease(f);polls=cancel.polls;
 for(size_t i=0;i<polls;++i){before=blocks();cancel=(cancellation){0,i};f=xideSemanticReferenceGraphFamily(graph,seed,0,cancelFamily,&cancel,&why);CHECK(!f && why==XIDE_REFERENCE_FAMILY_CANCELLED && blocks()==before);}
 return 1;
}
static int graphFamilies(const char* path,const char* text,const char* name)
{
 family_owner o={0};xls_semantic_input input={path,text,strlen(text)};CHECK(captureFamily(&o,path,&input,1));enum xide_reference_family_error why;
 xide_reference_graph* graph=graphCapture(&o,0,NULL,NULL,&why);xide_reference_graph_info info;CHECK(graph && why==XIDE_REFERENCE_FAMILY_OK && xideSemanticReferenceGraphInfo(graph,&info) && info.declarations && info.components && info.relations>=11 && info.bytes>info.ownedBytes);
 xide_reference_shard* bank=xideSemanticReferenceGraphRetainShard(graph);CHECK(bank && info.bytes==info.ownedBytes+xideSemanticReferenceShardBytes(bank));
 size_t streamed=0,relations=0,unmapped=0;uint32_t sourceId=0,start=0,end=0;observe(SIZE_MAX);
 for(size_t i=0;i<info.declarations;++i){xide_reference_graph_declaration d;xide_reference_graph_component c;CHECK(xideSemanticReferenceGraphDeclarationAt(graph,i,&d) && d.source && d.component<info.components && xideSemanticReferenceGraphComponentAt(graph,d.component,&c) && c.members);CHECK(d.source>sourceId || (d.source==sourceId && (d.start>start || (d.start==start && d.end>end))));sourceId=d.source;start=d.start;end=d.end;}
 for(size_t i=0;i<info.components;++i){xide_reference_graph_component c;CHECK(xideSemanticReferenceGraphComponentAt(graph,i,&c) && (c.members || c.unsupported));streamed+=c.members;relations+=c.relations;if(!c.members && c.unsupported)++unmapped;}
 tracking=0;CHECK(!operations && streamed==info.declarations && relations==info.relations && unmapped);xide_reference_graph_declaration absent={.source=9};xide_reference_graph_component absentComponent={.members=9};CHECK(!xideSemanticReferenceGraphDeclarationAt(graph,info.declarations,&absent) && !absent.source && !xideSemanticReferenceGraphComponentAt(graph,info.components,&absentComponent) && !absentComponent.members);
 xide_reference_key* seed=familyKeyAt(&o,path,text,name);CHECK(seed);xide_reference_family* family=xideSemanticReferenceGraphFamily(graph,seed,0,NULL,NULL,&why);CHECK(family && familyKeys(&o,family,name,5,1));CHECK(graphControls(&o,graph,seed));
 /* Compare every actual current declaration/type key with the original
    independent constructor, not merely the requested dispatch component. */
 CHECK(graphAllKeys(&o,graph));
 /* A leased key addresses the exact retained bank bytes, with no copy. */
 xide_reference_key* held=xideSemanticReferenceFamilyKey(family,0);xide_semantic_source target,bankSource;size_t first,last;CHECK(held && xideSemanticReferenceKeyTarget(held,&target,&first,&last) && xideSemanticReferenceShardSource(bank,0,&bankSource) && target.text==bankSource.text);
 xide_reference_family* same=xideSemanticReferenceGraphFamily(graph,held,0,NULL,NULL,&why);CHECK(same && equalFamily(family,same));xideSemanticReferenceFamilyRelease(same);
 char* alias=xrtStrDup(seed->path);CHECK(alias);for(char* c=alias;*c;++c)if(*c=='\\')*c='/';char* old=seed->path;seed->path=alias;same=xideSemanticReferenceGraphFamily(graph,seed,0,NULL,NULL,&why);seed->path=old;xrtFree(alias);CHECK(same && equalFamily(family,same));xideSemanticReferenceFamilyRelease(same);
 family_reader readers[4];xthread* threads[4];for(size_t i=0;i<4;++i){readers[i]=(family_reader){xideSemanticReferenceFamilyRetain(family),name,5,0};threads[i]=xrtThreadCreate(readFamily,readers+i,0);CHECK(threads[i]);}
 xideSemanticReferenceFamilyRelease(family);xideSemanticReferenceKeyRelease(seed);clearFamily(&o);xideSemanticReferenceGraphRelease(graph);xideSemanticReferenceShardRelease(bank);
 for(size_t i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}
 xstrview n;int units;CHECK(xideSemanticReferenceKeyIdentifier(held,&n,&units) && n.Size==strlen(name) && !memcmp(n.Data,name,n.Size));xideSemanticReferenceKeyRelease(held);return 1;
}
static int graphProfiles(const char* directory,const char* path)
{
 const char* raw="func choose(int value) as int\n return value\nend\nfunc choose(int first,int second) as int\n return first+second\nend\nclass Box\n int data\n get item() as int\n return self.data\n end\n let item(int value)\n self.data=value\n end\nend\nfunc main(...) as int\n auto box=Box()\n box.item=3\n return box.item+choose(1)+choose(2,3)\nend\n";
 family_owner o={0};xls_semantic_input input={path,raw,strlen(raw)};CHECK(captureFamily(&o,path,&input,1));enum xide_reference_family_error why;xide_reference_graph* g=graphCapture(&o,0,NULL,NULL,&why);CHECK(g && graphAllKeys(&o,g));
 const char* saved=o.sources[0].text;o.sources[0].text="different";CHECK(!graphCapture(&o,0,NULL,NULL,&why) && why==XIDE_REFERENCE_FAMILY_INVALID);o.sources[0].text=saved;
 char* changed=xrtStrDup(raw);CHECK(changed);char* value=strstr(changed,"box.item=3");CHECK(value);value[strlen("box.item=")]='4';
 family_owner second={0};input=(xls_semantic_input){path,changed,strlen(changed)};CHECK(captureFamily(&second,path,&input,1));xide_reference_key* key=familyKeyAt(&second,path,changed,"choose");CHECK(key && !xideSemanticReferenceGraphFamily(g,key,0,NULL,NULL,&why) && why==XIDE_REFERENCE_FAMILY_INVALID);xideSemanticReferenceKeyRelease(key);clearFamily(&second);xrtFree(changed);xideSemanticReferenceGraphRelease(g);clearFamily(&o);
 char* dep=xrtPathJoin(directory,"dep.xl");CHECK(dep);const char* lib="export class Box<T>\n T value\nend\nexport func choose(int value) as int\n return value\nend\nexport func choose(int first,int second) as int\n return first+second\nend\n";
 const char* imported="import dep\nfunc identity<T>(T value) as T\n return value\nend\nfunc main(...) as int\n auto box=dep.Box::<int>()\n return identity::<int>(box.value)+dep.choose(1)+dep.choose(2,3)\nend\n";xls_semantic_input inputs[]={{path,imported,strlen(imported)},{dep,lib,strlen(lib)}};CHECK(captureFamily(&o,path,inputs,2));g=graphCapture(&o,0,NULL,NULL,&why);CHECK(g && graphAllKeys(&o,g));xideSemanticReferenceGraphRelease(g);clearFamily(&o);
 CHECK(xrtFileWriteAtomic(dep,(xbytesview){(const unsigned char*)lib,strlen(lib)}));input=inputs[0];CHECK(captureFamily(&o,path,&input,1));g=graphCapture(&o,0,NULL,NULL,&why);key=familyKeyAt(&o,path,imported,"choose(1)");CHECK(g && key);CHECK(xrtFileWriteAtomic(dep,(xbytesview){(const unsigned char*)"func broken(\n",strlen("func broken(\n")}));CHECK(!xlsSemanticCheckValidate(o.check) && !graphCapture(&o,0,NULL,NULL,&why) && why==XIDE_REFERENCE_FAMILY_INVALID);
 xide_reference_family* old=xideSemanticReferenceGraphFamily(g,key,0,NULL,NULL,&why);CHECK(old);xideSemanticReferenceFamilyRelease(old);xideSemanticReferenceKeyRelease(key);xideSemanticReferenceGraphRelease(g);clearFamily(&o);
 xrtFree(dep);return 1;
}
static int runGraphs(void)
{
 char* directory=xrtPathAbs("build/reference-graphs");CHECK(directory && xrtDirCreateAll(directory));char* path=xrtPathJoin(directory,"入口😀.xl");CHECK(path && graphFamilies(path,dispatchFamilyText,"read"));
 xstrbuf unicode={0};CHECK(xrtStrBufAppend(&unicode,xrtStrView("\xef\xbb\xbf")));for(const char* at=dispatchFamilyText;*at;){if(!strncmp(at,"read",4)){CHECK(xrtStrBufAppend(&unicode,xrtStrView("读取😀")));at+=4;}else if(*at=='\n'){CHECK(xrtStrBufAppend(&unicode,xrtStrView("\r\n")));++at;}else{CHECK(xrtStrBufAppend(&unicode,(xstrview){at,1}));++at;}}
 CHECK(graphFamilies(path,unicode.Data,"读取😀"));xrtStrBufFree(&unicode);CHECK(graphProfiles(directory,path));xrtFree(path);xrtFree(directory);return 1;
}
int main(void){if(!runGraphs())return 1;printf("semantic_reference_graph_test passed: %u checks; all-declaration legacy parity, shared source bank, detached components, exact observed peak quotas, five real construction refusals, lease refusal, cancellation, Unicode and four-reader lifetime\n",checks);return 0;}
