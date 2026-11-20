#define main oldIrMemoMain
#include "analysis_ir_memo_test.c"
#undef main
#include "analysis/ir_table_graph.h"
#include "analysis/ir_node_link.h"
#include "analysis/ir_type_link.h"
#define XLS_IR_UNIT_CAPTURE_TEST 1

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

typedef struct capture_stop {size_t at,calls;} capture_stop;
static int captureCancelled(void* opaque){capture_stop* s=opaque;return ++s->calls==s->at;}
static int nodeBook(xstrbuf* out,const xls_ir_node_unit* unit)
{
 for(size_t i=0;i<xlsIrNodeUnitCount(unit);++i){const XL6_IrNode* n=xlsIrNodeUnitNode(unit,i);XL6_IrNode scalar;memcpy(&scalar,n,sizeof(scalar));scalar.text=NULL;scalar.literalData=NULL;
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
  scalar.compiledPackageLink=NULL;
#endif
  XL6_IrNodeId id=xlsIrNodeUnitNativeId(unit,i);size_t count;const XL6_IrNodeId* children=xlsIrNodeUnitChildren(unit,i,&count);
  CHECK(tableAppend(out,&id,sizeof(id)) && tableAppend(out,&scalar,sizeof(scalar)) && tableAppendText(out,n->text));
  unsigned char present=n->literalData!=NULL;CHECK(tableAppend(out,&present,1) && (!present || tableAppend(out,n->literalData,n->literalSize+1)));
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
  CHECK(tableAppendText(out,n->compiledPackageLink));
#endif
  CHECK(tableAppend(out,&count,sizeof(count)) && tableAppend(out,children,count*sizeof(*children)));
 }return 1;
}
static int captureTables(void)
{
 table_fixture fixture={0};CHECK(tableFixtureCreate(&fixture));xls_ir_graph_status status;
 size_t tables=xlsIrTableSchemaCount();
#ifdef XL6_FOREIGN_DATA_SNAPSHOT_RESULT
 CHECK(tables==108 && xlsIrTableSchemaFind("foreignDataSnapshots")!=SIZE_MAX);
#else
 CHECK(tables==107);
#endif
 xls_ir_table_graph* oracle=xlsIrTablesCapture(&fixture.module,SIZE_MAX,NULL,NULL,&status);CHECK(oracle && xlsIrTablesCount(oracle)==tables);
 xstrbuf sourceBefore={0},sourceAfter={0};CHECK(tableModuleBook(&sourceBefore,&fixture.module));
 CHECK(xlsIrTableSchemaFind(NULL)==SIZE_MAX && xlsIrTableSchemaFind("not-a-table")==SIZE_MAX && !xlsIrTableSchemaName(tables) && !xlsIrTableSchemaRowBytes(tables));
 xls_ir_table_unit* anchor=NULL,*keep=NULL;xstrbuf expectedKeep={0};size_t keepTable=0;
 for(size_t t=0;t<tables;++t){size_t count,width;int scalar;const unsigned char* rows=tableFixtureRows(&fixture.module,t,&count,&width,&scalar);
  CHECK(rows && count && xlsIrTableSchemaRowBytes(t)==width && xlsIrTableSchemaFind(xlsIrTableSchemaName(t))==t && tableFind(oracle,xlsIrTableSchemaName(t))==t && !strcmp(xlsIrTableSchemaName(t),xlsIrTablesName(oracle,t)));
  xls_ir_table_unit* original=xlsIrTablesPinUnit(oracle,t,0);CHECK(original);xstrbuf a={0},b={0};CHECK(tableUnitBook(&a,t,original));
  xls_ir_table_capture_input input={t,scalar ? 0 : (size_t)UINT32_MAX-1,scalar ? count : 1,rows,&fixture.module};size_t peak=0;
  faultActive=1;faultAt=faultCalls=denied=0;xls_ir_table_unit* unit=xlsIrTableUnitCapture(&input,anchor,SIZE_MAX,NULL,NULL,&status,&peak);faultActive=0;
  CHECK(unit && status==XLS_IR_GRAPH_OK && peak==xlsIrTableUnitBytes(unit) && xlsIrTableUnitFirstRow(unit)==input.first && xlsIrTableUnitRows(unit)==input.count && !overflow);
  CHECK(faultCalls==(anchor ? 1u : 2u));CHECK(tableUnitBook(&b,t,unit) && tableBooksEqual(&a,&b));
  if(anchor)CHECK(xlsIrTableUnitProducer(unit)==xlsIrTableUnitProducer(anchor));
  else anchor=xlsIrTableUnitRetain(unit);
  if(!strcmp(xlsIrTableSchemaName(t),"persistenceRestoreClaims")){CHECK(xlsIrTableUnitKeyAlias(unit)==3);keep=xlsIrTableUnitRetain(unit);keepTable=t;CHECK(tableUnitBook(&expectedKeep,t,unit));}
  if(!strcmp(xlsIrTableSchemaName(t),"persistenceProjectFields"))CHECK(xlsIrTableUnitKeyAlias(unit)==1);
  if(!strcmp(xlsIrTableSchemaName(t),"persistenceRestoreFields"))CHECK(xlsIrTableUnitKeyAlias(unit)==2);
  xls_ir_table_unit* exact=xlsIrTableUnitCapture(&input,anchor,peak,NULL,NULL,&status,NULL);CHECK(exact);xlsIrTableUnitRelease(exact);
  CHECK(!xlsIrTableUnitCapture(&input,anchor,peak-1,NULL,NULL,&status,NULL) && status==XLS_IR_GRAPH_BUDGET);
  xrtStrBufFree(&a);xrtStrBufFree(&b);xlsIrTableUnitRelease(original);xlsIrTableUnitRelease(unit);
 }
 CHECK(tableModuleBook(&sourceAfter,&fixture.module) && tableBooksEqual(&sourceBefore,&sourceAfter));xrtStrBufFree(&sourceBefore);xrtStrBufFree(&sourceAfter);
 CHECK(keep);xlsIrTableUnitRelease(anchor);xlsIrTablesRelease(oracle);tableFixtureFree(&fixture);
 CHECK(live==2 && liveBytes==xlsIrTableUnitBytes(keep));xstrbuf actual={0};CHECK(tableUnitBook(&actual,keepTable,keep) && tableBooksEqual(&actual,&expectedKeep));
 xrtStrBufFree(&actual);xrtStrBufFree(&expectedKeep);xlsIrTableUnitRelease(keep);CHECK(!live && !liveBytes);
 return 1;
}
static int tableCaptureFailures(void)
{
 XL6_IrType value={.name="type 中文😀"};XL6_IrTypeId params[]={3,0,7};value.params=params;value.paramCount=3;
 xls_ir_table_capture_input input={xlsIrTableSchemaFind("types.items"),19,1,&value,(const XL6_IrModule*)1};xls_ir_graph_status status;size_t peak;
 faultActive=1;faultAt=faultCalls=denied=peakBytes=0;xls_ir_table_unit* unit=xlsIrTableUnitCapture(&input,NULL,SIZE_MAX,NULL,NULL,&status,&peak);faultActive=0;
 CHECK(unit && faultCalls==2 && live==2 && liveBytes==peak && peakBytes==peak);xlsIrTableUnitRelease(unit);CHECK(!live);
 for(size_t i=1;i<=2;++i){faultActive=1;faultAt=i;faultCalls=denied=0;peak=777;unit=xlsIrTableUnitCapture(&input,NULL,SIZE_MAX,NULL,NULL,&status,&peak);faultActive=0;CHECK(!unit && status==XLS_IR_GRAPH_MEMORY && !peak && denied==1 && !live);}
 capture_stop stop={0};unit=xlsIrTableUnitCapture(&input,NULL,SIZE_MAX,captureCancelled,&stop,&status,NULL);CHECK(unit);size_t calls=stop.calls;xlsIrTableUnitRelease(unit);
 for(size_t i=1;i<=calls;++i){stop=(capture_stop){.at=i};faultActive=1;faultAt=faultCalls=denied=0;peak=777;unit=xlsIrTableUnitCapture(&input,NULL,SIZE_MAX,captureCancelled,&stop,&status,&peak);faultActive=0;CHECK(!unit && status==XLS_IR_GRAPH_CANCELLED && !peak && !live);}
 faultActive=1;faultAt=faultCalls=denied=0;unit=xlsIrTableUnitCapture(&input,NULL,0,NULL,NULL,&status,&peak);faultActive=0;CHECK(!unit && status==XLS_IR_GRAPH_BUDGET && !faultCalls && !live && !peak);
 xls_ir_table_capture_input bad=input;bad.count=2;CHECK(!xlsIrTableUnitCapture(&bad,NULL,SIZE_MAX,NULL,NULL,&status,NULL) && status==XLS_IR_GRAPH_INVALID);
 bad=input;bad.first=SIZE_MAX;CHECK(!xlsIrTableUnitCapture(&bad,NULL,SIZE_MAX,NULL,NULL,&status,NULL) && status==XLS_IR_GRAPH_INVALID);
 bad=input;bad.table=xlsIrTableSchemaCount();CHECK(!xlsIrTableUnitCapture(&bad,NULL,SIZE_MAX,NULL,NULL,&status,NULL) && status==XLS_IR_GRAPH_INVALID);
 bad=(xls_ir_table_capture_input){xlsIrTableSchemaFind("symbolNodes"),0,0,NULL,NULL};unit=xlsIrTableUnitCapture(&bad,NULL,SIZE_MAX,NULL,NULL,&status,NULL);CHECK(unit && !xlsIrTableUnitRows(unit) && !xlsIrTableUnitRow(unit,0));xlsIrTableUnitRelease(unit);
 bad.first=1;CHECK(!xlsIrTableUnitCapture(&bad,NULL,SIZE_MAX,NULL,NULL,&status,NULL) && status==XLS_IR_GRAPH_INVALID);bad.first=0;bad.count=SIZE_MAX;bad.rows=params;
 CHECK(!xlsIrTableUnitCapture(&bad,NULL,SIZE_MAX,NULL,NULL,&status,NULL) && status==XLS_IR_GRAPH_BUDGET);
 XL6_IrClassField field={.name="canonical",.jsonPersistenceKeyLiteral="json-key",.xlonPersistenceKeyLiteral="xlon-key"};XL6_IrModule aliases={.classFields=&field,.classFieldCount=1};
 XL6_IrPersistenceProjectField row={.classField=0,.targetKey=field.name};input=(xls_ir_table_capture_input){xlsIrTableSchemaFind("persistenceProjectFields"),0,1,&row,&aliases};
 unit=xlsIrTableUnitCapture(&input,NULL,SIZE_MAX,NULL,NULL,&status,NULL);CHECK(unit && xlsIrTableUnitKeyAlias(unit)==1 && ((const XL6_IrPersistenceProjectField*)xlsIrTableUnitRow(unit,0))->targetKey!=field.name);xlsIrTableUnitRelease(unit);
 input.aliases=NULL;CHECK(!xlsIrTableUnitCapture(&input,NULL,SIZE_MAX,NULL,NULL,&status,NULL) && status==XLS_IR_GRAPH_INVALID);input.aliases=&aliases;
 char* duplicate=xrtStrDup(field.name);CHECK(duplicate);row.targetKey=duplicate;CHECK(!xlsIrTableUnitCapture(&input,NULL,SIZE_MAX,NULL,NULL,&status,NULL) && status==XLS_IR_GRAPH_INVALID);xrtFree(duplicate);
 row.targetKey=NULL;input.aliases=NULL;unit=xlsIrTableUnitCapture(&input,NULL,SIZE_MAX,NULL,NULL,&status,NULL);CHECK(unit && !xlsIrTableUnitKeyAlias(unit));xlsIrTableUnitRelease(unit);
 printf("table unit capture: 2 allocation sites, %zu cancellation points, %zu table kinds\n",calls,xlsIrTableSchemaCount());return 1;
}
typedef struct node_fixture {
 XL6_IrNode values[3];XL6_IrNodeId children[2];unsigned char literal[4];
 xls_ir_node_capture_entry entries[3];xls_ir_node_capture_input input;
} node_fixture;
static void nodeFixture(node_fixture* f)
{
 memset(f,0,sizeof(*f));f->literal[0]=0xff;f->literal[1]=0;f->literal[2]=7;f->literal[3]=0;
 XL6_IrNodeId root=UINT32_MAX-2;f->children[0]=3;f->children[1]=UINT32_MAX-1;
 for(size_t i=0;i<3;++i){XL6_IrNode* n=&f->values[i];n->kind=i==1 ? XL6_IR_NODE_FUNC_DECL : XL6_IR_NODE_LITERAL;n->parent=i==1 ? 7 : root;n->firstChild=999;
  n->text="node 中文😀";n->literalData=f->literal;n->literalSize=3;n->unionField=SIZE_MAX;n->span=(XL6_IrSpan){(int)i+1,3};
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
  n->compiledPackageLink="package/owned.xlp";
#endif
 }
 f->values[1].childCount=2;f->entries[0]=(xls_ir_node_capture_entry){3,&f->values[0],NULL};f->entries[1]=(xls_ir_node_capture_entry){root,&f->values[1],f->children};f->entries[2]=(xls_ir_node_capture_entry){UINT32_MAX-1,&f->values[2],NULL};
 f->input=(xls_ir_node_capture_input){f->entries,3,UINT32_MAX,root,7};
}
static int nodeCaptureFailures(void)
{
 node_fixture f;nodeFixture(&f);xls_ir_graph_status status;size_t peak;
 faultActive=1;faultAt=faultCalls=denied=peakBytes=0;xls_ir_node_unit* unit=xlsIrNodeUnitCapture(&f.input,NULL,8192,NULL,NULL,&status,&peak);faultActive=0;
 CHECK(unit && status==XLS_IR_GRAPH_OK && faultCalls==3 && live==2 && liveBytes==xlsIrNodeUnitBytes(unit) && peakBytes==peak && peak<8192);
 CHECK(xlsIrNodeUnitRoot(unit)==f.input.root && xlsIrNodeUnitCount(unit)==3 && xlsIrNodeUnitNativeId(unit,0)==3 && xlsIrNodeUnitNode(unit,1)->firstChild==999);
 xstrbuf expected={0};CHECK(nodeBook(&expected,unit));xlsIrNodeUnitRelease(unit);CHECK(!live);
 unit=xlsIrNodeUnitCapture(&f.input,NULL,peak,NULL,NULL,&status,NULL);CHECK(unit);xlsIrNodeUnitRelease(unit);
 CHECK(!xlsIrNodeUnitCapture(&f.input,NULL,peak-1,NULL,NULL,&status,NULL) && status==XLS_IR_GRAPH_BUDGET);
 for(size_t i=1;i<=3;++i){faultActive=1;faultAt=i;faultCalls=denied=0;peak=777;unit=xlsIrNodeUnitCapture(&f.input,NULL,8192,NULL,NULL,&status,&peak);faultActive=0;CHECK(!unit && status==XLS_IR_GRAPH_MEMORY && !peak && denied==1 && !live);}
 capture_stop stop={0};unit=xlsIrNodeUnitCapture(&f.input,NULL,8192,captureCancelled,&stop,&status,NULL);CHECK(unit);size_t calls=stop.calls;xlsIrNodeUnitRelease(unit);
 for(size_t i=1;i<=calls;++i){stop=(capture_stop){.at=i};faultActive=1;faultAt=faultCalls=denied=0;peak=777;unit=xlsIrNodeUnitCapture(&f.input,NULL,8192,captureCancelled,&stop,&status,&peak);faultActive=0;CHECK(!unit && status==XLS_IR_GRAPH_CANCELLED && !peak && !live);}
 faultActive=1;faultAt=faultCalls=denied=0;unit=xlsIrNodeUnitCapture(&f.input,NULL,0,NULL,NULL,&status,&peak);faultActive=0;CHECK(!unit && status==XLS_IR_GRAPH_BUDGET && !faultCalls && !live && !peak);
#define BAD_NODE() CHECK(!xlsIrNodeUnitCapture(&f.input,NULL,8192,NULL,NULL,&status,NULL) && status==XLS_IR_GRAPH_INVALID)
 f.children[1]=3;BAD_NODE();nodeFixture(&f);f.children[0]=4;BAD_NODE();nodeFixture(&f);f.values[0].parent=7;BAD_NODE();nodeFixture(&f);
 f.entries[1].id=3;BAD_NODE();nodeFixture(&f);f.input.root=4;BAD_NODE();nodeFixture(&f);f.values[1].parent=0;BAD_NODE();nodeFixture(&f);
 f.entries[0].node=NULL;BAD_NODE();nodeFixture(&f);f.entries[1].children=NULL;BAD_NODE();nodeFixture(&f);
#undef BAD_NODE
 faultActive=1;faultAt=faultCalls=denied=0;xls_ir_node_unit* anchor=xlsIrNodeUnitCapture(&f.input,NULL,8192,NULL,NULL,&status,NULL);CHECK(anchor);
 unit=xlsIrNodeUnitCapture(&f.input,anchor,8192,NULL,NULL,&status,NULL);faultActive=0;CHECK(unit && xlsIrNodeUnitProducer(unit)==xlsIrNodeUnitProducer(anchor));xlsIrNodeUnitRelease(anchor);
 CHECK(live==2 && liveBytes==xlsIrNodeUnitBytes(unit));memset(&f,0,sizeof(f));xstrbuf actual={0};CHECK(nodeBook(&actual,unit) && tableBooksEqual(&expected,&actual));xrtStrBufFree(&actual);xrtStrBufFree(&expected);xlsIrNodeUnitRelease(unit);CHECK(!live && !liveBytes);
 printf("node unit capture:3allocation sites,%zu cancellation points;3nodes in UINT32_MAX namespace fit8192bytes\n",calls);return 1;
}
static int rootCapture(void)
{
 XL6_IrNode node={.kind=XL6_IR_NODE_MODULE};xls_ir_node_capture_entry entry={1,&node,NULL};xls_ir_node_capture_input input={&entry,1,UINT32_MAX,1,1};xls_ir_graph_status status;
 xls_ir_node_unit* unit=xlsIrNodeUnitCapture(&input,NULL,SIZE_MAX,NULL,NULL,&status,NULL);CHECK(unit && xlsIrNodeUnitCount(unit)==1);xlsIrNodeUnitRelease(unit);
 XL6_IrNodeId children[257],beforeChildren[257];entry.children=children;
 for(size_t n=2;n<=257;++n){node.childCount=n;for(size_t j=0;j<n;++j)children[j]=(XL6_IrNodeId)(n-j+1);
  memcpy(beforeChildren,children,n*sizeof(*children));unit=xlsIrNodeUnitCapture(&input,NULL,SIZE_MAX,NULL,NULL,&status,NULL);CHECK(unit);size_t count;const XL6_IrNodeId* ids=xlsIrNodeUnitChildren(unit,0,&count);CHECK(count==n && !memcmp(ids,beforeChildren,n*sizeof(*ids)) && !memcmp(children,beforeChildren,n*sizeof(*children)));xlsIrNodeUnitRelease(unit);
  children[n-1]=children[0];CHECK(!xlsIrNodeUnitCapture(&input,NULL,SIZE_MAX,NULL,NULL,&status,NULL) && status==XLS_IR_GRAPH_INVALID);}
 node.childCount=257;for(size_t j=0;j<257;++j)children[j]=(XL6_IrNodeId)(258-j);
 capture_stop stop={0};unit=xlsIrNodeUnitCapture(&input,NULL,SIZE_MAX,captureCancelled,&stop,&status,NULL);CHECK(unit);size_t calls=stop.calls;xlsIrNodeUnitRelease(unit);
 for(size_t i=1;i<=calls;++i){stop=(capture_stop){.at=i};faultActive=1;faultAt=faultCalls=denied=0;unit=xlsIrNodeUnitCapture(&input,NULL,SIZE_MAX,captureCancelled,&stop,&status,NULL);faultActive=0;CHECK(!unit && status==XLS_IR_GRAPH_CANCELLED && !live);}
 XL6_IrNode values[5]={0};xls_ir_node_capture_entry entries[5];XL6_IrNodeId links[4]={3,6,5,4};
 for(size_t i=0;i<5;++i)entries[i]=(xls_ir_node_capture_entry){(XL6_IrNodeId)i+2,&values[i],NULL};
 values[0].parent=1;values[0].childCount=2;entries[0].children=links;values[1].parent=2;values[4].parent=2;
 values[2].parent=5;values[2].childCount=1;entries[2].children=links+2;values[3].parent=4;values[3].childCount=1;entries[3].children=links+3;
 input=(xls_ir_node_capture_input){entries,5,6,2,1};CHECK(!xlsIrNodeUnitCapture(&input,NULL,SIZE_MAX,NULL,NULL,&status,NULL) && status==XLS_IR_GRAPH_INVALID);return 1;
}

