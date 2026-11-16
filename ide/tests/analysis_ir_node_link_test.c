#define main oldIrMemoMain
#include "analysis_ir_memo_test.c"
#undef main
#include "analysis/ir_node_link.h"
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
static int equalBook(const xstrbuf* a,const xstrbuf* b)
{return a->Size==b->Size && (!a->Size || !memcmp(a->Data,b->Data,a->Size));}
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

/* Intentionally handwritten independent oracle,not the generated relocator. */
#define TYPE_FIELDS(F) F(declarationOwnerType) F(operatorExecutionType) F(type) F(storageType) F(typeValueTarget) \
 F(implicitReceiverType) F(namedCallableSignatureType) F(loopResourceType) F(loopProtocolIterableReceiverType) \
 F(loopProtocolIteratorReceiverType) F(loopProtocolIteratorType) F(loopProtocolNextResultType) F(loopProtocolItemType) F(loopGeneratorItemType)
#define SYMBOL_FIELDS(F) F(symbol) F(targetSymbol) F(compoundOperatorSymbol) F(constructorReceiverSymbol) \
 F(namedCallableTargetSymbol) F(loopProtocolIterTarget) F(loopProtocolNextTarget) F(foreignCall.rollbackSymbol)
typedef struct test_map {uint64_t values[5][4096];size_t calls;int badDomain,mode;} test_map;
static int mapped(void* opaque,xls_ir_reference_domain d,uint64_t old,uint64_t* out)
{
 test_map* m=opaque;++m->calls;if((int)d==m->badDomain){if(m->mode==0)return 0;*out=m->mode==1 ? 0 : UINT64_MAX;return 1;}
 if(old>=4096 || !m->values[d][old])return 0;
 *out=m->values[d][old]-1;return 1;
}
static int identity(void* unused,xls_ir_reference_domain d,uint64_t old,uint64_t* out)
{(void)unused;(void)d;*out=old;return 1;}
static XL6_IrNodeId permute(XL6_IrNodeId id){return id ? (id+1)%7+1 : 0;}
static void fillReferences(XL6_IrNode* n,size_t i)
{
#define T(f) n->f=i%2 ? 0 : 1;
 TYPE_FIELDS(T)
#undef T
#define S(f) n->f=i%2 ? 0 : 2;
 SYMBOL_FIELDS(S)
#undef S
 n->implicitFieldDeclaration=i%2 ? 0 : 7;n->region=i%2 ? 0 : 3;n->targetRegion=i%2 ? 0 : 4;
 n->unionField=i%2 ? SIZE_MAX : 0;n->span=(XL6_IrSpan){(int)i+1,4};
}
static void expectedReferences(XL6_IrNode* n)
{
#define T(f) if(n->f)n->f+=100;
 TYPE_FIELDS(T)
#undef T
#define S(f) if(n->f)n->f+=200;
 SYMBOL_FIELDS(S)
#undef S
 n->implicitFieldDeclaration=permute(n->implicitFieldDeclaration);n->parent=permute(n->parent);
 if(n->region)n->region+=300;
 if(n->targetRegion)n->targetRegion+=300;
 if(n->unionField!=SIZE_MAX)n->unionField+=5;
}
typedef struct link_fixture {
 XL6_IrModule source,current;xls_ir_node_graph* graph;xls_ir_node_binding bindings[3];test_map map;
} link_fixture;
static int fixtureCreate(link_fixture* f)
{
 memset(f,0,sizeof(*f));f->map.badDomain=-1;CHECK(synthetic(&f->source));
 for(size_t i=0;i<7;++i)fillReferences(&f->source.nodes[i],i);
 f->current=f->source;f->current.root=permute(f->source.root);f->current.childCount=f->current.childCapacity=15;
 f->current.nodes=xrtCalloc(7,sizeof(XL6_IrNode));f->current.children=xrtCalloc(15,sizeof(XL6_IrNodeId));CHECK(f->current.nodes && f->current.children);
 f->current.children[0]=99;f->current.children[1]=UINT32_MAX;f->current.children[2]=88;
 for(size_t i=0;i<12;++i){XL6_IrNodeId id=f->source.children[i];f->current.children[i+3]=id<=7 ? permute(id) : id;}
 for(size_t i=0;i<7;++i){XL6_IrNode n=f->source.nodes[i];expectedReferences(&n);n.firstChild+=3;n.span.line+=1000;n.span.column+=80;f->current.nodes[permute((XL6_IrNodeId)i+1)-1]=n;}
 for(size_t i=1;i<=7;++i)f->map.values[XLS_IR_REF_NODE][i]=permute((XL6_IrNodeId)i)+1;
 f->map.values[XLS_IR_REF_TYPE][1]=102;f->map.values[XLS_IR_REF_SYMBOL][2]=203;
 f->map.values[XLS_IR_REF_REGION][3]=304;f->map.values[XLS_IR_REF_REGION][4]=305;f->map.values[XLS_IR_REF_AGGREGATE_FIELD][0]=6;
 xls_ir_graph_status status;f->graph=xlsIrNodeGraphCapture(&f->source,SIZE_MAX,NULL,NULL,&status);CHECK(f->graph && status==XLS_IR_GRAPH_OK);
 for(size_t i=0;i<3;++i){xls_ir_node_unit* u=xlsIrNodeGraphPinUnit(f->graph,i);CHECK(u);f->bindings[i]=(xls_ir_node_binding){u,xlsIrNodeUnitProducer(u),mapped,&f->map};}
 return 1;
}
static xls_ir_node_layout fixtureLayout(const link_fixture* f)
{return (xls_ir_node_layout){f->current.nodes,7,f->current.children,15,f->current.root,101,6};}
static void fixtureFree(link_fixture* f)
{
 for(size_t i=0;i<3;++i)xlsIrNodeUnitRelease(f->bindings[i].unit);
 xlsIrNodeGraphRelease(f->graph);
 xrtFree(f->current.nodes);xrtFree(f->current.children);syntheticFree(&f->source);memset(f,0,sizeof(*f));
}
static int linkMatches(const XL6_IrModule* current,const xls_ir_node_link* link)
{
 size_t n,c;XL6_IrModule view=*current;view.nodes=(XL6_IrNode*)xlsIrNodeLinkNodes(link,&n);view.children=(XL6_IrNodeId*)xlsIrNodeLinkChildren(link,&c);
 CHECK(n==current->nodeCount && c==current->childCount && xlsIrNodeLinkRoot(link)==current->root);
 xstrbuf a={0},b={0};CHECK(moduleBook(&a,current) && moduleBook(&b,&view) && equalBook(&a,&b));
 CHECK(!memcmp(current->children,view.children,c*sizeof(*view.children)));xrtStrBufFree(&a);xrtStrBufFree(&b);return 1;
}
typedef struct link_stop {size_t calls,at;} link_stop;
static int linkCancelled(void* opaque){link_stop* s=opaque;return ++s->calls==s->at;}
static int linkFailures(void)
{
 link_fixture f;CHECK(fixtureCreate(&f));xls_ir_node_layout l=fixtureLayout(&f);xls_ir_graph_status status;xls_ir_link_error error;
 faultActive=1;faultAt=faultCalls=denied=peakBytes=0;
 xls_ir_node_link* a=xlsIrNodeLink(&l,f.bindings,3,SIZE_MAX,NULL,NULL,&status,&error);faultActive=0;
 CHECK(a && status==XLS_IR_GRAPH_OK && error.reason==XLS_IR_LINK_NONE && !overflow && faultCalls==2 && live==1);
 CHECK(xlsIrNodeLinkBytes(a)==liveBytes && xlsIrNodeLinkPeakBytes(a)==peakBytes && linkMatches(&f.current,a));
 size_t peak=xlsIrNodeLinkPeakBytes(a);xlsIrNodeLinkDestroy(a);CHECK(!live && !liveBytes);
 a=xlsIrNodeLink(&l,f.bindings,3,peak,NULL,NULL,&status,&error);CHECK(a);xlsIrNodeLinkDestroy(a);
 CHECK(!xlsIrNodeLink(&l,f.bindings,3,peak-1,NULL,NULL,&status,&error) && status==XLS_IR_GRAPH_BUDGET);
 CHECK(!xlsIrNodeLink(&l,f.bindings,3,0,NULL,NULL,&status,&error) && status==XLS_IR_GRAPH_BUDGET);
 for(size_t i=1;i<=2;++i){faultActive=1;faultAt=i;faultCalls=denied=0;
  a=xlsIrNodeLink(&l,f.bindings,3,SIZE_MAX,NULL,NULL,&status,&error);faultActive=0;
  CHECK(!a && status==XLS_IR_GRAPH_MEMORY && denied==1 && !live && !liveBytes);}
 link_stop stop={0};a=xlsIrNodeLink(&l,f.bindings,3,SIZE_MAX,linkCancelled,&stop,&status,&error);CHECK(a);size_t calls=stop.calls;xlsIrNodeLinkDestroy(a);
 for(size_t i=1;i<=calls;++i){stop=(link_stop){.at=i};faultActive=1;faultAt=faultCalls=denied=0;
  a=xlsIrNodeLink(&l,f.bindings,3,SIZE_MAX,linkCancelled,&stop,&status,&error);faultActive=0;
  CHECK(!a && status==XLS_IR_GRAPH_CANCELLED && !live && !liveBytes);}
#define BAD(expectedReason) do{a=xlsIrNodeLink(&l,f.bindings,3,SIZE_MAX,NULL,NULL,&status,&error);CHECK(!a && status==XLS_IR_GRAPH_INVALID && error.reason==(expectedReason));}while(0)
 for(int d=0;d<5;++d)for(int mode=0;mode<3;++mode){f.map.badDomain=d;f.map.mode=mode;
  if(d==XLS_IR_REF_AGGREGATE_FIELD && mode==1){a=xlsIrNodeLink(&l,f.bindings,3,SIZE_MAX,NULL,NULL,&status,&error);CHECK(a);xlsIrNodeLinkDestroy(a);}
  else BAD(mode==0 ? XLS_IR_LINK_UNMAPPED : XLS_IR_LINK_RANGE);}
 f.map.badDomain=-1;const void* producer=f.bindings[0].producer;f.bindings[0].producer=&f;BAD(XLS_IR_LINK_NAMESPACE);f.bindings[0].producer=producer;
 uint64_t map=f.map.values[XLS_IR_REF_NODE][2];f.map.values[XLS_IR_REF_NODE][2]=f.map.values[XLS_IR_REF_NODE][1];BAD(XLS_IR_LINK_DUPLICATE);f.map.values[XLS_IR_REF_NODE][2]=map;
 XL6_IrNode* node=&f.current.nodes[0];uint32_t flags=node->flags;node->flags^=1;BAD(XLS_IR_LINK_SOURCE);node->flags=flags;
 char* text=node->text;node->text="changed";BAD(XLS_IR_LINK_SOURCE);node->text=text;
 unsigned char* data=node->literalData;node->literalData=(unsigned char*)"bad";BAD(XLS_IR_LINK_SOURCE);node->literalData=data;
 size_t span=node->firstChild;node->firstChild=16;BAD(XLS_IR_LINK_TOPOLOGY);node->firstChild=span;
 XL6_IrNodeId child=f.current.children[3];f.current.children[3]=l.root;BAD(XLS_IR_LINK_TOPOLOGY);f.current.children[3]=child;
 uint32_t parent=node->parent;node->parent=0;BAD(XLS_IR_LINK_TOPOLOGY);node->parent=parent;
 map=f.map.values[XLS_IR_REF_REGION][3];f.map.values[XLS_IR_REF_REGION][3]=(uint64_t)UINT32_MAX+1;
 a=xlsIrNodeLink(&l,f.bindings,3,SIZE_MAX,NULL,NULL,&status,&error);CHECK(a);xlsIrNodeLinkDestroy(a); /* largest native region is valid */
 f.map.values[XLS_IR_REF_REGION][3]=map;
#undef BAD
 a=xlsIrNodeLink(&l,f.bindings,3,SIZE_MAX,NULL,NULL,&status,&error);CHECK(a && linkMatches(&f.current,a));
 xstrbuf expected={0};CHECK(moduleBook(&expected,&f.current));XL6_IrModule view=f.current;
 view.nodes=(XL6_IrNode*)xlsIrNodeLinkNodes(a,NULL);view.children=(XL6_IrNodeId*)xlsIrNodeLinkChildren(a,NULL);fixtureFree(&f);
 xstrbuf actual={0};CHECK(moduleBook(&actual,&view) && equalBook(&expected,&actual));xrtStrBufFree(&actual);xrtStrBufFree(&expected);xlsIrNodeLinkDestroy(a);
 printf("node-link allocation sites: 2; cancellation points: %zu\n",calls);return 1;
}

