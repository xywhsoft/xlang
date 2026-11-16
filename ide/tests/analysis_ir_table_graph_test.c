#define main oldIrMemoMain
#include "analysis_ir_memo_test.c"
#undef main
#include "analysis/ir_table_graph.h"

static size_t faultAt,faultCalls,denied,live,liveBytes,peakBytes;
static int faultActive,overflow;
static void* allocations[4096];static size_t allocationBytes[4096];
void* __real_xrtMalloc(size_t);void* __real_xrtCalloc(size_t,size_t);void __real_xrtFree(void*);
static int refuse(void){if(faultActive && ++faultCalls==faultAt){++denied;return 1;}return 0;}
static void remember(void* p,size_t n)
{
 if(!faultActive || !p)return;
 for(size_t i=0;i<4096;++i)if(allocations[i]==p)return;
 for(size_t i=0;i<4096;++i)if(!allocations[i]){allocations[i]=p;allocationBytes[i]=n;++live;liveBytes+=n;if(liveBytes>peakBytes)peakBytes=liveBytes;return;}
 overflow=1;
}
static void forget(void* p)
{if(p)for(size_t i=0;i<4096;++i)if(allocations[i]==p){allocations[i]=NULL;--live;liveBytes-=allocationBytes[i];return;}}
void* __wrap_xrtMalloc(size_t n){if(refuse())return NULL;void* p=__real_xrtMalloc(n);remember(p,n);return p;}
void* __wrap_xrtCalloc(size_t n,size_t s){if(refuse())return NULL;void* p=__real_xrtCalloc(n,s);remember(p,n*s);return p;}
void __wrap_xrtFree(void* p){forget(p);__real_xrtFree(p);}
typedef struct table_fixture {XL6_IrModule module;void* owned[512];size_t count;} table_fixture;
static void* tableFixtureAlloc(table_fixture* f,size_t count,size_t width)
{
 if(f->count==512 || (width && count>SIZE_MAX/width))return NULL;
 void* p=xrtCalloc(count,width);if(p)f->owned[f->count++]=p;return p;
}
static char* tableFixtureText(table_fixture* f,const char* text)
{size_t n=strlen(text)+1;char* p=tableFixtureAlloc(f,n,1);if(p)memcpy(p,text,n);return p;}
static void tableFixtureFree(table_fixture* f)
{while(f->count)xrtFree(f->owned[--f->count]);*f=(table_fixture){0};}
static int tableAppend(xstrbuf* out,const void* p,size_t n)
{return !n || xrtStrBufAppend(out,(xstrview){p,n});}
static int tableAppendText(xstrbuf* out,const char* text)
{unsigned char present=text!=NULL;return tableAppend(out,&present,1) && (!present || tableAppend(out,text,strlen(text)+1));}
#include "ir-table-schema-test.inc"
static size_t tableFind(const xls_ir_table_graph* g,const char* name)
{for(size_t i=0;i<xlsIrTablesCount(g);++i)if(!strcmp(xlsIrTablesName(g,i),name))return i;return SIZE_MAX;}
static int tableBooksEqual(const xstrbuf* a,const xstrbuf* b)
{return a->Size==b->Size && (!a->Size || !memcmp(a->Data,b->Data,a->Size));}
static int tableSynthetic(int empty)
{
 table_fixture fixture={0};CHECK(tableFixtureCreate(&fixture));XL6_IrModule* source=&fixture.module;
 if(empty){
  source->types.items[0].paramCount=source->types.items[0].returnCount=0;
  source->types.items[0].name=tableFixtureText(&fixture,"");CHECK(source->types.items[0].name);
  source->closureTargets[0].callParameterCount=0;source->compiledPackages[0].interfaceSize=0;
  source->instructions[0].immediateSize=0;
 }
 xstrbuf expected={0};CHECK(tableModuleBook(&expected,source));xls_ir_graph_status status;
 faultActive=1;faultAt=faultCalls=denied=peakBytes=0;
 xls_ir_table_graph* g=xlsIrTablesCapture(source,SIZE_MAX,NULL,NULL,&status);faultActive=0;
 CHECK(g && status==XLS_IR_GRAPH_OK && xlsIrTablesCount(g)==107 && !overflow);
 CHECK(xlsIrTablesBytes(g)==liveBytes && peakBytes==liveBytes);
 size_t exact=xlsIrTablesBytes(g);
 xls_ir_table_graph* duplicate=xlsIrTablesCapture(source,exact,NULL,NULL,&status);CHECK(duplicate && status==XLS_IR_GRAPH_OK);xlsIrTablesRelease(duplicate);
 CHECK(!xlsIrTablesCapture(source,exact-1,NULL,NULL,&status) && status==XLS_IR_GRAPH_BUDGET);
 CHECK(!xlsIrTablesPinUnit(g,107,0) && !xlsIrTablesName(g,107));
 size_t scalar=tableFind(g,"symbolNodes");CHECK(scalar!=SIZE_MAX && xlsIrTablesRows(g,scalar)==3 && xlsIrTablesUnits(g,scalar)==1);
 size_t table=tableFind(g,"persistenceRestoreFields");CHECK(table!=SIZE_MAX);
 xls_ir_table_unit* unit=xlsIrTablesPinUnit(g,table,0);CHECK(unit && xlsIrTableUnitRows(unit)==1);
 CHECK(xlsIrTableUnitFirstRow(unit)==0 && xlsIrTableUnitRowBytes(unit)==sizeof(XL6_IrPersistenceRestoreField));
 CHECK(!strcmp(xlsIrTableUnitName(unit),"persistenceRestoreFields") && !xlsIrTableUnitRow(unit,1));
 CHECK(((const XL6_IrPersistenceRestoreField*)xlsIrTableUnitRow(unit,0))->sourceKey!=source->classFields[0].jsonPersistenceKeyLiteral);
 xstrbuf unitExpected={0};CHECK(book_XL6_IrPersistenceRestoreField(&unitExpected,source->persistenceRestoreFields));
 xls_ir_node_graph* nodes=xlsIrNodeGraphCapture(source,SIZE_MAX,NULL,NULL,&status);CHECK(nodes);
 xls_ir_node_image* nodeImage=xlsIrNodeGraphMaterialize(nodes,SIZE_MAX,NULL,NULL,&status);CHECK(nodeImage);xlsIrNodeGraphRelease(nodes);
 size_t n,c;const XL6_IrNode* nodeData=xlsIrNodeImageNodes(nodeImage,&n);const XL6_IrNodeId* childData=xlsIrNodeImageChildren(nodeImage,&c);
 xls_ir_table_image* image=xlsIrTablesMaterialize(g,nodeData,n,childData,c,SIZE_MAX,NULL,NULL,&status);CHECK(image && status==XLS_IR_GRAPH_OK);
 const XL6_IrModule* view=xlsIrTableImageModule(image);CHECK(view);
 CHECK(view->persistenceProjectFields[0].targetKey==view->classFields[0].name);
 CHECK(view->persistenceRestoreFields[0].sourceKey==view->classFields[0].jsonPersistenceKeyLiteral);
 CHECK(view->persistenceRestoreClaims[0].key==view->classFields[0].xlonPersistenceKeyLiteral);
 CHECK(!xlsIrTablesMaterialize(g,nodeData,n,childData,c,xlsIrTableImageBytes(image)-1,NULL,NULL,&status) && status==XLS_IR_GRAPH_BUDGET);
 CHECK(!xlsIrTablesMaterialize(g,nodeData,n+1,childData,c,SIZE_MAX,NULL,NULL,&status) && status==XLS_IR_GRAPH_INVALID);
 tableFixtureFree(&fixture);xlsIrTablesRelease(g);
 xstrbuf actual={0};CHECK(tableModuleBook(&actual,view) && tableBooksEqual(&expected,&actual));xrtStrBufFree(&actual);
 xlsIrTableImageDestroy(image);xlsIrNodeImageDestroy(nodeImage);
 CHECK(live==2 && liveBytes==xlsIrTableUnitBytes(unit));
 CHECK(tableUnitBook(&actual,table,unit) && tableBooksEqual(&unitExpected,&actual));
 xrtStrBufFree(&actual);xrtStrBufFree(&unitExpected);xrtStrBufFree(&expected);xlsIrTableUnitRelease(unit);CHECK(!live && !liveBytes);
 return 1;
}
typedef struct table_stop {size_t calls,at;} table_stop;
static int tableCancelled(void* opaque){table_stop* p=opaque;return ++p->calls==p->at;}
static int tableFailures(void)
{
 table_fixture fixture={0};CHECK(tableFixtureCreate(&fixture));xls_ir_graph_status status;XL6_IrModule* source=&fixture.module;
 xstrbuf original={0};CHECK(tableModuleBook(&original,source));faultActive=1;faultAt=faultCalls=denied=0;
 xls_ir_table_graph* g=xlsIrTablesCapture(source,SIZE_MAX,NULL,NULL,&status);faultActive=0;CHECK(g && !overflow);
 size_t sites=faultCalls;CHECK(sites>100);xlsIrTablesRelease(g);CHECK(!live && !liveBytes);
 for(size_t i=1;i<=sites;++i){faultActive=1;faultAt=i;faultCalls=denied=0;
  g=xlsIrTablesCapture(source,SIZE_MAX,NULL,NULL,&status);faultActive=0;
  CHECK(!g && status==XLS_IR_GRAPH_MEMORY && denied==1 && !live && !liveBytes && !overflow);
 }
 table_stop stopAt={0};g=xlsIrTablesCapture(source,SIZE_MAX,tableCancelled,&stopAt,&status);CHECK(g);size_t calls=stopAt.calls;xlsIrTablesRelease(g);
 for(size_t i=1;i<=calls;++i){stopAt=(table_stop){.at=i};faultActive=1;faultAt=faultCalls=denied=0;
  g=xlsIrTablesCapture(source,SIZE_MAX,tableCancelled,&stopAt,&status);faultActive=0;
  CHECK(!g && status==XLS_IR_GRAPH_CANCELLED && !live && !liveBytes);
 }
 faultActive=1;faultAt=faultCalls=denied=0;g=xlsIrTablesCapture(source,0,NULL,NULL,&status);faultActive=0;
 CHECK(!g && status==XLS_IR_GRAPH_BUDGET && !faultCalls && !live);
 const char* key=source->persistenceProjectFields[0].targetKey;
 source->persistenceProjectFields[0].targetKey=tableFixtureText(&fixture,key);CHECK(source->persistenceProjectFields[0].targetKey);
 g=xlsIrTablesCapture(source,SIZE_MAX,NULL,NULL,&status);CHECK(!g && status==XLS_IR_GRAPH_INVALID);source->persistenceProjectFields[0].targetKey=key;
 size_t count=source->callTargetCount;source->callTargetCount=source->callTargetCapacity+1;
 CHECK(!xlsIrTablesCapture(source,SIZE_MAX,NULL,NULL,&status) && status==XLS_IR_GRAPH_INVALID);source->callTargetCount=count;
 g=xlsIrTablesCapture(source,SIZE_MAX,NULL,NULL,&status);CHECK(g);
 faultActive=1;faultAt=1;faultCalls=denied=0;
 xls_ir_table_image* image=xlsIrTablesMaterialize(g,source->nodes,source->nodeCount,source->children,source->childCount,SIZE_MAX,NULL,NULL,&status);faultActive=0;
 CHECK(!image && status==XLS_IR_GRAPH_MEMORY && denied==1 && !live);
 stopAt=(table_stop){.at=2};faultActive=1;faultAt=faultCalls=denied=0;
 image=xlsIrTablesMaterialize(g,source->nodes,source->nodeCount,source->children,source->childCount,SIZE_MAX,tableCancelled,&stopAt,&status);faultActive=0;
 CHECK(!image && status==XLS_IR_GRAPH_CANCELLED && !live);xlsIrTablesRelease(g);
 xstrbuf after={0};CHECK(tableModuleBook(&after,source) && tableBooksEqual(&original,&after));xrtStrBufFree(&original);xrtStrBufFree(&after);
 tableFixtureFree(&fixture);printf("table-graph allocation sites: %zu; cancellation points: %zu\n",sites,calls);return 1;
}
static char* tableRealDump(const XL6_IrModule* module);
void __real_xl6IrDumpModuleJson(XL6_Output*,const XL6_IrModule*);
static xatomic32 tableOracleRuns,tableOracleErrors,tableOracleActive;static _Thread_local int tableOracleRecursing;
static int tableCloneOracle(const XL6_IrModule* source)
{
 xls_ir_graph_status status;xls_ir_table_graph* tables=xlsIrTablesCapture(source,128u*1024u*1024u,NULL,NULL,&status);CHECK(tables);
 xls_ir_node_graph* nodes=xlsIrNodeGraphCapture(source,128u*1024u*1024u,NULL,NULL,&status);CHECK(nodes);
 xls_ir_node_image* ni=xlsIrNodeGraphMaterialize(nodes,128u*1024u*1024u,NULL,NULL,&status);CHECK(ni);xlsIrNodeGraphRelease(nodes);
 size_t n,c;const XL6_IrNode* nodeData=xlsIrNodeImageNodes(ni,&n);const XL6_IrNodeId* childData=xlsIrNodeImageChildren(ni,&c);
 xls_ir_table_image* image=xlsIrTablesMaterialize(tables,nodeData,n,childData,c,128u*1024u*1024u,NULL,NULL,&status);CHECK(image);xlsIrTablesRelease(tables);
 XL6_DiagList diags;xl6DiagInit(&diags);CHECK(xl6IrVerifyModule(xlsIrTableImageModule(image),&diags) && !diags.count);xl6DiagFree(&diags);
 char* before=tableRealDump(source);char* after=tableRealDump(xlsIrTableImageModule(image));CHECK(before && after && !strcmp(before,after));
 xrtFree(before);xrtFree(after);xlsIrTableImageDestroy(image);xlsIrNodeImageDestroy(ni);return 1;
}
void __wrap_xl6IrDumpModuleJson(XL6_Output* out,const XL6_IrModule* module)
{
 if(xrtAtomic32Load(&tableOracleActive,XMEMORY_ACQUIRE) && !tableOracleRecursing){tableOracleRecursing=1;xrtAtomic32FetchAdd(&tableOracleRuns,1,XMEMORY_RELAXED);
  if(!tableCloneOracle(module)){xrtAtomic32FetchAdd(&tableOracleErrors,1,XMEMORY_RELAXED);}tableOracleRecursing=0;}
 __real_xl6IrDumpModuleJson(out,module);
}
static char* tableRealDump(const XL6_IrModule* module)
{XL6_Output out;if(!xl6OutputOpenMemory(&out))return NULL;__real_xl6IrDumpModuleJson(&out,module);char* value=xl6OutputHasFailed(&out) ? NULL : xl6OutputTakeMemoryText(&out,NULL);xl6OutputDiscard(&out);return value;}
static int tableNativeRetirement(void)
{
 char* path=xrtPathAbs("ir-table-中文.xl");CHECK(path);
 for(unsigned edit=0;edit<8;++edit){
  char* text=body(edit);CHECK(text);xls_semantic_check* check=checked(path,text);CHECK(check && xlsSemanticCheckInfo(check).accepted);
  const XL6_IrModule* source=xlsSemanticCheckIr(check);CHECK(source);char* expected=tableRealDump(source);CHECK(expected);xls_ir_graph_status status;
  xls_ir_table_graph* tables=xlsIrTablesCapture(source,SIZE_MAX,NULL,NULL,&status);CHECK(tables);
  xls_ir_node_graph* nodes=xlsIrNodeGraphCapture(source,SIZE_MAX,NULL,NULL,&status);CHECK(nodes);
  xls_ir_node_image* ni=xlsIrNodeGraphMaterialize(nodes,SIZE_MAX,NULL,NULL,&status);CHECK(ni);xlsIrNodeGraphRelease(nodes);
  size_t n,c;const XL6_IrNode* nodeData=xlsIrNodeImageNodes(ni,&n);const XL6_IrNodeId* childData=xlsIrNodeImageChildren(ni,&c);
  xls_ir_table_image* image=xlsIrTablesMaterialize(tables,nodeData,n,childData,c,SIZE_MAX,NULL,NULL,&status);CHECK(image);xlsIrTablesRelease(tables);
  xlsSemanticCheckDestroy(check);xrtFree(text);
  XL6_DiagList diags;xl6DiagInit(&diags);CHECK(xl6IrVerifyModule(xlsIrTableImageModule(image),&diags) && !diags.count);xl6DiagFree(&diags);
  char* actual=tableRealDump(xlsIrTableImageModule(image));CHECK(actual && !strcmp(expected,actual));xrtFree(actual);xrtFree(expected);
  xlsIrTableImageDestroy(image);xlsIrNodeImageDestroy(ni);
 }
 xrtAtomic32Store(&tableOracleActive,1,XMEMORY_RELEASE);int okay=corpus(path) && regionCorpus(path);xrtAtomic32Store(&tableOracleActive,0,XMEMORY_RELEASE);
 CHECK(okay && !xrtAtomic32Load(&tableOracleErrors,XMEMORY_RELAXED));
 CHECK(xrtAtomic32Load(&tableOracleRuns,XMEMORY_RELAXED)>20);xrtFree(path);return 1;
}
#ifdef XLS_IR_TABLE_GRAPH_LOWERED_TEST
static int tableNativePersistence(void)
{
 const char* code="class Profile extends object\n derive value\n string name @json(\"json_name\") @xlon(\"xlon_name\") = \"\"\n int count = 0\nend\n"
  "func encode(Profile item) as string\n return json.stringify(item)\nend\n"
  "func decode(string text) as Profile\n return json.parse::<Profile>(text)\nend\n"
  "func encodeX(Profile item) as string\n return xlon.stringify(item)\nend\n"
  "func decodeX(string text) as Profile\n return xlon.parse::<Profile>(text)\nend\n"
  "func main(...) as int\n Profile item = Profile()\n item = decode(encode(item))\n item = decodeX(encodeX(item))\n return item.count\nend\n";
 char* path=xrtPathAbs("ir-table-persistence.xl");CHECK(path);
 xls_semantic_check* source=checked(path,code);CHECK(source && xlsSemanticCheckInfo(source).accepted);
 XL6_IrModule module;xl6IrModuleInit(&module);XL6_DiagList diags;xl6DiagInit(&diags);
 CHECK(xl6IrBuildModuleFromAst(&module,xlsSemanticCheckAst(source),&diags) && !diags.count);
 CHECK(xl6IrLowerModule(&module,&diags) && !diags.count);
 if(!module.persistenceProjectFieldCount || !module.persistenceRestoreFieldCount || !module.persistenceRestoreClaimCount){
  fprintf(stderr,"lowered persistence counts: project=%zu restore=%zu claims=%zu functions=%zu instructions=%zu classes=%zu entry=%u\n",
   module.persistenceProjectFieldCount,module.persistenceRestoreFieldCount,module.persistenceRestoreClaimCount,module.functionCount,module.instructionCount,module.classLayoutCount,(unsigned)module.abi.entryKind);
  char* detail=tableRealDump(&module);if(detail){xrtFileWriteAtomic("ir-table-lowered-failure.json",(xbytesview){(const unsigned char*)detail,strlen(detail)});xrtFree(detail);}
 }
 CHECK(module.persistenceProjectFieldCount && module.persistenceRestoreFieldCount && module.persistenceRestoreClaimCount);
 char* expected=tableRealDump(&module);CHECK(expected);xls_ir_graph_status status;
 xls_ir_table_graph* g=xlsIrTablesCapture(&module,SIZE_MAX,NULL,NULL,&status);CHECK(g);
 xls_ir_table_image* image=xlsIrTablesMaterialize(g,module.nodes,module.nodeCount,module.children,module.childCount,SIZE_MAX,NULL,NULL,&status);CHECK(image);xlsIrTablesRelease(g);
 /* Transfer only node ownership;retire every original non-node table. */
 XL6_IrModule nodeOwner;xl6IrModuleInit(&nodeOwner);
 nodeOwner.nodes=module.nodes;nodeOwner.nodeCount=module.nodeCount;nodeOwner.nodeCapacity=module.nodeCapacity;
 nodeOwner.children=module.children;nodeOwner.childCount=module.childCount;nodeOwner.childCapacity=module.childCapacity;
 module.nodes=NULL;module.nodeCount=module.nodeCapacity=0;module.children=NULL;module.childCount=module.childCapacity=0;
 xl6IrModuleFree(&module);xlsSemanticCheckDestroy(source);
 CHECK(xl6IrVerifyModule(xlsIrTableImageModule(image),&diags) && !diags.count);
 char* actual=tableRealDump(xlsIrTableImageModule(image));CHECK(actual && !strcmp(expected,actual));
 xrtFree(expected);xrtFree(actual);xlsIrTableImageDestroy(image);xl6IrModuleFree(&nodeOwner);xl6DiagFree(&diags);xrtFree(path);return 1;
}
#endif
typedef struct table_reader {xls_ir_table_unit* unit;size_t table;const xstrbuf* expected;int okay;} table_reader;
static int32 tableReadUnit(void* opaque)
{table_reader* r=opaque;xstrbuf value={0};r->okay=tableUnitBook(&value,r->table,r->unit) && tableBooksEqual(&value,r->expected);xrtStrBufFree(&value);xlsIrTableUnitRelease(r->unit);return r->okay ? 0 : 1;}
static int tableConcurrent(void)
{
 table_fixture fixture={0};CHECK(tableFixtureCreate(&fixture));xls_ir_graph_status status;
 xls_ir_table_graph* g=xlsIrTablesCapture(&fixture.module,SIZE_MAX,NULL,NULL,&status);CHECK(g);
 size_t table=tableFind(g,"types.items");xls_ir_table_unit* unit=xlsIrTablesPinUnit(g,table,0);CHECK(unit);
 xstrbuf expected={0};CHECK(tableUnitBook(&expected,table,unit));tableFixtureFree(&fixture);xlsIrTablesRelease(g);
 table_reader readers[4];xthread* threads[4];
 for(size_t i=0;i<4;++i){readers[i]=(table_reader){xlsIrTableUnitRetain(unit),table,&expected,0};threads[i]=xrtThreadCreate(tableReadUnit,&readers[i],0);CHECK(threads[i]);}
 xlsIrTableUnitRelease(unit);
 for(size_t i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}
 xrtStrBufFree(&expected);return 1;
}
static int tableGenerations(void)
{
 table_fixture fixture={0};CHECK(tableFixtureCreate(&fixture));xls_ir_table_unit* held[32];size_t table=0,bytes=0;
 xstrbuf expected={0};CHECK(book_XL6_IrType(&expected,fixture.module.types.items));
 for(size_t i=0;i<32;++i){
  xls_ir_graph_status status;faultActive=1;faultAt=faultCalls=denied=0;
  xls_ir_table_graph* g=xlsIrTablesCapture(&fixture.module,SIZE_MAX,NULL,NULL,&status);faultActive=0;CHECK(g);
  table=tableFind(g,"types.items");held[i]=xlsIrTablesPinUnit(g,table,0);CHECK(held[i]);
  bytes+=xlsIrTableUnitBytes(held[i]);xlsIrTablesRelease(g);CHECK(live==2*(i+1) && liveBytes==bytes && !overflow);
  if(i)CHECK(xlsIrTableUnitProducer(held[i])!=xlsIrTableUnitProducer(held[i-1]));
 }
 tableFixtureFree(&fixture);
 for(size_t i=32;i--;){xstrbuf actual={0};CHECK(tableUnitBook(&actual,table,held[i]) && tableBooksEqual(&expected,&actual));xrtStrBufFree(&actual);xlsIrTableUnitRelease(held[i]);}
 xrtStrBufFree(&expected);CHECK(!live && !liveBytes);return 1;
}
int main(void)
{
 xrtAtomic32Init(&checks,0);xrtAtomic32Init(&tableOracleRuns,0);xrtAtomic32Init(&tableOracleErrors,0);xrtAtomic32Init(&tableOracleActive,0);
 if(!tableSynthetic(0) || !tableSynthetic(1) || !tableFailures() || !tableNativeRetirement() || !tableConcurrent() || !tableGenerations())return 1;
#ifdef XLS_IR_TABLE_GRAPH_LOWERED_TEST
 if(!tableNativePersistence())return 1;
#endif
 printf("analysis_ir_table_graph_test passed: %u checks; %u complete native graph clones;107tables,70pointer fields,exact records/payloads,aliases,retirement,4readers,OOM,cancellation\n",
  xrtAtomic32Load(&checks,XMEMORY_RELAXED),xrtAtomic32Load(&tableOracleRuns,XMEMORY_RELAXED));return 0;
}
