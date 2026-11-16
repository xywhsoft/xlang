#define main oldIrMemoMain
#include "analysis_ir_memo_test.c"
#undef main
#include "analysis/ir_type_link.h"
#include "analysis/ir_table_graph.h"

static size_t faultAt,faultCalls,denied,live,liveBytes,peakBytes;
static int faultActive,overflow;
static void* allocations[4096];
static size_t allocationBytes[4096];
void* __real_xrtMalloc(size_t);void* __real_xrtCalloc(size_t,size_t);void __real_xrtFree(void*);
static int refuse(void){if(faultActive && ++faultCalls==faultAt){++denied;return 1;}return 0;}
static void remember(void* p,size_t bytes)
{
 if(!faultActive || !p)return;
 for(size_t i=0;i<4096;++i)if(allocations[i]==p)return;
 for(size_t i=0;i<4096;++i)if(!allocations[i]){allocations[i]=p;allocationBytes[i]=bytes;++live;
  liveBytes+=bytes;if(liveBytes>peakBytes)peakBytes=liveBytes;return;}
 overflow=1;
}
static void forget(void* p)
{if(p)for(size_t i=0;i<4096;++i)if(allocations[i]==p){allocations[i]=NULL;--live;liveBytes-=allocationBytes[i];return;}}
void* __wrap_xrtMalloc(size_t n){if(refuse())return NULL;void* p=__real_xrtMalloc(n);remember(p,n);return p;}
void* __wrap_xrtCalloc(size_t n,size_t s){if(refuse())return NULL;void* p=__real_xrtCalloc(n,s);remember(p,n*s);return p;}
void __wrap_xrtFree(void* p){forget(p);__real_xrtFree(p);}