static char* linkRealDump(const XL6_IrModule* module);
void __real_xl6IrDumpModuleJson(XL6_Output*,const XL6_IrModule*);
static xatomic32 oracleRuns,oracleErrors,oracleActive;static _Thread_local int oracleRecursing;
static xls_ir_node_link* identityLink(const XL6_IrModule* source)
{
 xls_ir_graph_status status;xls_ir_node_graph* g=xlsIrNodeGraphCapture(source,SIZE_MAX,NULL,NULL,&status);if(!g)return NULL;
 size_t count=xlsIrNodeGraphUnitCount(g);xls_ir_node_binding* b=xrtCalloc(count,sizeof(*b));if(!b){xlsIrNodeGraphRelease(g);return NULL;}
 for(size_t i=0;i<count;++i){b[i].unit=xlsIrNodeGraphPinUnit(g,i);b[i].producer=xlsIrNodeUnitProducer(b[i].unit);b[i].map=identity;}
 xls_ir_node_layout layout={source->nodes,source->nodeCount,source->children,source->childCount,source->root,source->types.count,source->aggregateFieldCount};
 xls_ir_link_error error;xls_ir_node_link* link=xlsIrNodeLink(&layout,b,count,SIZE_MAX,NULL,NULL,&status,&error);
 if(!link)fprintf(stderr,"identity link failed: status=%d reason=%d field=%s value=%llu\n",status,error.reason,error.field ? error.field : "none",(unsigned long long)error.value);
 for(size_t i=0;i<count;++i)xlsIrNodeUnitRelease(b[i].unit);
 xrtFree(b);xlsIrNodeGraphRelease(g);return link;
}
static xls_ir_table_image* linkTables(const XL6_IrModule* source,const xls_ir_node_link* nodes)
{
 xls_ir_graph_status status;xls_ir_table_graph* tables=xlsIrTablesCapture(source,SIZE_MAX,NULL,NULL,&status);if(!tables)return NULL;
 size_t n,c;const XL6_IrNode* nd=xlsIrNodeLinkNodes(nodes,&n);const XL6_IrNodeId* cd=xlsIrNodeLinkChildren(nodes,&c);
 xls_ir_table_image* image=xlsIrTablesMaterialize(tables,nd,n,cd,c,SIZE_MAX,NULL,NULL,&status);xlsIrTablesRelease(tables);return image;
}
static int verifyLinked(const XL6_IrModule* actual,const char* expected)
{
 XL6_DiagList diags;xl6DiagInit(&diags);CHECK(xl6IrVerifyModule(actual,&diags) && !diags.count);xl6DiagFree(&diags);
 char* value=linkRealDump(actual);CHECK(value && !strcmp(expected,value));xrtFree(value);return 1;
}
static int nativeOracle(const XL6_IrModule* source)
{
 xls_ir_node_link* link=identityLink(source);CHECK(link);xls_ir_table_image* tables=linkTables(source,link);CHECK(tables);
 char* expected=linkRealDump(source);CHECK(expected && verifyLinked(xlsIrTableImageModule(tables),expected));
 xrtFree(expected);xlsIrTableImageDestroy(tables);xlsIrNodeLinkDestroy(link);return 1;
}
void __wrap_xl6IrDumpModuleJson(XL6_Output* out,const XL6_IrModule* module)
{
 if(xrtAtomic32Load(&oracleActive,XMEMORY_ACQUIRE) && !oracleRecursing){oracleRecursing=1;xrtAtomic32FetchAdd(&oracleRuns,1,XMEMORY_RELAXED);
  if(!nativeOracle(module))xrtAtomic32FetchAdd(&oracleErrors,1,XMEMORY_RELAXED);
  oracleRecursing=0;}
 __real_xl6IrDumpModuleJson(out,module);
}
static char* linkRealDump(const XL6_IrModule* module)
{XL6_Output out;if(!xl6OutputOpenMemory(&out))return NULL;__real_xl6IrDumpModuleJson(&out,module);char* value=xl6OutputHasFailed(&out) ? NULL : xl6OutputTakeMemoryText(&out,NULL);xl6OutputDiscard(&out);return value;}
static int nativeRetirement(void)
{
 char* path=xrtPathAbs("ir-node-link-中文.xl");CHECK(path);
 for(unsigned edit=0;edit<8;++edit){char* text=body(edit);CHECK(text);xls_semantic_check* check=checked(path,text);CHECK(check && xlsSemanticCheckInfo(check).accepted);
  const XL6_IrModule* source=xlsSemanticCheckIr(check);CHECK(source);char* expected=linkRealDump(source);CHECK(expected);
  xls_ir_node_link* link=identityLink(source);CHECK(link);xls_ir_table_image* tables=linkTables(source,link);CHECK(tables);
  xlsSemanticCheckDestroy(check);xrtFree(text);CHECK(verifyLinked(xlsIrTableImageModule(tables),expected));
  xrtFree(expected);xlsIrTableImageDestroy(tables);xlsIrNodeLinkDestroy(link);}
 xrtAtomic32Store(&oracleActive,1,XMEMORY_RELEASE);int okay=corpus(path) && regionCorpus(path);xrtAtomic32Store(&oracleActive,0,XMEMORY_RELEASE);
 CHECK(okay && !xrtAtomic32Load(&oracleErrors,XMEMORY_RELAXED) && xrtAtomic32Load(&oracleRuns,XMEMORY_RELAXED)>20);xrtFree(path);return 1;
}
static int pairReference(test_map* map,xls_ir_reference_domain d,uint64_t a,uint64_t b)
{
 uint64_t sentinel=d==XLS_IR_REF_AGGREGATE_FIELD ? SIZE_MAX : 0;
 if(a==sentinel || b==sentinel){CHECK(a==b);return 1;}
 CHECK(a<4096 && b<4096);CHECK(!map->values[d][a] || map->values[d][a]==b+1);map->values[d][a]=b+1;return 1;
}
static int pairReferences(test_map* map,const XL6_IrNode* a,const XL6_IrNode* b)
{
#define T(f) CHECK(pairReference(map,XLS_IR_REF_TYPE,a->f,b->f));
 TYPE_FIELDS(T)
#undef T
#define S(f) CHECK(pairReference(map,XLS_IR_REF_SYMBOL,a->f,b->f));
 SYMBOL_FIELDS(S)
#undef S
 CHECK(pairReference(map,XLS_IR_REF_NODE,a->implicitFieldDeclaration,b->implicitFieldDeclaration));
 CHECK(pairReference(map,XLS_IR_REF_NODE,a->parent,b->parent));
 CHECK(pairReference(map,XLS_IR_REF_REGION,a->region,b->region));
 CHECK(pairReference(map,XLS_IR_REF_REGION,a->targetRegion,b->targetRegion));
 CHECK(pairReference(map,XLS_IR_REF_AGGREGATE_FIELD,a->unionField,b->unionField));return 1;
}
static int nativeMixed(void)
{
 /* Test oracle supplies explicit reference correspondences from two fresh
  * native analyses. This is deliberately not a production eligibility/map
  * algorithm: proving dependencies is a separate prerequisite. */
 char* path=xrtPathAbs("ir-node-link-mixed.xl"),*a=body(0),*b=body(5);CHECK(path && a && b);
 const char* insertion=strstr(b," string inserted =");CHECK(insertion);xstrbuf edited={0};
 CHECK(xrtStrBufAppend(&edited,(xstrview){b,(size_t)(insertion-b)}) &&
  xrtStrBufAppend(&edited,xrtStrView(" if local > 2\r\n int added = 5\r\n local = local + added\r\n end\r\n")) &&
  xrtStrBufAppend(&edited,xrtStrView(insertion)));
 xrtFree(b);b=xrtStrDup(edited.Data);xrtStrBufFree(&edited);CHECK(b);
 xls_semantic_check* old=checked(path,a),*current=checked(path,b);CHECK(old && current && xlsSemanticCheckInfo(old).accepted && xlsSemanticCheckInfo(current).accepted);
 const XL6_IrModule* om=xlsSemanticCheckIr(old),*cm=xlsSemanticCheckIr(current);CHECK(om && cm && om->nodeCount<cm->nodeCount);
 xls_ir_graph_status status;xls_ir_node_graph* og=xlsIrNodeGraphCapture(om,SIZE_MAX,NULL,NULL,&status),*cg=xlsIrNodeGraphCapture(cm,SIZE_MAX,NULL,NULL,&status);CHECK(og && cg);
 size_t count=xlsIrNodeGraphUnitCount(cg);CHECK(count==xlsIrNodeGraphUnitCount(og));
 xls_ir_node_binding* bindings=xrtCalloc(count,sizeof(*bindings));test_map* map=xrtCalloc(1,sizeof(*map));CHECK(bindings && map);map->badDomain=-1;
 size_t reused=0,changed=0;const char* retained=NULL;size_t retainedId=0;
 for(size_t i=0;i<count;++i){xls_ir_node_unit* ou=xlsIrNodeGraphPinUnit(og,i),*cu=xlsIrNodeGraphPinUnit(cg,i);CHECK(ou && cu);
  if(!i || xlsIrNodeUnitCount(ou)!=xlsIrNodeUnitCount(cu)){bindings[i]=(xls_ir_node_binding){cu,xlsIrNodeUnitProducer(cu),identity,NULL};xlsIrNodeUnitRelease(ou);if(i)++changed;}
  else{++reused;bindings[i]=(xls_ir_node_binding){ou,xlsIrNodeUnitProducer(ou),mapped,map};
   for(size_t j=0;j<xlsIrNodeUnitCount(ou);++j){XL6_IrNodeId from=xlsIrNodeUnitNativeId(ou,j),to=xlsIrNodeUnitNativeId(cu,j);
    CHECK(pairReference(map,XLS_IR_REF_NODE,from,to));CHECK(pairReferences(map,xlsIrNodeUnitNode(ou,j),xlsIrNodeUnitNode(cu,j)));
    if(xlsIrNodeUnitNode(ou,j)->text){retained=xlsIrNodeUnitNode(ou,j)->text;retainedId=to;}}
   xlsIrNodeUnitRelease(cu);}
 }
 CHECK(reused>10 && changed==1 && retained && retainedId);
 size_t relocated[5]={0};for(size_t d=0;d<5;++d)for(size_t i=0;i<4096;++i)if(map->values[d][i] && map->values[d][i]!=i+1)++relocated[d];
 CHECK(relocated[XLS_IR_REF_NODE] && relocated[XLS_IR_REF_SYMBOL] && relocated[XLS_IR_REF_REGION]);
 xls_ir_node_layout layout={cm->nodes,cm->nodeCount,cm->children,cm->childCount,cm->root,cm->types.count,cm->aggregateFieldCount};
 xls_ir_link_error error;xls_ir_node_link* link=xlsIrNodeLink(&layout,bindings,count,SIZE_MAX,NULL,NULL,&status,&error);
 if(!link)fprintf(stderr,"mixed link status=%d reason=%d source=%u destination=%u field=%s value=%llu\n",status,error.reason,error.source,error.destination,error.field ? error.field : "none",(unsigned long long)error.value);
 CHECK(link);CHECK(xlsIrNodeLinkNodes(link,NULL)[retainedId-1].text==retained);
 xls_ir_table_image* tables=linkTables(cm,link);CHECK(tables);char* expected=linkRealDump(cm);CHECK(expected);
 for(size_t i=0;i<count;++i)xlsIrNodeUnitRelease(bindings[i].unit);
 xrtFree(bindings);xrtFree(map);xlsIrNodeGraphRelease(og);xlsIrNodeGraphRelease(cg);xlsSemanticCheckDestroy(old);xlsSemanticCheckDestroy(current);
 xrtFree(a);xrtFree(b);xrtFree(path);CHECK(verifyLinked(xlsIrTableImageModule(tables),expected));
 printf("mixed native declarations: reused=%zu rebuilt=%zu; relocated node/type/symbol/region/field=%zu/%zu/%zu/%zu/%zu\n",reused,changed,relocated[0],relocated[1],relocated[2],relocated[3],relocated[4]);
 xrtFree(expected);xlsIrTableImageDestroy(tables);xlsIrNodeLinkDestroy(link);return 1;
}
static int linkShape(size_t count,int wide)
{
 XL6_IrModule module={.ready=1,.stage=XL6_IR_STAGE_TYPED,.root=1,.nodeCount=count,.nodeCapacity=count,.childCount=count-1,.childCapacity=count-1};
 module.nodes=xrtCalloc(count,sizeof(*module.nodes));module.children=xrtCalloc(count,sizeof(*module.children));CHECK(module.nodes && module.children);
 for(size_t i=0;i<count;++i){XL6_IrNode* n=&module.nodes[i];n->kind=i ? XL6_IR_NODE_LITERAL : XL6_IR_NODE_MODULE;n->unionField=SIZE_MAX;
  n->parent=i ? (wide ? 1 : (XL6_IrNodeId)i) : 0;n->childCount=wide ? (i ? 0 : count-1) : (i+1<count ? 1 : 0);
  n->firstChild=wide ? 0 : (i<count-1 ? i : count-1);module.children[i]=(XL6_IrNodeId)(i+2);}
 xls_ir_graph_status status;xls_ir_node_graph* graph=xlsIrNodeGraphCapture(&module,SIZE_MAX,NULL,NULL,&status);CHECK(graph);
 size_t units=xlsIrNodeGraphUnitCount(graph);xls_ir_node_binding* bindings=xrtCalloc(units,sizeof(*bindings));CHECK(bindings);
 for(size_t i=0;i<units;++i){xls_ir_node_unit* u=xlsIrNodeGraphPinUnit(graph,i);bindings[i]=(xls_ir_node_binding){u,xlsIrNodeUnitProducer(u),identity,NULL};}
 xls_ir_node_layout layout={module.nodes,count,module.children,count-1,1,0,0};link_stop stop={0};xls_ir_link_error error;
 faultActive=1;faultAt=faultCalls=denied=peakBytes=0;
 xls_ir_node_link* link=xlsIrNodeLink(&layout,bindings,units,SIZE_MAX,linkCancelled,&stop,&status,&error);faultActive=0;
 CHECK(link && status==XLS_IR_GRAPH_OK && !overflow && live==1 && liveBytes==xlsIrNodeLinkBytes(link) && peakBytes==xlsIrNodeLinkPeakBytes(link));
 CHECK(linkMatches(&module,link));size_t calls=stop.calls;xlsIrNodeLinkDestroy(link);CHECK(!live && !liveBytes);
 const size_t cuts[]={1,2,4,10,100,calls};for(size_t i=0;i<sizeof(cuts)/sizeof(*cuts);++i){if(cuts[i]>calls)continue;
  stop=(link_stop){.at=cuts[i]};faultActive=1;faultAt=faultCalls=denied=0;
  link=xlsIrNodeLink(&layout,bindings,units,SIZE_MAX,linkCancelled,&stop,&status,&error);faultActive=0;
  CHECK(!link && status==XLS_IR_GRAPH_CANCELLED && !live && !liveBytes);}
 for(size_t i=0;i<units;++i)xlsIrNodeUnitRelease(bindings[i].unit);
 xrtFree(bindings);xlsIrNodeGraphRelease(graph);syntheticFree(&module);return 1;
}
int main(void)
{
 xrtAtomic32Init(&checks,0);xrtAtomic32Init(&oracleRuns,0);xrtAtomic32Init(&oracleErrors,0);xrtAtomic32Init(&oracleActive,0);
 if(!linkFailures() || !nativeRetirement() || !nativeMixed() || !linkShape(1,1) || !linkShape(2049,1) || !linkShape(16385,0))return 1;
 printf("analysis_ir_node_link_test passed: %u checks; %u complete native verifier/JSON oracles; all reference domains,permuted IDs,current spans,retirement,budget,OOM,cancellation\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED),xrtAtomic32Load(&oracleRuns,XMEMORY_RELAXED));return 0;
}
