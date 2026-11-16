#define main oldIrMemoMain
#include "analysis_ir_memo_test.c"
#undef main
#include "analysis/ir_node_graph.h"

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

/* Canonical test book includes every native byte except pointer addresses,
 * plus payload presence/content and each actual child list. */
static int bookNode(xstrbuf* out,XL6_IrNodeId id,const XL6_IrNode* node,const XL6_IrNodeId* children,size_t count)
{
 XL6_IrNode scalar;memcpy(&scalar,node,sizeof(scalar));scalar.text=NULL;scalar.literalData=NULL;
 unsigned char present[]={node->text!=NULL,node->literalData!=NULL,
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
  node->compiledPackageLink!=NULL
#else
  0
#endif
 };
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
 scalar.compiledPackageLink=NULL;
#endif
#define APPEND(p,n) xrtStrBufAppend(out,(xstrview){(const char*)(p),(n)})
 int okay=APPEND(&id,sizeof(id)) && APPEND(&scalar,sizeof(scalar)) && APPEND(present,sizeof(present)) &&
  (!node->text || APPEND(node->text,strlen(node->text)+1)) &&
  (!node->literalData || APPEND(node->literalData,node->literalSize+1)) &&
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
  (!node->compiledPackageLink || APPEND(node->compiledPackageLink,strlen(node->compiledPackageLink)+1)) &&
#endif
  APPEND(&count,sizeof(count)) && (!count || APPEND(children,count*sizeof(*children)));
#undef APPEND
 return okay;
}
static int moduleBook(xstrbuf* out,const XL6_IrModule* module)
{
 for(size_t i=0;i<module->nodeCount;++i){const XL6_IrNode* n=&module->nodes[i];
  if(!bookNode(out,(XL6_IrNodeId)(i+1),n,n->childCount ? module->children+n->firstChild : NULL,n->childCount))return 0;}
 return 1;
}
static int graphBook(xstrbuf* out,const xls_ir_node_graph* graph)
{
 for(size_t i=0;i<xlsIrNodeGraphNodeCount(graph);++i){size_t count;XL6_IrNodeId id=(XL6_IrNodeId)(i+1);
  const XL6_IrNodeId* children=xlsIrNodeGraphChildren(graph,id,&count);
  if(!bookNode(out,id,xlsIrNodeGraphNode(graph,id),children,count))return 0;}
 return 1;
}
static int unitBook(xstrbuf* out,const xls_ir_node_unit* unit)
{
 for(size_t i=0;i<xlsIrNodeUnitCount(unit);++i){size_t count;const XL6_IrNodeId* children=xlsIrNodeUnitChildren(unit,i,&count);
  if(!bookNode(out,xlsIrNodeUnitNativeId(unit,i),xlsIrNodeUnitNode(unit,i),children,count))return 0;}
 return 1;
}
static int equalBook(const xstrbuf* a,const xstrbuf* b)
{return a->Size==b->Size && (!a->Size || !memcmp(a->Data,b->Data,a->Size));}
static int graphMatches(const XL6_IrModule* module,xls_ir_node_graph* graph)
{
 xstrbuf original={0},copy={0};CHECK(moduleBook(&original,module) && graphBook(&copy,graph));
 CHECK(equalBook(&original,&copy));CHECK(xlsIrNodeGraphRoot(graph)==module->root);
 CHECK(xlsIrNodeGraphNodeCount(graph)==module->nodeCount);
 xls_ir_graph_status status;xls_ir_node_image* image=xlsIrNodeGraphMaterialize(graph,SIZE_MAX,NULL,NULL,&status);
 CHECK(image && status==XLS_IR_GRAPH_OK);size_t count,children;
 XL6_IrModule view=*module;view.nodes=(XL6_IrNode*)xlsIrNodeImageNodes(image,&count);
 view.children=(XL6_IrNodeId*)xlsIrNodeImageChildren(image,&children);
 view.nodeCapacity=count;view.childCapacity=children;
 CHECK(count==module->nodeCount && children==module->childCount);
 CHECK(!children || !memcmp(view.children,module->children,children*sizeof(*view.children)));
 xstrbuf materialized={0};CHECK(moduleBook(&materialized,&view) && equalBook(&original,&materialized));
 CHECK(!xlsIrNodeGraphMaterialize(graph,xlsIrNodeImageBytes(image)-1,NULL,NULL,&status) && status==XLS_IR_GRAPH_BUDGET);
 xlsIrNodeImageDestroy(image);xrtStrBufFree(&original);xrtStrBufFree(&copy);xrtStrBufFree(&materialized);return 1;
}
static int synthetic(XL6_IrModule* module)
{
 *module=(XL6_IrModule){.ready=1,.stage=XL6_IR_STAGE_TYPED,.root=4,.nodeCount=7,.nodeCapacity=7,.childCount=12,.childCapacity=12};
 module->nodes=xrtCalloc(7,sizeof(*module->nodes));module->children=xrtCalloc(12,sizeof(*module->children));CHECK(module->nodes && module->children);
 const XL6_IrNodeId links[]={2,5,UINT32_MAX,7,1,4,0,3,99,6,88,4};memcpy(module->children,links,sizeof(links));
 for(size_t i=0;i<7;++i){
  XL6_IrNode* node=&module->nodes[i];memset(node,0xa5,sizeof(*node));
  node->kind=XL6_IR_NODE_LITERAL;node->sourceKind=XL6_AST_STRING;node->parent=0;node->firstChild=12;node->childCount=0;
  node->text=xrtStrDup("node 中文😀");node->literalData=xrtMalloc(4);node->literalSize=3;
  CHECK(node->text && node->literalData);unsigned char literal[]={0xff,0,(unsigned char)i,0};memcpy(node->literalData,literal,4);
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
  node->compiledPackageLink=xrtStrDup("package/模块.xlp");CHECK(node->compiledPackageLink);
#endif
 }
 module->nodes[3].kind=XL6_IR_NODE_MODULE;module->nodes[3].firstChild=3;module->nodes[3].childCount=2;
 module->nodes[6].kind=module->nodes[0].kind=XL6_IR_NODE_FUNC_DECL;
 module->nodes[6].parent=4;module->nodes[6].firstChild=0;module->nodes[6].childCount=2;
 module->nodes[1].parent=7;module->nodes[1].firstChild=9;module->nodes[1].childCount=1;
 module->nodes[5].parent=2;module->nodes[4].parent=7;
 module->nodes[0].parent=4;module->nodes[0].firstChild=7;module->nodes[0].childCount=1;module->nodes[2].parent=1;
 return 1;
}
static void syntheticFree(XL6_IrModule* module)
{
 for(size_t i=0;i<module->nodeCount;++i){xrtFree(module->nodes[i].text);xrtFree(module->nodes[i].literalData);
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
  xrtFree(module->nodes[i].compiledPackageLink);
#endif
 }
 xrtFree(module->nodes);xrtFree(module->children);*module=(XL6_IrModule){0};
}
typedef struct graph_stop {size_t calls,at;} graph_stop;
static int graphCancelled(void* opaque){graph_stop* s=opaque;return ++s->calls==s->at;}
typedef struct unit_reader {xls_ir_node_unit* unit;const xstrbuf* expected;int okay;} unit_reader;
static int32 readUnit(void* opaque)
{
 unit_reader* r=opaque;xstrbuf book={0};r->okay=unitBook(&book,r->unit) && equalBook(&book,r->expected);
 xrtStrBufFree(&book);xlsIrNodeUnitRelease(r->unit);return r->okay ? 0 : 1;
}
static int graphLifetime(void)
{
 XL6_IrModule source;CHECK(synthetic(&source));xls_ir_graph_status status;
 xls_ir_node_graph* graph=xlsIrNodeGraphCapture(&source,SIZE_MAX,NULL,NULL,&status);CHECK(graph && status==XLS_IR_GRAPH_OK);
 CHECK(graphMatches(&source,graph));CHECK(xlsIrNodeGraphUnitCount(graph)==3);
 CHECK(!xlsIrNodeGraphNode(graph,0) && !xlsIrNodeGraphNode(graph,8) && !xlsIrNodeGraphPinUnit(graph,3));
 xls_ir_node_unit *a=xlsIrNodeGraphPinUnit(graph,1),*b=xlsIrNodeGraphPinUnit(graph,2);CHECK(a && b);
 CHECK(xlsIrNodeUnitRoot(a)==7 && xlsIrNodeUnitCount(a)==4 && xlsIrNodeUnitRoot(b)==1 && xlsIrNodeUnitCount(b)==2);
 CHECK(xlsIrNodeUnitNativeId(a,0)==2 && xlsIrNodeUnitNativeId(a,3)==7);
 CHECK(xlsIrNodeUnitProducer(a)==xlsIrNodeUnitProducer(b));CHECK(xlsIrNodeUnitBytes(a)<xlsIrNodeGraphBytes(graph));
 CHECK(xlsIrNodeUnitNode(a,0)->text!=source.nodes[1].text);
 xstrbuf expected={0};CHECK(unitBook(&expected,a));
 size_t peak=xlsIrNodeGraphPeakBytes(graph);CHECK(peak>xlsIrNodeGraphBytes(graph));
 xls_ir_node_graph* exact=xlsIrNodeGraphCapture(&source,peak,NULL,NULL,&status);CHECK(exact && status==XLS_IR_GRAPH_OK);
 xls_ir_node_unit* different=xlsIrNodeGraphPinUnit(exact,1);CHECK(different && xlsIrNodeUnitProducer(different)!=xlsIrNodeUnitProducer(a));
 xlsIrNodeUnitRelease(different);xlsIrNodeGraphRelease(exact);
 CHECK(!xlsIrNodeGraphCapture(&source,peak-1,NULL,NULL,&status) && status==XLS_IR_GRAPH_BUDGET);
 xls_ir_node_image* image=xlsIrNodeGraphMaterialize(graph,SIZE_MAX,NULL,NULL,&status);CHECK(image);
 syntheticFree(&source);xlsIrNodeGraphRelease(graph);xlsIrNodeUnitRelease(b);
 xstrbuf current={0};CHECK(unitBook(&current,a) && equalBook(&expected,&current));xrtStrBufFree(&current);
 unit_reader readers[4];xthread* threads[4];
 for(size_t i=0;i<4;++i){readers[i]=(unit_reader){xlsIrNodeUnitRetain(a),&expected,0};threads[i]=xrtThreadCreate(readUnit,&readers[i],0);CHECK(threads[i]);}
 xlsIrNodeUnitRelease(a);xlsIrNodeImageDestroy(image);
 for(size_t i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}
 xrtStrBufFree(&expected);return 1;
}
static int graphFailures(void)
{
 XL6_IrModule source;CHECK(synthetic(&source));xls_ir_graph_status status;
 faultActive=1;faultAt=faultCalls=denied=0;
 xls_ir_node_graph* graph=xlsIrNodeGraphCapture(&source,SIZE_MAX,NULL,NULL,&status);faultActive=0;
 CHECK(graph && !overflow && live);size_t sites=faultCalls;CHECK(sites>=6);xlsIrNodeGraphRelease(graph);CHECK(!live);
 for(size_t i=1;i<=sites;++i){faultActive=1;faultAt=i;faultCalls=denied=0;
  graph=xlsIrNodeGraphCapture(&source,SIZE_MAX,NULL,NULL,&status);faultActive=0;
  CHECK(!graph && denied==1 && status==XLS_IR_GRAPH_MEMORY && !live && !overflow);
 }
 graph_stop stopAt={0};graph=xlsIrNodeGraphCapture(&source,SIZE_MAX,graphCancelled,&stopAt,&status);CHECK(graph);size_t calls=stopAt.calls;xlsIrNodeGraphRelease(graph);
 for(size_t i=1;i<=calls;++i){stopAt=(graph_stop){.at=i};faultActive=1;faultAt=faultCalls=denied=0;
  graph=xlsIrNodeGraphCapture(&source,SIZE_MAX,graphCancelled,&stopAt,&status);faultActive=0;
  CHECK(!graph && status==XLS_IR_GRAPH_CANCELLED && !live);
 }
 faultActive=1;faultAt=faultCalls=denied=0;
 graph=xlsIrNodeGraphCapture(&source,0,NULL,NULL,&status);faultActive=0;CHECK(!graph && status==XLS_IR_GRAPH_BUDGET && !faultCalls && !live);
 graph=xlsIrNodeGraphCapture(&source,SIZE_MAX,NULL,NULL,&status);CHECK(graph);
 faultActive=1;faultAt=1;faultCalls=denied=0;
 xls_ir_node_image* image=xlsIrNodeGraphMaterialize(graph,SIZE_MAX,NULL,NULL,&status);faultActive=0;
 CHECK(!image && denied==1 && status==XLS_IR_GRAPH_MEMORY && !live);
 stopAt=(graph_stop){.at=2};faultActive=1;faultAt=faultCalls=denied=0;
 image=xlsIrNodeGraphMaterialize(graph,SIZE_MAX,graphCancelled,&stopAt,&status);faultActive=0;
 CHECK(!image && status==XLS_IR_GRAPH_CANCELLED && !live);CHECK(graphMatches(&source,graph));xlsIrNodeGraphRelease(graph);
 XL6_IrModule bad=source;bad.nodeCount=SIZE_MAX;CHECK(!xlsIrNodeGraphCapture(&bad,SIZE_MAX,NULL,NULL,&status) && status==XLS_IR_GRAPH_INVALID);
 bad=source;bad.childCount=bad.childCapacity=SIZE_MAX;CHECK(!xlsIrNodeGraphCapture(&bad,SIZE_MAX,NULL,NULL,&status) && status==XLS_IR_GRAPH_BUDGET);
 XL6_IrNodeId saved=source.children[0];source.children[0]=source.root;
 CHECK(!xlsIrNodeGraphCapture(&source,SIZE_MAX,NULL,NULL,&status) && status==XLS_IR_GRAPH_INVALID);source.children[0]=saved;
 source.nodes[2].parent=4;CHECK(!xlsIrNodeGraphCapture(&source,SIZE_MAX,NULL,NULL,&status) && status==XLS_IR_GRAPH_INVALID);source.nodes[2].parent=1;
 size_t before=source.nodes[2].firstChild;source.nodes[2].firstChild=13;
 CHECK(!xlsIrNodeGraphCapture(&source,SIZE_MAX,NULL,NULL,&status) && status==XLS_IR_GRAPH_INVALID);source.nodes[2].firstChild=before;
 syntheticFree(&source);printf("node-graph allocation sites: %zu; cancellation points: %zu\n",sites,calls);return 1;
}
static int graphNative(void)
{
 char* path=xrtPathAbs("ir-node-graph-中文.xl");char* text=body(0);CHECK(path && text);
 xls_semantic_check* source=checked(path,text);CHECK(source && xlsSemanticCheckInfo(source).accepted);
 const XL6_IrModule* module=xlsSemanticCheckIr(source);CHECK(module);xls_ir_graph_status status;
 xls_ir_node_graph* graph=xlsIrNodeGraphCapture(module,128u*1024u*1024u,NULL,NULL,&status);CHECK(graph && graphMatches(module,graph));
 xls_ir_node_image* image=xlsIrNodeGraphMaterialize(graph,128u*1024u*1024u,NULL,NULL,&status);CHECK(image);
 XL6_IrModule view=*module;view.nodes=(XL6_IrNode*)xlsIrNodeImageNodes(image,NULL);view.children=(XL6_IrNodeId*)xlsIrNodeImageChildren(image,NULL);
 view.nodeCapacity=view.nodeCount;view.childCapacity=view.childCount;
 char *expected=dump(module),*actual=dump(&view);CHECK(expected && actual && !strcmp(expected,actual));xrtFree(expected);xrtFree(actual);
 xstrbuf old={0};CHECK(graphBook(&old,graph));xlsSemanticCheckDestroy(source);xrtFree(path);xrtFree(text);
 xstrbuf current={0};CHECK(graphBook(&current,graph) && equalBook(&old,&current));xrtStrBufFree(&old);xrtStrBufFree(&current);
 xlsIrNodeGraphRelease(graph);CHECK(xlsIrNodeImageNodes(image,NULL));xlsIrNodeImageDestroy(image);return 1;
}
static int graphShapes(size_t count,int wide)
{
 XL6_IrModule source={.ready=1,.stage=XL6_IR_STAGE_TYPED,.root=1,
  .nodeCount=count,.nodeCapacity=count,.childCount=count-1,.childCapacity=count-1};
 source.nodes=xrtCalloc(count,sizeof(*source.nodes));source.children=xrtCalloc(count,sizeof(*source.children));
 CHECK(source.nodes && source.children);
 for(size_t i=0;i<count;++i){XL6_IrNode* n=&source.nodes[i];n->kind=i ? XL6_IR_NODE_LITERAL : XL6_IR_NODE_MODULE;
  n->parent=i ? (wide ? 1 : (XL6_IrNodeId)i) : 0;
  n->childCount=wide ? (i ? 0 : count-1) : (i+1<count ? 1 : 0);
  n->firstChild=wide ? 0 : (i<count-1 ? i : count-1);
  source.children[i]=(XL6_IrNodeId)(i+2);
 }
 graph_stop stopAt={0};xls_ir_graph_status status;
 faultActive=1;faultAt=faultCalls=denied=peakBytes=0;
 xls_ir_node_graph* graph=xlsIrNodeGraphCapture(&source,SIZE_MAX,graphCancelled,&stopAt,&status);faultActive=0;
 CHECK(graph && status==XLS_IR_GRAPH_OK && !overflow);
 CHECK(xlsIrNodeGraphPeakBytes(graph)>=peakBytes && xlsIrNodeGraphBytes(graph)==liveBytes);
 CHECK(xlsIrNodeGraphUnitCount(graph)==(wide ? count : (count>1 ? 2u : 1u)));
 CHECK(graphMatches(&source,graph));size_t calls=stopAt.calls;CHECK(calls>=5);
 xls_ir_node_unit* unit=xlsIrNodeGraphPinUnit(graph,count>1 ? 1 : 0);CHECK(unit);
 size_t kept=xlsIrNodeUnitBytes(unit);xlsIrNodeGraphRelease(graph);
 CHECK(live==2 && liveBytes==kept); /* one unit plus its namespace,never siblings */
 xlsIrNodeUnitRelease(unit);CHECK(!live && !liveBytes);
 const size_t cuts[]={1,2,4,10,100,calls};
 for(size_t i=0;i<sizeof(cuts)/sizeof(*cuts);++i){if(cuts[i]>calls)continue;
  stopAt=(graph_stop){.at=cuts[i]};faultActive=1;faultAt=faultCalls=denied=peakBytes=0;
  graph=xlsIrNodeGraphCapture(&source,SIZE_MAX,graphCancelled,&stopAt,&status);faultActive=0;
  CHECK(!graph && status==XLS_IR_GRAPH_CANCELLED && !live && !liveBytes && !overflow);
 }
 syntheticFree(&source);return 1;
}
static int graphGenerations(void)
{
 XL6_IrModule source;CHECK(synthetic(&source));xls_ir_node_unit* units[32];size_t retained=0;
 for(size_t i=0;i<32;++i){xls_ir_graph_status status;faultActive=1;faultAt=faultCalls=denied=0;
  xls_ir_node_graph* graph=xlsIrNodeGraphCapture(&source,SIZE_MAX,NULL,NULL,&status);faultActive=0;CHECK(graph);
  units[i]=xlsIrNodeGraphPinUnit(graph,2);CHECK(units[i]);retained+=xlsIrNodeUnitBytes(units[i]);
  xlsIrNodeGraphRelease(graph);CHECK(live==2*(i+1) && liveBytes==retained && !overflow);
  if(i)CHECK(xlsIrNodeUnitProducer(units[i])!=xlsIrNodeUnitProducer(units[i-1]));
 }
 syntheticFree(&source);
 for(size_t i=32;i--;){CHECK(xlsIrNodeUnitNode(units[i],0)->literalData[0]==0xff);
  xlsIrNodeUnitRelease(units[i]);}
 CHECK(!live && !liveBytes);return 1;
}
int main(void)
{
 xrtAtomic32Init(&checks,0);
 if(!graphLifetime() || !graphFailures() || !graphNative() || !graphShapes(1,1) ||
  !graphShapes(2049,1) || !graphShapes(16385,0) || !graphGenerations())return 1;
 printf("analysis_ir_node_graph_test passed: %u checks; full native values,payloads,partitioned ownership,exact image,retirement,4threads,budgets,OOM,cancellation,deep/wide/empty,32generations\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;
}