/* Independent native-byte/payload book and handwritten relocation oracle. */
#define TYPE_TEXT_FIELDS(F) F(name) F(displayName) F(foreignHeader) F(foreignRecordSource) F(foreignPolicy) F(foreignScope) F(foreignWatch) F(foreignDomain)
#define TYPE_ID_FIELDS(F) F(persistencePayloadType) F(messageSchemaType) F(treeKeySchemaType) F(treeValueSchemaType) F(collectionValueSchemaType) F(foreignBase) F(foreignOwner) F(callableResultType) F(sequenceLayout.elementType) F(sequenceLayout.baseElementType) F(sequenceLayout.childElementType)
static int append(xstrbuf* out,const void* p,size_t n){return xrtStrBufAppend(out,(xstrview){p,n});}
static int poolBook(xstrbuf* out,const XL6_IrTypePool* pool)
{
 CHECK(append(out,&pool->count,sizeof(pool->count)) && append(out,&pool->slotCapacity,sizeof(pool->slotCapacity)));
 for(size_t i=0;i<pool->count;++i){const XL6_IrType* t=&pool->items[i];XL6_IrType scalar=*t;
#define CLEAR(f) scalar.f=NULL;
  TYPE_TEXT_FIELDS(CLEAR)
#undef CLEAR
  scalar.params=NULL;scalar.returns=NULL;scalar.parameterModes=NULL;scalar.parameterAbis=NULL;CHECK(append(out,&scalar,sizeof(scalar)));
#define TEXT(f) do{unsigned char p=t->f!=NULL;CHECK(append(out,&p,1) && (!p || append(out,t->f,strlen(t->f)+1)));}while(0);
  TYPE_TEXT_FIELDS(TEXT)
#undef TEXT
#define VECTOR(f,n) do{unsigned char p=t->f!=NULL;CHECK(append(out,&p,1) && (!p || append(out,t->f,t->n*sizeof(*t->f))));}while(0)
  VECTOR(params,paramCount);VECTOR(returns,returnCount);VECTOR(parameterModes,paramCount);VECTOR(parameterAbis,paramCount);
#undef VECTOR
 }
 CHECK(!pool->slotCapacity || append(out,pool->slots,pool->slotCapacity*sizeof(size_t)));return 1;
}
static int booksEqual(const xstrbuf* a,const xstrbuf* b){return a->Size==b->Size && !memcmp(a->Data,b->Data,a->Size);}
typedef struct type_map {uint64_t type[4096];int bad,mode;size_t calls;} type_map;
static int mapType(void* opaque,xls_ir_type_domain d,uint64_t old,uint64_t* to)
{
 type_map* m=opaque;++m->calls;
 if((int)d==m->bad){if(!m->mode)return 0;*to=m->mode==1 ? 0 : UINT64_MAX;return 1;}
 if(d==XLS_IR_TYPE_REFERENCE){if(old>=4096 || !m->type[old])return 0;*to=m->type[old];}
 else *to=old+(d==XLS_IR_TYPE_SYMBOL ? 100 : 200);
 return 1;
}
static int identityType(void* unused,xls_ir_type_domain d,uint64_t old,uint64_t* to)
{(void)unused;(void)d;*to=old;return 1;}
static size_t typeTable(const xls_ir_table_graph* g)
{for(size_t i=0;i<xlsIrTablesCount(g);++i)if(!strcmp(xlsIrTablesName(g,i),"types.items"))return i;return SIZE_MAX;}
static XL6_IrTypeId permuteType(XL6_IrTypeId id){return id ? id%4+1 : 0;}
typedef struct type_fixture {
 XL6_IrModule source;XL6_IrTypePool current;xls_ir_table_graph* graph;xls_ir_type_binding bindings[4];type_map map;
} type_fixture;
static int fixtureCreate(type_fixture* f)
{
 memset(f,0,sizeof(*f));f->map.bad=-1;f->source=(XL6_IrModule){.ready=1,.stage=XL6_IR_STAGE_TYPED,.root=1,.nodeCount=1,.nodeCapacity=1};
 f->source.nodes=xrtCalloc(1,sizeof(XL6_IrNode));f->source.types=(XL6_IrTypePool){.count=4,.capacity=4};
 f->source.types.items=xrtCalloc(4,sizeof(XL6_IrType));f->current=(XL6_IrTypePool){.count=4,.capacity=8,.slotCapacity=8};
 f->current.items=xrtCalloc(4,sizeof(XL6_IrType));f->current.slots=xrtCalloc(8,sizeof(size_t));CHECK(f->source.nodes && f->source.types.items && f->current.items && f->current.slots);
 f->source.nodes[0].kind=XL6_IR_NODE_MODULE;
 for(size_t i=0;i<4;++i){XL6_IrType* s=&f->source.types.items[i];s->kind=i==1 ? XL6_IR_TYPE_TYPE_PARAM : XL6_IR_TYPE_BUILTIN;
  s->sourceTypeId=(uint32)i+10;s->nominalSymbol=i%2 ? 0 : 9;s->abiId=1234;s->typeValueId=9876;
  snprintf(s->cName,sizeof(s->cName),"old_%zu",i);snprintf(s->errorResultCName,sizeof(s->errorResultCName),"old_result_%zu",i);
  s->hash=i+1;s->sequenceLayout.abi=XL6_IR_SEQUENCE_LAYOUT_NONE;
#define TEXT(f) s->f=xrtStrDup("type 中文😀");CHECK(s->f);
  TYPE_TEXT_FIELDS(TEXT)
#undef TEXT
#define REF(f) s->f=i%2 ? 0 : 2;
  TYPE_ID_FIELDS(REF)
#undef REF
  s->paramCount=i%2 ? 0 : 2;s->returnCount=i%2 ? 0 : 1;
  if(i!=3){s->params=xrtCalloc(s->paramCount ? s->paramCount : 1,sizeof(*s->params));s->returns=xrtCalloc(s->returnCount ? s->returnCount : 1,sizeof(*s->returns));CHECK(s->params && s->returns);}
  if(s->paramCount){s->params[0]=2;s->params[1]=0;s->returns[0]=4;s->parameterModes=xrtCalloc(2,sizeof(*s->parameterModes));s->parameterAbis=xrtCalloc(2,sizeof(*s->parameterAbis));CHECK(s->parameterModes && s->parameterAbis);}
  XL6_IrType* t=&f->current.items[permuteType((XL6_IrTypeId)i+1)-1];*t=*s;
#define MAP(f) t->f=permuteType(s->f);
  TYPE_ID_FIELDS(MAP)
#undef MAP
  if(t->nominalSymbol)t->nominalSymbol+=100;
  t->sourceTypeId+=200;t->hash+=1000;
  snprintf(t->cName,sizeof(t->cName),"new_%zu",i);snprintf(t->errorResultCName,sizeof(t->errorResultCName),"new_result_%zu",i);
  if(s->params){t->params=xrtCalloc(s->paramCount ? s->paramCount : 1,sizeof(*s->params));CHECK(t->params);for(size_t j=0;j<s->paramCount;++j)t->params[j]=permuteType(s->params[j]);}
  if(s->returns){t->returns=xrtCalloc(s->returnCount ? s->returnCount : 1,sizeof(*s->returns));CHECK(t->returns);for(size_t j=0;j<s->returnCount;++j)t->returns[j]=permuteType(s->returns[j]);}
  f->map.type[i+1]=permuteType((XL6_IrTypeId)i+1);f->current.slots[i*2]=i+1;
 }
 f->current.slots[7]=SIZE_MAX-7; /* Preserve opaque native lookup words. */
 xls_ir_graph_status status;f->graph=xlsIrTablesCapture(&f->source,SIZE_MAX,NULL,NULL,&status);CHECK(f->graph && status==XLS_IR_GRAPH_OK);size_t table=typeTable(f->graph);CHECK(table!=SIZE_MAX);
 for(size_t i=0;i<4;++i){xls_ir_table_unit* u=xlsIrTablesPinUnit(f->graph,table,i);CHECK(u);f->bindings[3-i]=(xls_ir_type_binding){u,xlsIrTableUnitProducer(u),mapType,&f->map};}
 return 1;
}
static void fixtureFree(type_fixture* f)
{
 for(size_t i=0;i<4;++i){xlsIrTableUnitRelease(f->bindings[i].unit);XL6_IrType* s=&f->source.types.items[i];
#define FREE(f) xrtFree(s->f);
  TYPE_TEXT_FIELDS(FREE)
#undef FREE
  xrtFree(s->params);xrtFree(s->returns);xrtFree(s->parameterModes);xrtFree(s->parameterAbis);
  xrtFree(f->current.items[i].params);xrtFree(f->current.items[i].returns);}
 xlsIrTablesRelease(f->graph);xrtFree(f->source.nodes);xrtFree(f->source.types.items);xrtFree(f->current.items);xrtFree(f->current.slots);memset(f,0,sizeof(*f));
}
static int poolMatches(const XL6_IrTypePool* expected,const xls_ir_type_link* image)
{
 const XL6_IrTypePool* actual=xlsIrTypeLinkPool(image);CHECK(actual && actual->capacity==actual->count);
 xstrbuf a={0},b={0};CHECK(poolBook(&a,expected) && poolBook(&b,actual) && booksEqual(&a,&b));xrtStrBufFree(&a);xrtStrBufFree(&b);return 1;
}
typedef struct type_stop {size_t at,calls;} type_stop;
static int cancelledType(void* opaque){type_stop* s=opaque;return ++s->calls==s->at;}
static int typeFailures(void)
{
 type_fixture f;CHECK(fixtureCreate(&f));xls_ir_graph_status status;xls_ir_type_error error;
 faultActive=1;faultAt=faultCalls=denied=peakBytes=0;xls_ir_type_link* image=xlsIrTypeLink(&f.current,f.bindings,4,SIZE_MAX,NULL,NULL,&status,&error);faultActive=0;
 CHECK(image && status==XLS_IR_GRAPH_OK && !error.reason && live==1 && faultCalls==2 && !overflow && poolMatches(&f.current,image));
 CHECK(liveBytes==xlsIrTypeLinkBytes(image) && peakBytes==xlsIrTypeLinkPeakBytes(image));size_t peak=xlsIrTypeLinkPeakBytes(image);xlsIrTypeLinkDestroy(image);CHECK(!live && !liveBytes);
 image=xlsIrTypeLink(&f.current,f.bindings,4,peak,NULL,NULL,&status,&error);CHECK(image);xlsIrTypeLinkDestroy(image);
 CHECK(!xlsIrTypeLink(&f.current,f.bindings,4,peak-1,NULL,NULL,&status,&error) && status==XLS_IR_GRAPH_BUDGET);
 CHECK(!xlsIrTypeLink(&f.current,f.bindings,4,0,NULL,NULL,&status,&error) && status==XLS_IR_GRAPH_BUDGET);
 for(size_t i=1;i<=2;++i){faultActive=1;faultAt=i;faultCalls=denied=0;image=xlsIrTypeLink(&f.current,f.bindings,4,SIZE_MAX,NULL,NULL,&status,&error);faultActive=0;
  CHECK(!image && status==XLS_IR_GRAPH_MEMORY && denied==1 && !live && !liveBytes);}
 type_stop stop={0};image=xlsIrTypeLink(&f.current,f.bindings,4,SIZE_MAX,cancelledType,&stop,&status,&error);CHECK(image);size_t calls=stop.calls;xlsIrTypeLinkDestroy(image);
 for(size_t i=1;i<=calls;++i){stop=(type_stop){.at=i};faultActive=1;faultAt=faultCalls=denied=0;image=xlsIrTypeLink(&f.current,f.bindings,4,SIZE_MAX,cancelledType,&stop,&status,&error);faultActive=0;CHECK(!image && status==XLS_IR_GRAPH_CANCELLED && !live && !liveBytes);}
#define BAD(why) do{image=xlsIrTypeLink(&f.current,f.bindings,4,SIZE_MAX,NULL,NULL,&status,&error);CHECK(!image && status==XLS_IR_GRAPH_INVALID && error.reason==(why));}while(0)
 for(int d=0;d<3;++d)for(int mode=0;mode<3;++mode){f.map.bad=d;f.map.mode=mode;BAD(mode ? XLS_IR_TYPE_RANGE : XLS_IR_TYPE_UNMAPPED);}
 f.map.bad=-1;
 const void* producer=f.bindings[0].producer;f.bindings[0].producer=&f;BAD(XLS_IR_TYPE_NAMESPACE);f.bindings[0].producer=producer;
 uint64_t mapped=f.map.type[1];f.map.type[1]=f.map.type[2];BAD(XLS_IR_TYPE_DUPLICATE);f.map.type[1]=mapped;
 XL6_IrType* t=&f.current.items[0];uint64_t abi=t->abiId;t->abiId^=1;BAD(XLS_IR_TYPE_DIFFERENT);t->abiId=abi;
 char* text=t->foreignDomain;t->foreignDomain="different";BAD(XLS_IR_TYPE_DIFFERENT);t->foreignDomain=text;
 size_t n=t->paramCount;t->paramCount++;BAD(XLS_IR_TYPE_DIFFERENT);t->paramCount=n;
 size_t slot=f.current.slots[0];f.current.slots[0]=SIZE_MAX;
 image=xlsIrTypeLink(&f.current,f.bindings,4,SIZE_MAX,NULL,NULL,&status,&error);CHECK(image && poolMatches(&f.current,image));xlsIrTypeLinkDestroy(image);f.current.slots[0]=slot;
 /* Fixed type-param provenance must not be silently refreshed. */
 t=&f.current.items[permuteType(2)-1];uint32_t source=t->sourceTypeId;t->sourceTypeId++;BAD(XLS_IR_TYPE_DIFFERENT);t->sourceTypeId=source;
#undef BAD
 image=xlsIrTypeLink(&f.current,f.bindings,4,SIZE_MAX,NULL,NULL,&status,&error);CHECK(image && poolMatches(&f.current,image));
 const XL6_IrTypePool* pool=xlsIrTypeLinkPool(image);const XL6_IrType* unit=xlsIrTableUnitRow(f.bindings[0].unit,0);CHECK(pool->items[permuteType(4)-1].name==unit->name);
 xstrbuf expected={0};CHECK(poolBook(&expected,&f.current));fixtureFree(&f);xstrbuf actual={0};CHECK(poolBook(&actual,pool) && booksEqual(&expected,&actual));
 xrtStrBufFree(&actual);xrtStrBufFree(&expected);xlsIrTypeLinkDestroy(image);
 printf("type-link allocation sites: 2; cancellation points: %zu\n",calls);return 1;
}