static char* captureDump(const XL6_IrModule*);
void __real_xl6IrDumpModuleJson(XL6_Output*,const XL6_IrModule*);
static xatomic32 oracleRuns,oracleErrors,oracleActive;static _Thread_local int oracleRecursing;
static int captureIdentity(void* unused,xls_ir_reference_domain domain,uint64_t old,uint64_t* result){(void)unused;(void)domain;*result=old;return 1;}
static int captureTypeIdentity(void* unused,xls_ir_type_domain domain,uint64_t old,uint64_t* result){(void)unused;(void)domain;*result=old;return 1;}
typedef struct capture_view {xls_ir_node_link* nodes;xls_ir_type_link* types;xls_ir_table_image* tables;XL6_IrModule module;} capture_view;
static void captureViewFree(capture_view* view){xlsIrTableImageDestroy(view->tables);xlsIrTypeLinkDestroy(view->types);xlsIrNodeLinkDestroy(view->nodes);memset(view,0,sizeof(*view));}
static int captureView(const XL6_IrModule* source,capture_view* view)
{
 memset(view,0,sizeof(*view));xls_ir_graph_status status;
 /* The existing graph is only an independent partition/value oracle. The
  * new factory receives selected entries,never the graph or full module. */
 xls_ir_node_graph* graph=xlsIrNodeGraphCapture(source,SIZE_MAX,NULL,NULL,&status);CHECK(graph);size_t units=xlsIrNodeGraphUnitCount(graph);
 xls_ir_node_binding* bindings=xrtCalloc(units,sizeof(*bindings));CHECK(bindings);xls_ir_node_unit* anchor=NULL;
 for(size_t i=0;i<units;++i){xls_ir_node_unit* old=xlsIrNodeGraphPinUnit(graph,i);CHECK(old);size_t n=xlsIrNodeUnitCount(old);xls_ir_node_capture_entry* entries=xrtCalloc(n,sizeof(*entries));CHECK(entries);
  for(size_t j=0;j<n;++j){size_t ignored;entries[j]=(xls_ir_node_capture_entry){xlsIrNodeUnitNativeId(old,j),xlsIrNodeUnitNode(old,j),xlsIrNodeUnitChildren(old,j,&ignored)};}
  xls_ir_node_capture_input input={entries,n,source->nodeCount,xlsIrNodeUnitRoot(old),source->root};xls_ir_node_unit* unit=xlsIrNodeUnitCapture(&input,anchor,SIZE_MAX,NULL,NULL,&status,NULL);CHECK(unit);
  xstrbuf a={0},b={0};CHECK(nodeBook(&a,old) && nodeBook(&b,unit) && tableBooksEqual(&a,&b));xrtStrBufFree(&a);xrtStrBufFree(&b);
  for(size_t j=0;j<n;++j){const XL6_IrNode* before=xlsIrNodeUnitNode(old,j);const XL6_IrNode* after=xlsIrNodeUnitNode(unit,j);
   CHECK(!before->text || before->text!=after->text);CHECK(!before->literalData || before->literalData!=after->literalData);
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
   CHECK(!before->compiledPackageLink || before->compiledPackageLink!=after->compiledPackageLink);
#endif
  }
  if(!anchor)anchor=unit;else CHECK(xlsIrNodeUnitProducer(unit)==xlsIrNodeUnitProducer(anchor));
  bindings[i]=(xls_ir_node_binding){unit,xlsIrNodeUnitProducer(unit),captureIdentity,NULL};xrtFree(entries);xlsIrNodeUnitRelease(old);
 }
 xlsIrNodeGraphRelease(graph);xls_ir_node_layout layout={source->nodes,source->nodeCount,source->children,source->childCount,source->root,source->types.count,source->aggregateFieldCount};
 view->nodes=xlsIrNodeLink(&layout,bindings,units,SIZE_MAX,NULL,NULL,&status,NULL);CHECK(view->nodes);
 for(size_t i=0;i<units;++i)xlsIrNodeUnitRelease(bindings[i].unit);
 xrtFree(bindings);
 size_t count=source->types.count;xls_ir_type_binding* types=xrtCalloc(count ? count : 1,sizeof(*types));CHECK(types);xls_ir_table_unit* typeAnchor=NULL;
 for(size_t i=0;i<count;++i){xls_ir_table_capture_input input={xlsIrTableSchemaFind("types.items"),i,1,&source->types.items[i],NULL};xls_ir_table_unit* unit=xlsIrTableUnitCapture(&input,typeAnchor,SIZE_MAX,NULL,NULL,&status,NULL);CHECK(unit);
  if(!typeAnchor)typeAnchor=unit;else CHECK(xlsIrTableUnitProducer(unit)==xlsIrTableUnitProducer(typeAnchor));
  types[i]=(xls_ir_type_binding){unit,xlsIrTableUnitProducer(unit),captureTypeIdentity,NULL};}
 view->types=xlsIrTypeLink(&source->types,types,count,SIZE_MAX,NULL,NULL,&status,NULL);CHECK(view->types);
 for(size_t i=0;i<count;++i)xlsIrTableUnitRelease(types[i].unit);
 xrtFree(types);
 xls_ir_table_graph* tables=xlsIrTablesCapture(source,SIZE_MAX,NULL,NULL,&status);CHECK(tables);
 view->tables=xlsIrTablesMaterialize(tables,xlsIrNodeLinkNodes(view->nodes,NULL),source->nodeCount,xlsIrNodeLinkChildren(view->nodes,NULL),source->childCount,SIZE_MAX,NULL,NULL,&status);xlsIrTablesRelease(tables);CHECK(view->tables);
 view->module=*xlsIrTableImageModule(view->tables);view->module.types=*xlsIrTypeLinkPool(view->types);return 1;
}
static int verifyCapture(const XL6_IrModule* module,const char* expected)
{
 XL6_DiagList diags;xl6DiagInit(&diags);CHECK(xl6IrVerifyModule(module,&diags) && !diags.count);xl6DiagFree(&diags);char* actual=captureDump(module);CHECK(actual && !strcmp(actual,expected));xrtFree(actual);return 1;
}
static int captureOracle(const XL6_IrModule* source)
{capture_view view;CHECK(captureView(source,&view));char* expected=captureDump(source);CHECK(expected && verifyCapture(&view.module,expected));xrtFree(expected);captureViewFree(&view);return 1;}
void __wrap_xl6IrDumpModuleJson(XL6_Output* out,const XL6_IrModule* module)
{
 if(xrtAtomic32Load(&oracleActive,XMEMORY_ACQUIRE) && !oracleRecursing){oracleRecursing=1;xrtAtomic32FetchAdd(&oracleRuns,1,XMEMORY_RELAXED);if(!captureOracle(module))xrtAtomic32FetchAdd(&oracleErrors,1,XMEMORY_RELAXED);oracleRecursing=0;}
 __real_xl6IrDumpModuleJson(out,module);
}
static char* captureDump(const XL6_IrModule* module)
{XL6_Output out;if(!xl6OutputOpenMemory(&out))return NULL;__real_xl6IrDumpModuleJson(&out,module);char* value=xl6OutputHasFailed(&out) ? NULL : xl6OutputTakeMemoryText(&out,NULL);xl6OutputDiscard(&out);return value;}
static int captureNative(void)
{
 char* path=xrtPathAbs("ir-unit-capture-中文.xl");CHECK(path);
 for(unsigned edit=0;edit<8;++edit){char* text=body(edit);CHECK(text);xls_semantic_check* model=checked(path,text);CHECK(model && xlsSemanticCheckInfo(model).accepted);const XL6_IrModule* source=xlsSemanticCheckIr(model);CHECK(source);
  char* expected=captureDump(source);capture_view view;CHECK(expected && captureView(source,&view));xlsSemanticCheckDestroy(model);xrtFree(text);CHECK(verifyCapture(&view.module,expected));xrtFree(expected);captureViewFree(&view);}
 xrtAtomic32Store(&oracleActive,1,XMEMORY_RELEASE);int okay=corpus(path) && regionCorpus(path);xrtAtomic32Store(&oracleActive,0,XMEMORY_RELEASE);CHECK(okay && !xrtAtomic32Load(&oracleErrors,XMEMORY_RELAXED) && xrtAtomic32Load(&oracleRuns,XMEMORY_RELAXED)==134);xrtFree(path);return 1;
}
typedef struct capture_reader {const node_fixture* fixture;xls_ir_node_unit* anchor;const xstrbuf* expected;int okay;} capture_reader;
static int32 readCaptures(void* opaque)
{
 capture_reader* reader=opaque;reader->okay=1;
 for(size_t i=0;i<16;++i){xls_ir_graph_status status;xls_ir_node_unit* unit=xlsIrNodeUnitCapture(&reader->fixture->input,reader->anchor,8192,NULL,NULL,&status,NULL);xstrbuf book={0};
  int okay=unit && nodeBook(&book,unit) && tableBooksEqual(&book,reader->expected);xrtStrBufFree(&book);xlsIrNodeUnitRelease(unit);if(!okay){reader->okay=0;break;}}
 return reader->okay ? 0 : 1;
}
static int captureLifetimes(void)
{
 node_fixture fixture;nodeFixture(&fixture);xls_ir_graph_status status;xls_ir_node_unit* anchor=xlsIrNodeUnitCapture(&fixture.input,NULL,8192,NULL,NULL,&status,NULL);CHECK(anchor);xstrbuf expected={0};CHECK(nodeBook(&expected,anchor));
 capture_reader readers[4];xthread* threads[4];for(size_t i=0;i<4;++i){readers[i]=(capture_reader){&fixture,anchor,&expected,0};threads[i]=xrtThreadCreate(readCaptures,&readers[i],0);CHECK(threads[i]);}
 for(size_t i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}xlsIrNodeUnitRelease(anchor);
 xls_ir_node_unit* units[32];for(size_t i=0;i<32;++i){faultActive=1;faultAt=faultCalls=denied=0;units[i]=xlsIrNodeUnitCapture(&fixture.input,NULL,8192,NULL,NULL,&status,NULL);faultActive=0;CHECK(units[i] && live==2*(i+1) && !overflow);
  if(i)CHECK(xlsIrNodeUnitProducer(units[i])!=xlsIrNodeUnitProducer(units[i-1]));}
 memset(&fixture,0,sizeof(fixture));for(size_t i=32;i--;){xstrbuf book={0};CHECK(nodeBook(&book,units[i]) && tableBooksEqual(&book,&expected));xrtStrBufFree(&book);xlsIrNodeUnitRelease(units[i]);}
 CHECK(!live && !liveBytes);xrtStrBufFree(&expected);return 1;
}
int main(void)
{
 xrtAtomic32Init(&checks,0);xrtAtomic32Init(&oracleRuns,0);xrtAtomic32Init(&oracleErrors,0);xrtAtomic32Init(&oracleActive,0);
 if(!captureTables() || !tableCaptureFailures() || !nodeCaptureFailures() || !rootCapture() || !captureNative() || !captureLifetimes())return 1;
 printf("analysis_ir_unit_capture_test passed: %u checks; %u native verifier/JSON comparisons; %zu table kinds, retirement, OOM, budgets, cancellation, sparse IDs\n",
  xrtAtomic32Load(&checks,XMEMORY_RELAXED),xrtAtomic32Load(&oracleRuns,XMEMORY_RELAXED),xlsIrTableSchemaCount());return 0;
}
