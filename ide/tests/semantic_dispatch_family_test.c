#include <xrt.h>
#include <stdio.h>
#include <string.h>
static _Thread_local size_t familyFaultAt=SIZE_MAX,familyAllocations;
static void* allocateFamily(size_t bytes){return familyAllocations++==familyFaultAt?NULL:xrtMalloc(bytes);}
#define XIDE_FAMILY_ALLOC allocateFamily
#include "../src/services/semantic_definition.c"
#include "analysis/frontend_internal.h"
#include "services/semantic_rename.h"
#include "dispatch_family_fixture.h"
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"semantic_dispatch_family_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
typedef struct family_owner {xls_semantic_check* check;xide_reference_shard* shard;xide_semantic_source sources[8];size_t count;xls_projection_store* cache;} family_owner;
static uint64_t blocks(void){xmemstats s;xrtMemStatsGet(&s);return s.BlockAllocCalls-s.BlockFreeCalls;}
static int captureFamily(family_owner* o,const char* path,const xls_semantic_input* inputs,size_t count)
{
 o->check=xlsSemanticCheckCreate(path,inputs,count,1,NULL,NULL,NULL,0);CHECK(o->check);xls_check_info info=xlsSemanticCheckInfo(o->check);
 if(!info.accepted)for(size_t i=0;i<info.diagnostics;++i){xls_check_diagnostic d;xlsSemanticCheckDiagnostic(o->check,i,&d);fprintf(stderr,"%s:%d %s\n",d.path,d.line,d.message);}
 CHECK(info.accepted);xls_check_inputs rows;CHECK(xlsSemanticCheckInputs(o->check,&rows) && rows.sources<=8);o->count=rows.sources;
 for(size_t i=0;i<o->count;++i){xls_input_source s;CHECK(xlsSemanticCheckSource(o->check,i,&s));int prefix=0;for(size_t j=0;j<count;++j)if(!strcmp(s.path,inputs[j].path))prefix=inputs[j].size>=3 && !memcmp(inputs[j].text,"\xef\xbb\xbf",3);o->sources[i]=(xide_semantic_source){s.path,s.text,s.kind,prefix};}
 o->cache=xlsProjectionStoreCreate(XLS_PROJECTION_STORE_BYTES);CHECK(o->cache);o->shard=xideSemanticReferenceShardCreate(o->cache,o->check,o->sources,o->count,XIDE_REFERENCE_BYTES,NULL);CHECK(o->shard);return 1;
}
static void clearFamily(family_owner* o){xideSemanticReferenceShardRelease(o->shard);xlsProjectionStoreDestroy(o->cache);xlsSemanticCheckDestroy(o->check);memset(o,0,sizeof(*o));}
static xide_reference_key* familyKeyAt(family_owner* o,const char* path,const char* raw,const char* needle)
{const char* at=strstr(raw,needle);if(!at)return NULL;xls_text* text=xlsTextCreate(raw,strlen(raw),1024*1024,NULL);xls_position p;if(!text || !xlsTextPosition(text,(size_t)(at-raw),&p)){xlsTextRelease(text);return NULL;}xlsTextRelease(text);return xideSemanticReferenceKeyCreate(o->cache,o->check,o->sources,o->count,path,(int)p.line,(int)p.character,XIDE_REFERENCE_BYTES,NULL);}
static xide_reference_family* makeFamily(family_owner* o,const xide_reference_key* key,size_t bytes,int (*stop)(void*),void* user,enum xide_reference_family_error* why)
{return xideSemanticReferenceFamilyCreate(o->cache,o->check,o->sources,o->count,key,bytes,stop,user,why);}
static int bridge(family_owner* o)
{
 const XL6_IrModule* ir=xlsSemanticCheckIr(o->check);xls_dispatch_info info;CHECK(ir && xlsSemanticCheckDispatchInfo(o->check,&info) && info.classes==ir->classDispatchImplCount && info.protocols==ir->protocolDispatchImplCount);
 for(size_t i=0;i<info.classes+info.protocols;++i){xls_dispatch_relation row;CHECK(xlsSemanticCheckDispatchRelation(o->check,i,&row));
  if(i<info.classes){const XL6_IrClassDispatchImpl* r=ir->classDispatchImpls+i;const XL6_IrClassDispatchSlot* s=ir->classDispatchSlots+r->slot;CHECK(row.kind==XLS_DISPATCH_CLASS && row.slot==r->slot && row.declaration==s->declarationSymbol && row.implementation==r->implementationSymbol && row.ownerType==r->ownerType && row.contractType==s->ownerType);}
  else{const XL6_IrProtocolDispatchImpl* r=ir->protocolDispatchImpls+i-info.classes;const XL6_IrProtocolDispatchSlot* s=ir->protocolDispatchSlots+r->slot;CHECK(row.kind==XLS_DISPATCH_PROTOCOL && row.slot==r->slot && row.declaration==s->declarationSymbol && row.implementation==r->implementationSymbol && row.ownerType==r->ownerType && row.contractType==s->protocolType);}
 }
 xls_dispatch_relation empty={.kind=9};CHECK(!xlsSemanticCheckDispatchRelation(o->check,info.classes+info.protocols,&empty) && !empty.kind && !empty.declaration);info=(xls_dispatch_info){9,9};CHECK(!xlsSemanticCheckDispatchInfo(NULL,&info) && !info.classes && !info.protocols);return 1;
}
static int familyKeys(family_owner* o,xide_reference_family* f,const char* name,size_t expected,size_t sourceCount)
{
 xide_reference_family_info info;CHECK(xideSemanticReferenceFamilyInfo(f,&info) && info.members==expected && info.sources==sourceCount && info.bytes);
 const char* paths[8]={0};const char* texts[8]={0};size_t unique=0,uses=0;
 for(size_t i=0;i<info.members;++i){xide_reference_key* k=xideSemanticReferenceFamilyKey(f,i);xide_semantic_source s;size_t first,last;CHECK(k && xideSemanticReferenceKeyTarget(k,&s,&first,&last) && last-first==strlen(name) && !memcmp(s.text+first,name,strlen(name)) && xideSemanticReferenceKeyBytes(k)==sizeof(xide_reference_key)+info.bytes);
  size_t at=0;while(at<unique && strcmp(paths[at],s.path))++at;if(at==unique){CHECK(unique<8);paths[unique]=s.path;texts[unique++]=s.text;}else CHECK(texts[at]==s.text);
  xide_reference_identifier_view refs;CHECK(xideSemanticReferenceShardIdentifiers(o->shard,k,1,&refs)==XIDE_REFERENCE_MATCH && refs.references.count);
  for(size_t j=0;j<refs.references.count;++j){xide_semantic_reference ref;CHECK(xideSemanticReferenceShardIdentifierAt(&refs,j,&ref) && ref.end.offset-ref.start.offset==(int)strlen(name) && ref.end.character-ref.start.character==k->identifierUnits);++uses;}
  xideSemanticReferenceKeyRelease(k);
 }
 CHECK(unique==sourceCount && uses>=expected && !xideSemanticReferenceFamilyKey(f,info.members));return 1;
}
typedef struct cancellation {size_t polls,at;} cancellation;
static int cancelFamily(void* user){cancellation* c=user;return c->polls++==c->at;}
typedef struct family_reader {xide_reference_family* family;const char* name;size_t members;int okay;} family_reader;
static int32 readFamily(void* user)
{family_reader* r=user;r->okay=1;for(size_t n=0;n<80;++n){xide_reference_key* key=xideSemanticReferenceFamilyKey(r->family,n%r->members);xstrview name;int units;if(!key || !xideSemanticReferenceKeyIdentifier(key,&name,&units) || name.Size!=strlen(r->name) || memcmp(name.Data,r->name,name.Size))r->okay=0;xideSemanticReferenceKeyRelease(key);}xideSemanticReferenceFamilyRelease(r->family);return 0;}
static int controls(family_owner* o,xide_reference_key* key,xide_reference_family* held)
{
 enum xide_reference_family_error why;xide_reference_family_info info;CHECK(xideSemanticReferenceFamilyInfo(held,&info));
 CHECK(!makeFamily(o,key,1,NULL,NULL,&why) && why==XIDE_REFERENCE_FAMILY_LIMIT);
 CHECK(!makeFamily(o,key,info.bytes,NULL,NULL,&why) && why==XIDE_REFERENCE_FAMILY_LIMIT);
 size_t lo=info.bytes,hi=info.bytes+8192;while(lo<hi){size_t mid=lo+(hi-lo)/2;xide_reference_family* f=makeFamily(o,key,mid,NULL,NULL,&why);if(f){hi=mid;xideSemanticReferenceFamilyRelease(f);}else{CHECK(why==XIDE_REFERENCE_FAMILY_LIMIT);lo=mid+1;}}
 xide_reference_family* exact=makeFamily(o,key,lo,NULL,NULL,&why);CHECK(exact && why==XIDE_REFERENCE_FAMILY_OK);xideSemanticReferenceFamilyRelease(exact);CHECK(!makeFamily(o,key,lo-1,NULL,NULL,&why) && why==XIDE_REFERENCE_FAMILY_LIMIT);
 for(size_t i=0;i<2;++i){uint64_t before=blocks();familyFaultAt=i;familyAllocations=0;xide_reference_family* f=makeFamily(o,key,0,NULL,NULL,&why);familyFaultAt=SIZE_MAX;CHECK(!f && why==XIDE_REFERENCE_FAMILY_MEMORY && familyAllocations==i+1 && blocks()==before);}
 familyFaultAt=0;familyAllocations=0;CHECK(!xideSemanticReferenceFamilyKey(held,0));familyFaultAt=SIZE_MAX;CHECK(xideSemanticReferenceFamilyInfo(held,&info));
 cancellation c={0,SIZE_MAX};xide_reference_family* f=makeFamily(o,key,0,cancelFamily,&c,&why);CHECK(f && c.polls>8);xideSemanticReferenceFamilyRelease(f);size_t polls=c.polls;
 for(size_t i=0;i<polls;++i){uint64_t before=blocks();c=(cancellation){0,i};f=makeFamily(o,key,0,cancelFamily,&c,&why);CHECK(!f && why==XIDE_REFERENCE_FAMILY_CANCELLED && c.polls==i+1 && blocks()==before);}
 return 1;
}
static int simpleFamily(const char* path,const char* raw,const char* name)
{
 family_owner o={0};xls_semantic_input input={path,raw,strlen(raw)};CHECK(captureFamily(&o,path,&input,1) && bridge(&o));xide_reference_key* key=familyKeyAt(&o,path,raw,name);CHECK(key);enum xide_reference_family_error why;xide_reference_family* family=makeFamily(&o,key,0,NULL,NULL,&why);CHECK(family && why==XIDE_REFERENCE_FAMILY_OK && familyKeys(&o,family,name,5,1));xide_reference_family_info info;CHECK(xideSemanticReferenceFamilyInfo(family,&info) && info.relations==11 && info.kinds==3);CHECK(controls(&o,key,family));
 /* Wrong has the same name, but its actual signature has no slot/witness. */
 const char* wrong=strstr(raw,"class Wrong");CHECK(wrong);const char* method=strstr(wrong,name);CHECK(method);xide_reference_key* unrelated=familyKeyAt(&o,path,raw,method);CHECK(unrelated);xide_reference_family* single=makeFamily(&o,unrelated,0,NULL,NULL,&why);CHECK(single && familyKeys(&o,single,name,1,1));CHECK(xideSemanticReferenceFamilyInfo(single,&info) && !info.relations && !info.kinds);xideSemanticReferenceFamilyRelease(single);xideSemanticReferenceKeyRelease(unrelated);
 xide_reference_key* lease=xideSemanticReferenceFamilyKey(family,0);CHECK(lease);family_reader readers[4];xthread* threads[4];
 for(size_t i=0;i<4;++i){readers[i]=(family_reader){xideSemanticReferenceFamilyRetain(family),name,5,0};threads[i]=xrtThreadCreate(readFamily,readers+i,0);CHECK(threads[i]);}
 xideSemanticReferenceFamilyRelease(family);xideSemanticReferenceKeyRelease(key);clearFamily(&o);
 for(size_t i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}
 xstrview value;int units;CHECK(xideSemanticReferenceKeyIdentifier(lease,&value,&units) && value.Size==strlen(name) && !memcmp(value.Data,name,value.Size));xideSemanticReferenceKeyRelease(lease);return 1;
}
static int importedFamily(const char* path,const char* dependency)
{
 const char* lib="\xef\xbb\xbf" "export protocol Reader\r\n func read() as int\r\nend\r\nexport class Base\r\n virtual func read() as int\r\n return 1\r\n end\r\nend\r\n";
 const char* raw="import dep\nclass Derived extends dep.Base\n override func read() as int\n return 2\n end\nend\nfunc through(dep.Reader value) as int\n return value.read()\nend\nfunc main(...) as int\n return through(Derived())\nend\n";
 xls_semantic_input inputs[]={{path,raw,strlen(raw)},{dependency,lib,strlen(lib)}};family_owner first={0},second={0};CHECK(captureFamily(&first,path,inputs,2) && bridge(&first));
 xide_reference_key* seed=familyKeyAt(&first,path,raw,"read() as int");CHECK(seed);enum xide_reference_family_error why;xide_reference_family* f=makeFamily(&first,seed,0,NULL,NULL,&why);CHECK(f && familyKeys(&first,f,"read",3,2));
 /* A second compilation has independent IR/AST and local numeric IDs. */
 CHECK(captureFamily(&second,path,inputs,2) && bridge(&second));CHECK(familyKeys(&second,f,"read",3,2));
 xide_reference_family* again=makeFamily(&second,seed,0,NULL,NULL,&why);CHECK(again && familyKeys(&second,again,"read",3,2));xideSemanticReferenceFamilyRelease(again);
 xide_semantic_source old=second.sources[0];second.sources[0].text="changed";CHECK(!makeFamily(&second,seed,0,NULL,NULL,&why) && why==XIDE_REFERENCE_FAMILY_INVALID);second.sources[0]=old;
 xide_reference_key* surviving=xideSemanticReferenceFamilyKey(f,0);CHECK(surviving);xideSemanticReferenceFamilyRelease(f);xideSemanticReferenceKeyRelease(seed);clearFamily(&first);
 xide_reference_identifier_view view;CHECK(xideSemanticReferenceShardIdentifiers(second.shard,surviving,1,&view)==XIDE_REFERENCE_MATCH && view.references.count);clearFamily(&second);xideSemanticReferenceKeyRelease(surviving);return 1;
}
static int withoutDispatch(const char* path)
{
 const char* raw="func simple(int value) as int\n return value\nend\nfunc main(...) as int\n return simple(3)\nend\n";xls_semantic_input input={path,raw,strlen(raw)};family_owner o={0};CHECK(captureFamily(&o,path,&input,1) && bridge(&o));xls_dispatch_info rows;CHECK(xlsSemanticCheckDispatchInfo(o.check,&rows) && !rows.classes && !rows.protocols);
 xide_reference_key* key=familyKeyAt(&o,path,raw,"simple");CHECK(key);enum xide_reference_family_error why;xide_reference_family* f=makeFamily(&o,key,0,NULL,NULL,&why);CHECK(f && familyKeys(&o,f,"simple",1,1));xide_reference_family_info info;CHECK(xideSemanticReferenceFamilyInfo(f,&info) && !info.relations && !info.kinds);
 xide_reference_family* exact=makeFamily(&o,key,info.bytes,NULL,NULL,&why);CHECK(exact);xideSemanticReferenceFamilyRelease(exact);CHECK(!makeFamily(&o,key,info.bytes-1,NULL,NULL,&why) && why==XIDE_REFERENCE_FAMILY_LIMIT);
 xideSemanticReferenceFamilyRelease(f);xideSemanticReferenceKeyRelease(key);clearFamily(&o);
 const char* invalid="func main(...) as int\n return missing\nend\n";input=(xls_semantic_input){path,invalid,strlen(invalid)};xls_semantic_check* check=xlsSemanticCheckCreate(path,&input,1,1,NULL,NULL,NULL,0);CHECK(check && !xlsSemanticCheckInfo(check).accepted);rows=(xls_dispatch_info){9,9};CHECK(!xlsSemanticCheckDispatchInfo(check,&rows) && !rows.classes && !rows.protocols);xlsSemanticCheckDestroy(check);return 1;
}
static int dispatchFamilies(void)
{
 char* directory=xrtPathAbs("build/dispatch-families");CHECK(directory && xrtDirCreateAll(directory));char* path=xrtPathJoin(directory,"入口😀.xl");CHECK(path && simpleFamily(path,dispatchFamilyText,"read"));
 xstrbuf unicode={0};CHECK(xrtStrBufAppend(&unicode,xrtStrView("\xef\xbb\xbf")));for(const char* at=dispatchFamilyText;*at;){if(!strncmp(at,"read",4)){CHECK(xrtStrBufAppend(&unicode,xrtStrView("读取😀")));at+=4;}else if(*at=='\n'){CHECK(xrtStrBufAppend(&unicode,xrtStrView("\r\n")));++at;}else{CHECK(xrtStrBufAppend(&unicode,(xstrview){at,1}));++at;}}
 CHECK(simpleFamily(path,unicode.Data,"读取😀"));xrtStrBufFree(&unicode);char* dep=xrtPathJoin(directory,"dep.xl");CHECK(dep && importedFamily(path,dep) && withoutDispatch(path));xrtFree(dep);xrtFree(path);xrtFree(directory);return 1;
}
int main(void){if(!dispatchFamilies())return 1;printf("semantic_dispatch_family_test passed: %u checks; exact native class/protocol rows, transitive source declarations, inherited duplicates, signature separation, source sharing, quotas, allocator refusals, cancellation, Unicode and detached four-reader lifetime\n",checks);return 0;}