static char* typeRealDump(const XL6_IrModule*);
void __real_xl6IrDumpModuleJson(XL6_Output*,const XL6_IrModule*);
static xatomic32 oracleRuns,oracleErrors,oracleActive;static _Thread_local int oracleRecursing;
static xls_ir_type_link* identityPool(const XL6_IrModule* source)
{
 xls_ir_graph_status status;xls_ir_table_graph* g=xlsIrTablesCapture(source,SIZE_MAX,NULL,NULL,&status);if(!g)return NULL;size_t table=typeTable(g),count=source->types.count;
 xls_ir_type_binding* b=xrtCalloc(count ? count : 1,sizeof(*b));if(!b){xlsIrTablesRelease(g);return NULL;}
 for(size_t i=0;i<count;++i){b[i].unit=xlsIrTablesPinUnit(g,table,i);b[i].producer=xlsIrTableUnitProducer(b[i].unit);b[i].map=identityType;}
 xls_ir_type_error error;xls_ir_type_link* link=xlsIrTypeLink(&source->types,b,count,SIZE_MAX,NULL,NULL,&status,&error);
 if(!link)fprintf(stderr,"native type-link status=%d reason=%d source=%u field=%s\n",status,error.reason,error.source,error.field ? error.field : "none");
 for(size_t i=0;i<count;++i)xlsIrTableUnitRelease(b[i].unit);
 xrtFree(b);xlsIrTablesRelease(g);return link;
}
typedef struct owned_view {xls_ir_node_image* nodes;xls_ir_table_image* tables;XL6_IrModule module;} owned_view;
static int viewCreate(const XL6_IrModule* source,owned_view* view)
{
 memset(view,0,sizeof(*view));xls_ir_graph_status status;xls_ir_node_graph* nodes=xlsIrNodeGraphCapture(source,SIZE_MAX,NULL,NULL,&status);CHECK(nodes);
 view->nodes=xlsIrNodeGraphMaterialize(nodes,SIZE_MAX,NULL,NULL,&status);xlsIrNodeGraphRelease(nodes);CHECK(view->nodes);
 xls_ir_table_graph* tables=xlsIrTablesCapture(source,SIZE_MAX,NULL,NULL,&status);CHECK(tables);
 view->tables=xlsIrTablesMaterialize(tables,xlsIrNodeImageNodes(view->nodes,NULL),source->nodeCount,xlsIrNodeImageChildren(view->nodes,NULL),source->childCount,SIZE_MAX,NULL,NULL,&status);xlsIrTablesRelease(tables);CHECK(view->tables);
 view->module=*xlsIrTableImageModule(view->tables);return 1;
}
static void viewFree(owned_view* view){xlsIrTableImageDestroy(view->tables);xlsIrNodeImageDestroy(view->nodes);memset(view,0,sizeof(*view));}
static int verifyTypeView(const XL6_IrModule* actual,const char* expected)
{
 XL6_DiagList diags;xl6DiagInit(&diags);CHECK(xl6IrVerifyModule(actual,&diags) && !diags.count);xl6DiagFree(&diags);
 char* text=typeRealDump(actual);CHECK(text && !strcmp(text,expected));xrtFree(text);return 1;
}
static int typeOracle(const XL6_IrModule* source)
{
 xls_ir_type_link* link=identityPool(source);CHECK(link);owned_view view;CHECK(viewCreate(source,&view));view.module.types=*xlsIrTypeLinkPool(link);
 char* expected=typeRealDump(source);CHECK(expected && verifyTypeView(&view.module,expected));xrtFree(expected);viewFree(&view);xlsIrTypeLinkDestroy(link);return 1;
}
void __wrap_xl6IrDumpModuleJson(XL6_Output* out,const XL6_IrModule* module)
{
 if(xrtAtomic32Load(&oracleActive,XMEMORY_ACQUIRE) && !oracleRecursing){oracleRecursing=1;xrtAtomic32FetchAdd(&oracleRuns,1,XMEMORY_RELAXED);
  if(!typeOracle(module))xrtAtomic32FetchAdd(&oracleErrors,1,XMEMORY_RELAXED);
  oracleRecursing=0;}
 __real_xl6IrDumpModuleJson(out,module);
}
static char* typeRealDump(const XL6_IrModule* module)
{XL6_Output out;if(!xl6OutputOpenMemory(&out))return NULL;__real_xl6IrDumpModuleJson(&out,module);char* value=xl6OutputHasFailed(&out) ? NULL : xl6OutputTakeMemoryText(&out,NULL);xl6OutputDiscard(&out);return value;}
static int typeNative(void)
{
 char* path=xrtPathAbs("ir-type-link-中文.xl");CHECK(path);
 for(unsigned edit=0;edit<8;++edit){char* text=body(edit);CHECK(text);xls_semantic_check* check=checked(path,text);CHECK(check && xlsSemanticCheckInfo(check).accepted);
  const XL6_IrModule* source=xlsSemanticCheckIr(check);CHECK(source);char* expected=typeRealDump(source);CHECK(expected);
  xls_ir_type_link* link=identityPool(source);CHECK(link);owned_view view;CHECK(viewCreate(source,&view));view.module.types=*xlsIrTypeLinkPool(link);
  xlsSemanticCheckDestroy(check);xrtFree(text);CHECK(verifyTypeView(&view.module,expected));xrtFree(expected);viewFree(&view);xlsIrTypeLinkDestroy(link);}
 xrtAtomic32Store(&oracleActive,1,XMEMORY_RELEASE);int okay=corpus(path) && regionCorpus(path);xrtAtomic32Store(&oracleActive,0,XMEMORY_RELEASE);
 CHECK(okay && !xrtAtomic32Load(&oracleErrors,XMEMORY_RELAXED) && xrtAtomic32Load(&oracleRuns,XMEMORY_RELAXED)>20);xrtFree(path);return 1;
}
static int emptyTypes(void)
{
 XL6_IrTypePool empty={0};xls_ir_graph_status status;xls_ir_type_error error;
 xls_ir_type_link* link=xlsIrTypeLink(&empty,NULL,0,SIZE_MAX,NULL,NULL,&status,&error);CHECK(link && status==XLS_IR_GRAPH_OK && !xlsIrTypeLinkPool(link)->count && !xlsIrTypeLinkPool(link)->items);
 CHECK(xlsIrTypeLinkBytes(link)==xlsIrTypeLinkPeakBytes(link));xlsIrTypeLinkDestroy(link);return 1;
}
static XL6_IrTypeId floatSignature(const XL6_IrModule* m)
{
 for(size_t i=0;i<m->types.count;++i){const XL6_IrType* t=&m->types.items[i];
  if(t->kind==XL6_IR_TYPE_FUNCTION && t->paramCount==1 && t->returnCount==1 && t->params && t->returns && t->params[0]==t->returns[0]){
   const XL6_IrType* p=xl6IrTypeGet(&m->types,t->params[0]);if(p && p->cAbi==XL6_IR_C_ABI_F64)return (XL6_IrTypeId)i+1;}}
 return 0;
}
static int sameTypeIdentity(const XL6_IrType* a,const XL6_IrType* b)
{return a->kind==b->kind && a->cAbi==b->cAbi && a->abiId==b->abiId && a->typeValueId==b->typeValueId && a->paramCount==b->paramCount && a->returnCount==b->returnCount && a->name && b->name && !strcmp(a->name,b->name);}
static int mixedNativeTypes(void)
{
 /* Independent full native analyses establish test correspondences. These
  * are not a production matcher or a dependency/freshness certificate. */
 const char* code[]={
  "func lead(int value) as int\n return value\nend\nfunc late(float value) as float\n return value\nend\nfunc bridge(func(float) as float callback) as int\n return 0\nend\nfunc main(...) as int\n return lead(3)\nend\n",
  "func lead(int value) as int\n array<float> inserted = [1.5]\n return value\nend\nfunc late(float value) as float\n return value\nend\nfunc bridge(func(float) as float callback) as int\n return 0\nend\nfunc main(...) as int\n return lead(3)\nend\n"};
 char* path=xrtPathAbs("type-link-mixed.xl");CHECK(path);xls_semantic_check* check[2]={0};XL6_IrModule module[2];XL6_IrTypeId callback[2];
 XL6_DiagList diags;xl6DiagInit(&diags);
 for(size_t i=0;i<2;++i){check[i]=checked(path,code[i]);CHECK(check[i] && xlsSemanticCheckInfo(check[i]).accepted);xl6IrModuleInit(&module[i]);
  CHECK(xl6IrBuildModuleFromAst(&module[i],xlsSemanticCheckAst(check[i]),&diags) && !diags.count);
  XL6_IrTypeId signature=floatSignature(&module[i]);CHECK(signature);callback[i]=xl6IrTypeInternCFunctionPointer(&module[i],signature);CHECK(callback[i]);
  module[i].stats.typeCount=module[i].types.count; /* additional native interning */
  int verified=xl6IrVerifyModule(&module[i],&diags);
  if(!verified)for(size_t d=0;d<diags.count;++d)fprintf(stderr,"native callback fixture: %s\n",diags.items[d].message);
  CHECK(verified && !diags.count);}
 CHECK(callback[0]!=callback[1]);CHECK(strcmp(module[0].types.items[callback[0]-1].cName,module[1].types.items[callback[1]-1].cName));
 xls_ir_graph_status status;xls_ir_table_graph* graph[2];for(size_t i=0;i<2;++i){graph[i]=xlsIrTablesCapture(&module[i],SIZE_MAX,NULL,NULL,&status);CHECK(graph[i]);}
 type_map* map=xrtCalloc(1,sizeof(*map));CHECK(map);map->bad=-1;size_t oldCount=module[0].types.count,newCount=module[1].types.count;CHECK(oldCount<4096 && newCount<4096);
 for(size_t i=0;i<oldCount;++i){size_t found=SIZE_MAX;for(size_t j=0;j<newCount;++j)if(sameTypeIdentity(&module[0].types.items[i],&module[1].types.items[j])){CHECK(found==SIZE_MAX);found=j;}
  if(found!=SIZE_MAX)map->type[i+1]=found+1;}
 CHECK(map->type[callback[0]]==callback[1]);xls_ir_type_binding* bindings=xrtCalloc(newCount,sizeof(*bindings));CHECK(bindings);size_t reused=0,rebuilt=0,relocated=0;const char* kept=NULL;size_t keptAt=0;
 for(size_t j=0;j<newCount;++j){size_t old=SIZE_MAX;for(size_t i=0;i<oldCount;++i)if(map->type[i+1]==j+1){CHECK(old==SIZE_MAX);old=i;}
  size_t which=old==SIZE_MAX ? 1 : 0;size_t row=old==SIZE_MAX ? j : old;
  xls_ir_table_unit* unit=xlsIrTablesPinUnit(graph[which],typeTable(graph[which]),row);CHECK(unit);
  bindings[j]=(xls_ir_type_binding){unit,xlsIrTableUnitProducer(unit),which ? identityType : mapType,which ? NULL : map};
  if(which)++rebuilt;else{++reused;if(old!=j)++relocated;kept=((const XL6_IrType*)xlsIrTableUnitRow(unit,0))->name;keptAt=j;}}
 CHECK(reused>3 && rebuilt && relocated>1 && kept);xls_ir_type_error error;
 xls_ir_type_link* image=xlsIrTypeLink(&module[1].types,bindings,newCount,SIZE_MAX,NULL,NULL,&status,&error);
 if(!image)fprintf(stderr,"mixed type status=%d reason=%d source=%u target=%u field=%s\n",status,error.reason,error.source,error.destination,error.field ? error.field : "none");
 CHECK(image && xlsIrTypeLinkPool(image)->items[keptAt].name==kept);
 char* expected=typeRealDump(&module[1]);CHECK(expected);owned_view view;CHECK(viewCreate(&module[1],&view));view.module.types=*xlsIrTypeLinkPool(image);
 for(size_t i=0;i<newCount;++i)xlsIrTableUnitRelease(bindings[i].unit);
 xrtFree(bindings);xrtFree(map);for(size_t i=0;i<2;++i){xlsIrTablesRelease(graph[i]);xl6IrModuleFree(&module[i]);xlsSemanticCheckDestroy(check[i]);}
 CHECK(verifyTypeView(&view.module,expected));printf("mixed native types: reused=%zu rebuilt=%zu relocated=%zu; native callback names rebound\n",reused,rebuilt,relocated);
 xrtFree(expected);viewFree(&view);xlsIrTypeLinkDestroy(image);xl6DiagFree(&diags);xrtFree(path);return 1;
}
typedef struct type_reader {const type_fixture* fixture;const xstrbuf* expected;int okay;} type_reader;
static int32 readTypes(void* opaque)
{
 type_reader* reader=opaque;type_map localMap=reader->fixture->map;xls_ir_type_binding b[4];memcpy(b,reader->fixture->bindings,sizeof(b));for(size_t i=0;i<4;++i)b[i].user=&localMap;
 reader->okay=1;for(size_t i=0;i<16;++i){xls_ir_graph_status status;xls_ir_type_error error;
  xls_ir_type_link* link=xlsIrTypeLink(&reader->fixture->current,b,4,SIZE_MAX,NULL,NULL,&status,&error);
  xstrbuf book={0};int okay=link && poolBook(&book,xlsIrTypeLinkPool(link)) && booksEqual(&book,reader->expected);xrtStrBufFree(&book);xlsIrTypeLinkDestroy(link);if(!okay){reader->okay=0;break;}}
 return reader->okay ? 0 : 1;
}
static int typeLifetimes(void)
{
 type_fixture f;CHECK(fixtureCreate(&f));xstrbuf expected={0};CHECK(poolBook(&expected,&f.current));
 type_reader readers[4];xthread* threads[4];for(size_t i=0;i<4;++i){readers[i]=(type_reader){&f,&expected,0};threads[i]=xrtThreadCreate(readTypes,&readers[i],0);CHECK(threads[i]);}
 for(size_t i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}
 xls_ir_type_link* generations[32];
 for(size_t i=0;i<32;++i){xls_ir_graph_status status;xls_ir_type_error error;faultActive=1;faultAt=faultCalls=denied=0;
  xls_ir_table_graph* g=xlsIrTablesCapture(&f.source,SIZE_MAX,NULL,NULL,&status);CHECK(g);xls_ir_type_binding b[4];
  for(size_t j=0;j<4;++j){xls_ir_table_unit* unit=xlsIrTablesPinUnit(g,typeTable(g),j);CHECK(unit);b[j]=(xls_ir_type_binding){unit,xlsIrTableUnitProducer(unit),mapType,&f.map};}
  generations[i]=xlsIrTypeLink(&f.current,b,4,SIZE_MAX,NULL,NULL,&status,&error);faultActive=0;CHECK(generations[i]);
  for(size_t j=0;j<4;++j)xlsIrTableUnitRelease(b[j].unit);
  xlsIrTablesRelease(g);CHECK(live==6*(i+1) && !overflow); /* image + four independent units + namespace */
 }
 fixtureFree(&f);
 for(size_t i=32;i--;){xstrbuf actual={0};CHECK(poolBook(&actual,xlsIrTypeLinkPool(generations[i])) && booksEqual(&expected,&actual));xrtStrBufFree(&actual);xlsIrTypeLinkDestroy(generations[i]);}
 CHECK(!live && !liveBytes);xrtStrBufFree(&expected);return 1;
}
static int typeVectors(void)
{
 const size_t n=4097;XL6_IrNode node={.kind=XL6_IR_NODE_MODULE};XL6_IrType type={0};type.paramCount=type.returnCount=n;
 type.params=xrtCalloc(n,sizeof(*type.params));type.returns=xrtCalloc(n,sizeof(*type.returns));CHECK(type.params && type.returns);
 for(size_t i=0;i<n;++i)type.params[i]=type.returns[i]=i%2 ? 0 : 1;
 XL6_IrModule module={.ready=1,.stage=XL6_IR_STAGE_TYPED,.root=1,.nodes=&node,.nodeCount=1,.nodeCapacity=1,.types={.items=&type,.count=1,.capacity=1}};
 xls_ir_graph_status status;xls_ir_type_error error;xls_ir_table_graph* graph=xlsIrTablesCapture(&module,SIZE_MAX,NULL,NULL,&status);CHECK(graph);
 xls_ir_table_unit* unit=xlsIrTablesPinUnit(graph,typeTable(graph),0);CHECK(unit);xls_ir_type_binding b={unit,xlsIrTableUnitProducer(unit),identityType,NULL};type_stop stop={0};
 faultActive=1;faultAt=faultCalls=denied=peakBytes=0;xls_ir_type_link* link=xlsIrTypeLink(&module.types,&b,1,SIZE_MAX,cancelledType,&stop,&status,&error);faultActive=0;
 CHECK(link && status==XLS_IR_GRAPH_OK && live==1 && peakBytes==xlsIrTypeLinkPeakBytes(link) && liveBytes==xlsIrTypeLinkBytes(link) && poolMatches(&module.types,link));
 size_t calls=stop.calls;xlsIrTypeLinkDestroy(link);CHECK(!live && calls>100);
 const size_t cuts[]={1,2,4,10,50,100,calls};for(size_t i=0;i<sizeof(cuts)/sizeof(*cuts);++i){stop=(type_stop){.at=cuts[i]};faultActive=1;faultAt=faultCalls=denied=0;
  link=xlsIrTypeLink(&module.types,&b,1,SIZE_MAX,cancelledType,&stop,&status,&error);faultActive=0;CHECK(!link && status==XLS_IR_GRAPH_CANCELLED && !live && !liveBytes);}
 xlsIrTableUnitRelease(unit);xlsIrTablesRelease(graph);xrtFree(type.params);xrtFree(type.returns);return 1;
}
int main(void)
{
 xrtAtomic32Init(&checks,0);xrtAtomic32Init(&oracleRuns,0);xrtAtomic32Init(&oracleErrors,0);xrtAtomic32Init(&oracleActive,0);
 if(!typeFailures() || !emptyTypes() || !typeNative() || !mixedNativeTypes() || !typeLifetimes() || !typeVectors())return 1;
 printf("analysis_ir_type_link_test passed: %u checks; %u complete native verifier/JSON oracles; current derived metadata,explicit references,complete equality,retirement,budgets,OOM,cancellation\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED),xrtAtomic32Load(&oracleRuns,XMEMORY_RELAXED));return 0;
}
