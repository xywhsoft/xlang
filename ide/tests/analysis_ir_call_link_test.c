#define XLS_IR_UNIT_CAPTURE_LIBRARY 1
#define __wrap_xl6IrDumpModuleJson priorUnitCaptureDump
#include "analysis_ir_unit_capture_test.c"
#undef __wrap_xl6IrDumpModuleJson
#include "analysis/ir_call_link.h"

static const char* callNames[8]={"callSites","callArguments","dynamicCallSites","dynamicCallArguments","dynamicMethods","dynamicMethodParameters","intrinsicCalls","intrinsicOperands"};
static const size_t callWidths[8]={sizeof(XL6_IrCallSite),sizeof(XL6_IrCallArgument),sizeof(XL6_IrDynamicCallSite),sizeof(XL6_IrDynamicCallArgument),sizeof(XL6_IrDynamicMethod),sizeof(XL6_IrDynamicMethodParameter),sizeof(XL6_IrIntrinsicCall),sizeof(XL6_IrIntrinsicOperand)};
static void* callRows(XL6_IrModule* m,size_t table,size_t* count)
{
 switch(table){
 case 0:*count=m->callSiteCount;return m->callSites;
 case 1:*count=m->callArgumentCount;return m->callArguments;
 case 2:*count=m->dynamicCallSiteCount;return m->dynamicCallSites;
 case 3:*count=m->dynamicCallArgumentCount;return m->dynamicCallArguments;
 case 4:*count=m->dynamicMethodCount;return m->dynamicMethods;
 case 5:*count=m->dynamicMethodParameterCount;return m->dynamicMethodParameters;
 case 6:*count=m->intrinsicCallCount;return m->intrinsicCalls;
 case 7:*count=m->intrinsicOperandCount;return m->intrinsicOperands;
 default:*count=0;return NULL;
 }
}
static void setCallRows(XL6_IrModule* m,size_t t,void* p,size_t n)
{
 switch(t){
 case 0:m->callSites=p;m->callSiteCount=m->callSiteCapacity=n;break;
 case 1:m->callArguments=p;m->callArgumentCount=m->callArgumentCapacity=n;break;
 case 2:m->dynamicCallSites=p;m->dynamicCallSiteCount=m->dynamicCallSiteCapacity=n;break;
 case 3:m->dynamicCallArguments=p;m->dynamicCallArgumentCount=m->dynamicCallArgumentCapacity=n;break;
 case 4:m->dynamicMethods=p;m->dynamicMethodCount=m->dynamicMethodCapacity=n;break;
 case 5:m->dynamicMethodParameters=p;m->dynamicMethodParameterCount=m->dynamicMethodParameterCapacity=n;break;
 case 6:m->intrinsicCalls=p;m->intrinsicCallCount=m->intrinsicCallCapacity=n;break;
 case 7:m->intrinsicOperands=p;m->intrinsicOperandCount=m->intrinsicOperandCapacity=n;break;
 }
}
static void originalRow(size_t t,void* value,size_t index)
{
 switch(t){
 case 0:*(XL6_IrCallSite*)value=(XL6_IrCallSite){.origin=1,.targetSymbol=2,.resultType=3,.receiver=4,.preparedGetter=5,.dispatchKind=index?XL6_IR_CALL_DISPATCH_PROTOCOL:XL6_IR_CALL_DISPATCH_CLASS,.dispatchSlot=1,.firstArgument=0,.argumentCount=2,.receiverEvaluationIndex=1,.fixedParameterCount=2};break;
 case 1:*(XL6_IrCallArgument*)value=(XL6_IrCallArgument){.expression=2,.parameter=1,.evaluationIndex=2,.name="key"};break;
 case 2:*(XL6_IrDynamicCallSite*)value=(XL6_IrDynamicCallSite){.origin=1,.subject=2,.subjectSymbol=3,.memberName="member",.resultType=4,.firstArgument=0,.argumentCount=2,.positionalCount=1,.keywordCount=1};break;
 case 3:*(XL6_IrDynamicCallArgument*)value=(XL6_IrDynamicCallArgument){.expression=1,.type=2,.name="keyword"};break;
 case 4:*(XL6_IrDynamicMethod*)value=(XL6_IrDynamicMethod){.ownerDeclaration=1,.ownerType=2,.runtimeTypeTag=1,.declaration=3,.targetSymbol=4,.name="method",.returnCount=1,.returnType=5,.firstParameter=0,.parameterCount=2,.fixedParameterCount=1,.variadicElementIdentity=UINT64_C(0xaabbccdd),.firstScalarResultCodec=0,.scalarResultCodecCount=2,.boundSignatureType=6};break;
 case 5:*(XL6_IrDynamicMethodParameter*)value=(XL6_IrDynamicMethodParameter){.declaration=1,.name="parameter",.type=2,.scalarCodec=1,.defaultExpression=3,.defaultLiteral="42",.defaultFunction=1};break;
 case 6:*(XL6_IrIntrinsicCall*)value=(XL6_IrIntrinsicCall){.origin=1,.resultType=2,.receiver=3,.receiverType=4,.payloadType=5,.runtimeResourceKeyType=6,.sourcePayloadType=7,.typeQueryTarget=8,.stringClassType=9,.firstOperand=0,.operandCount=2,.persistenceOptionsOperand=SIZE_MAX,.persistenceDestinationOperand=SIZE_MAX};break;
 case 7:*(XL6_IrIntrinsicOperand*)value=(XL6_IrIntrinsicOperand){.expression=1,.type=2};break;
 }
}
/* Handwritten expected values,independent of generated role operations. */
static void expectedRow(size_t t,void* p)
{
 switch(t){
 case 0:{XL6_IrCallSite* r=p;r->origin=11;r->targetSymbol=32;r->resultType=23;r->receiver=14;r->preparedGetter=15;r->dispatchSlot=r->dispatchKind==XL6_IR_CALL_DISPATCH_CLASS?41:51;r->firstArgument=1;break;}
 case 1:{XL6_IrCallArgument* r=p;r->expression=12;break;}
 case 2:{XL6_IrDynamicCallSite* r=p;r->origin=11;r->subject=12;r->subjectSymbol=33;r->resultType=24;r->firstArgument=1;break;}
 case 3:{XL6_IrDynamicCallArgument* r=p;r->expression=11;r->type=22;break;}
 case 4:{XL6_IrDynamicMethod* r=p;r->ownerDeclaration=11;r->ownerType=22;r->runtimeTypeTag=61;r->declaration=13;r->targetSymbol=34;r->returnType=25;r->boundSignatureType=26;r->firstParameter=1;r->firstScalarResultCodec=80;break;}
 case 5:{XL6_IrDynamicMethodParameter* r=p;r->declaration=11;r->type=22;r->scalarCodec=81;r->defaultExpression=13;r->defaultFunction=71;break;}
 case 6:{XL6_IrIntrinsicCall* r=p;r->origin=11;r->resultType=22;r->receiver=13;r->receiverType=24;r->payloadType=25;r->runtimeResourceKeyType=26;r->sourcePayloadType=27;r->typeQueryTarget=28;r->stringClassType=29;r->firstOperand=1;break;}
 case 7:{XL6_IrIntrinsicOperand* r=p;r->expression=11;r->type=22;break;}
 }
}
typedef struct call_map {int shift,bad,mode;size_t calls[XLS_CALL_DOMAIN_COUNT];} call_map;
static int callMapped(void* user,xls_ir_call_domain d,uint64_t from,uint64_t* to)
{
 call_map* m=user;++m->calls[d];if((int)d==m->bad){if(m->mode==3){*to=from+80+(from==2?8:0);return 1;}if(!m->mode)return 0;*to=m->mode==1?UINT64_MAX:0;return 1;}*to=from+(m->shift?(d<XLS_CALL_NODE?1:10*(uint64_t)(d-XLS_CALL_NODE+1)):0);return 1;
}
typedef struct call_fixture {XL6_IrModule current;xls_ir_call_binding bindings[24];call_map old,fresh;void* arrays[8];} call_fixture;
static int callFixture(call_fixture* f)
{
 memset(f,0,sizeof(*f));f->old=(call_map){.shift=1,.bad=-1};f->fresh=(call_map){.bad=-1};
 f->current.nodeCount=128;f->current.types.count=128;f->current.classLayoutCount=f->current.functionCount=f->current.classDispatchSlotCount=f->current.protocolDispatchSlotCount=f->current.dynamicMethodScalarCodecCount=128;
 for(size_t t=0;t<8;++t){
  f->arrays[t]=xrtCalloc(3,callWidths[t]);CHECK(f->arrays[t]);setCallRows(&f->current,t,f->arrays[t],3);
  for(size_t i=0;i<3;++i){void* row=(char*)f->arrays[t]+i*callWidths[t];xls_ir_table_capture_input input={.table=xlsIrTableSchemaFind(callNames[t]),.first=i?i-1:0,.count=1,.rows=row};
   if(i){originalRow(t,row,i-1);}else if(!t)((XL6_IrCallSite*)row)->firstArgument=SIZE_MAX;
   xls_ir_graph_status status;xls_ir_table_unit* unit=xlsIrTableUnitCapture(&input,NULL,SIZE_MAX,NULL,NULL,&status,NULL);CHECK(unit);
   f->bindings[t*3+i]=(xls_ir_call_binding){unit,xlsIrTableUnitProducer(unit),callMapped,i?&f->old:&f->fresh};if(i)expectedRow(t,row);else if(!t)((XL6_IrCallSite*)row)->firstArgument=3;
  }
 }
 return 1;
}
static void callFixtureFree(call_fixture* f)
{for(size_t i=0;i<24;++i)xlsIrTableUnitRelease(f->bindings[i].unit);for(size_t i=0;i<8;++i)xrtFree(f->arrays[i]);}
static int callBook(xstrbuf* book,const XL6_IrModule* m)
{
 return book_XL6_IrCallSite(book,&m->callSites[1]) && book_XL6_IrCallArgument(book,&m->callArguments[1]) &&
 book_XL6_IrDynamicCallSite(book,&m->dynamicCallSites[1]) && book_XL6_IrDynamicCallArgument(book,&m->dynamicCallArguments[1]) &&
 book_XL6_IrDynamicMethod(book,&m->dynamicMethods[1]) && book_XL6_IrDynamicMethodParameter(book,&m->dynamicMethodParameters[1]) &&
 book_XL6_IrIntrinsicCall(book,&m->intrinsicCalls[1]) && book_XL6_IrIntrinsicOperand(book,&m->intrinsicOperands[1]);
}
typedef struct call_cancel {size_t at,calls;}call_cancel;
static int callCancelled(void* user){call_cancel* c=user;return ++c->calls==c->at;}
static int callFailures(void)
{
 call_fixture f;CHECK(callFixture(&f));xls_ir_graph_status status;xls_ir_call_error error;
 faultActive=1;faultAt=faultCalls=denied=peakBytes=0;xls_ir_call_link* p=xlsIrCallLink(&f.current,f.bindings,24,SIZE_MAX,NULL,NULL,&status,&error);faultActive=0;
 CHECK(p && status==XLS_IR_GRAPH_OK && faultCalls==2 && live==1 && !overflow);size_t peak=xlsIrCallLinkPeakBytes(p);CHECK(peak==peakBytes && xlsIrCallLinkBytes(p)==liveBytes);xlsIrCallLinkDestroy(p);CHECK(!live);
 p=xlsIrCallLink(&f.current,f.bindings,24,peak,NULL,NULL,&status,&error);CHECK(p);xlsIrCallLinkDestroy(p);
 CHECK(!xlsIrCallLink(&f.current,f.bindings,24,peak-1,NULL,NULL,&status,&error) && status==XLS_IR_GRAPH_BUDGET);
 for(size_t i=1;i<=2;++i){faultActive=1;faultAt=i;faultCalls=denied=0;p=xlsIrCallLink(&f.current,f.bindings,24,SIZE_MAX,NULL,NULL,&status,&error);faultActive=0;CHECK(!p && status==XLS_IR_GRAPH_MEMORY && denied==1 && !live);}
 size_t cancelCalls=0;
 for(size_t i=1;;++i){call_cancel c={.at=i};p=xlsIrCallLink(&f.current,f.bindings,24,SIZE_MAX,callCancelled,&c,&status,&error);if(p){cancelCalls=c.calls;xlsIrCallLinkDestroy(p);break;}CHECK(status==XLS_IR_GRAPH_CANCELLED);}
 CHECK(cancelCalls>10);
 for(int domain=0;domain<XLS_CALL_DOMAIN_COUNT;++domain){CHECK(f.old.calls[domain]>0);for(int mode=0;mode<2;++mode){f.old.bad=domain;f.old.mode=mode;p=xlsIrCallLink(&f.current,f.bindings,24,SIZE_MAX,NULL,NULL,&status,&error);CHECK(!p && status==XLS_IR_GRAPH_INVALID && error.reason==(mode?XLS_CALL_RANGE:XLS_CALL_UNMAPPED));}}f.old.bad=XLS_CALL_CODEC;f.old.mode=3;CHECK(!xlsIrCallLink(&f.current,f.bindings,24,SIZE_MAX,NULL,NULL,&status,&error)&&error.reason==XLS_CALL_RANGE);f.old.bad=-1;CHECK(!f.fresh.calls[XLS_CALL_NODE]&&!f.fresh.calls[XLS_CALL_TYPE]);
 const void* producer=f.bindings[0].producer;f.bindings[0].producer=&f;CHECK(!xlsIrCallLink(&f.current,f.bindings,24,SIZE_MAX,NULL,NULL,&status,&error) && error.reason==XLS_CALL_NAMESPACE);f.bindings[0].producer=producer;
 xls_ir_call_binding save=f.bindings[2];f.bindings[2]=f.bindings[1];CHECK(!xlsIrCallLink(&f.current,f.bindings,24,SIZE_MAX,NULL,NULL,&status,&error) && error.reason==XLS_CALL_DUPLICATE);f.bindings[2]=save;
 uint64_t identity=f.current.dynamicMethods[1].variadicElementIdentity;f.current.dynamicMethods[1].variadicElementIdentity++;CHECK(!xlsIrCallLink(&f.current,f.bindings,24,SIZE_MAX,NULL,NULL,&status,&error) && error.reason==XLS_CALL_DIFFERENT);f.current.dynamicMethods[1].variadicElementIdentity=identity;
 xstrbuf expected={0},actual={0};CHECK(callBook(&expected,&f.current));p=xlsIrCallLink(&f.current,f.bindings,24,SIZE_MAX,NULL,NULL,&status,&error);CHECK(p);XL6_IrModule view={0};CHECK(xlsIrCallLinkApply(p,&view));callFixtureFree(&f);CHECK(callBook(&actual,&view) && tableBooksEqual(&expected,&actual));xrtStrBufFree(&expected);xrtStrBufFree(&actual);xlsIrCallLinkDestroy(p);
 XL6_IrModule empty={0};p=xlsIrCallLink(&empty,NULL,0,SIZE_MAX,NULL,NULL,&status,&error);CHECK(p);xlsIrCallLinkDestroy(p);CHECK(!xlsIrCallLink(&empty,NULL,0,0,NULL,NULL,&status,&error)&&status==XLS_IR_GRAPH_BUDGET);return 1;
}
static int callIdentity(void* user,xls_ir_call_domain d,uint64_t value,uint64_t* out){(void)user;(void)d;*out=value;return 1;}
static xls_ir_call_link* callNativeLink(const XL6_IrModule* source)
{
 size_t total=0;for(size_t t=0;t<8;++t){size_t n;callRows((XL6_IrModule*)source,t,&n);total+=n;}
 xls_ir_call_binding* bindings=xrtCalloc(total?total:1,sizeof(*bindings));if(!bindings)return NULL;size_t used=0;xls_ir_graph_status status;
 for(size_t t=0;t<8;++t){size_t n;char* rows=callRows((XL6_IrModule*)source,t,&n);for(size_t i=0;i<n;++i){xls_ir_table_capture_input input={.table=xlsIrTableSchemaFind(callNames[t]),.first=i,.count=1,.rows=rows+i*callWidths[t]};xls_ir_table_unit* unit=xlsIrTableUnitCapture(&input,NULL,SIZE_MAX,NULL,NULL,&status,NULL);if(!unit)goto failed;bindings[used++]=(xls_ir_call_binding){unit,xlsIrTableUnitProducer(unit),callIdentity,NULL};}}
 {xls_ir_call_error error;xls_ir_call_link* result=xlsIrCallLink(source,bindings,total,SIZE_MAX,NULL,NULL,&status,&error);for(size_t i=0;i<used;++i)xlsIrTableUnitRelease(bindings[i].unit);xrtFree(bindings);return result;}
failed:for(size_t i=0;i<used;++i)xlsIrTableUnitRelease(bindings[i].unit);xrtFree(bindings);return NULL;
}
static int callOracle(const XL6_IrModule* source)
{
 capture_view base;CHECK(captureView(source,&base));xls_ir_call_link* link=callNativeLink(source);CHECK(link);CHECK(xlsIrCallLinkApply(link,&base.module));char* expected=captureDump(source);CHECK(expected && verifyCapture(&base.module,expected));xrtFree(expected);xlsIrCallLinkDestroy(link);captureViewFree(&base);return 1;
}
#ifndef XLS_CALL_LINK_DUMP_NAME
#define XLS_CALL_LINK_DUMP_NAME __wrap_xl6IrDumpModuleJson
#endif
void XLS_CALL_LINK_DUMP_NAME(XL6_Output* out,const XL6_IrModule* module)
{
 if(xrtAtomic32Load(&oracleActive,XMEMORY_ACQUIRE)&&!oracleRecursing){oracleRecursing=1;xrtAtomic32FetchAdd(&oracleRuns,1,XMEMORY_RELAXED);if(!callOracle(module))xrtAtomic32FetchAdd(&oracleErrors,1,XMEMORY_RELAXED);oracleRecursing=0;}__real_xl6IrDumpModuleJson(out,module);
}
static int callQuiet(void* user,xls_ir_call_domain d,uint64_t from,uint64_t* to)
{call_map* m=user;(void)d;*to=from+(m->shift?(d<XLS_CALL_NODE?1:10*(uint64_t)(d-XLS_CALL_NODE+1)):0);return 1;}
typedef struct call_reader {call_fixture* fixture;const xstrbuf* expected;int okay;}call_reader;
static int32 callRead(void* opaque)
{
 call_reader* reader=opaque;reader->okay=1;
 for(size_t i=0;i<16;++i){xls_ir_graph_status status;xls_ir_call_link* link=xlsIrCallLink(&reader->fixture->current,reader->fixture->bindings,24,SIZE_MAX,NULL,NULL,&status,NULL);XL6_IrModule view={0};xstrbuf book={0};
  int okay=link && xlsIrCallLinkApply(link,&view) && callBook(&book,&view) && tableBooksEqual(&book,reader->expected);xrtStrBufFree(&book);xlsIrCallLinkDestroy(link);if(!okay){reader->okay=0;break;}}
 return reader->okay?0:1;
}
static int callLifetimes(void)
{
 call_fixture f;CHECK(callFixture(&f));for(size_t i=0;i<24;++i)f.bindings[i].map=callQuiet;
 xstrbuf expected={0};CHECK(callBook(&expected,&f.current));call_reader readers[4];xthread* threads[4];
 for(size_t i=0;i<4;++i){readers[i]=(call_reader){&f,&expected,0};threads[i]=xrtThreadCreate(callRead,&readers[i],0);CHECK(threads[i]);}
 for(size_t i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}callFixtureFree(&f);
 for(size_t generation=0;generation<32;++generation){faultActive=1;faultAt=faultCalls=denied=0;CHECK(callFixture(&f));size_t baseline=live;CHECK(baseline==56);
  for(size_t i=1;i<=3;++i){call_cancel c={.at=i};xls_ir_graph_status status;xls_ir_call_link* refused=xlsIrCallLink(&f.current,f.bindings,24,SIZE_MAX,callCancelled,&c,&status,NULL);CHECK(!refused&&status==XLS_IR_GRAPH_CANCELLED&&live==baseline);}
  xls_ir_graph_status status;xls_ir_call_link* link=xlsIrCallLink(&f.current,f.bindings,24,SIZE_MAX,NULL,NULL,&status,NULL);CHECK(link);XL6_IrModule view={0};CHECK(xlsIrCallLinkApply(link,&view));callFixtureFree(&f);CHECK(live==49);faultActive=0;
  xstrbuf book={0};CHECK(callBook(&book,&view)&&tableBooksEqual(&book,&expected));xrtStrBufFree(&book);xlsIrCallLinkDestroy(link);CHECK(!live&&!liveBytes&&!overflow);
 }
 xrtStrBufFree(&expected);return 1;
}
static int repeatedMap(void* user,xls_ir_call_domain d,uint64_t value,uint64_t* out)
{if(d!=XLS_CALL_ARGUMENT||value)return 0;*out=*(size_t*)user;return 1;}
static int callLargeCancellation(void)
{
 faultActive=1;faultAt=faultCalls=denied=0;XL6_IrModule current={0};current.callArgumentCount=current.callArgumentCapacity=256;current.callArguments=xrtCalloc(256,sizeof(*current.callArguments));xls_ir_call_binding* b=xrtCalloc(256,sizeof(*b));size_t* positions=xrtCalloc(256,sizeof(*positions));CHECK(current.callArguments&&b&&positions);
 xls_ir_table_capture_input input={.table=xlsIrTableSchemaFind("callArguments"),.count=1,.rows=current.callArguments};xls_ir_graph_status status;xls_ir_call_error error;
 xls_ir_table_unit* unit=xlsIrTableUnitCapture(&input,NULL,SIZE_MAX,NULL,NULL,&status,NULL);CHECK(unit);for(size_t i=0;i<256;++i){positions[i]=i;b[i]=(xls_ir_call_binding){unit,xlsIrTableUnitProducer(unit),repeatedMap,positions+i};}size_t baseline=live;CHECK(baseline==5);
 size_t calls=0;for(size_t at=1;;++at){call_cancel c={.at=at};xls_ir_call_link* p=xlsIrCallLink(&current,b,256,SIZE_MAX,callCancelled,&c,&status,&error);if(p){calls=c.calls;xlsIrCallLinkDestroy(p);break;}CHECK(status==XLS_IR_GRAPH_CANCELLED&&live==baseline);}
 CHECK(calls>10&&live==baseline);positions[255]=254;CHECK(!xlsIrCallLink(&current,b,256,SIZE_MAX,NULL,NULL,&status,&error)&&error.reason==XLS_CALL_DUPLICATE&&live==baseline);positions[255]=255;
 xls_ir_call_link* p=xlsIrCallLink(&current,b,256,SIZE_MAX,NULL,NULL,&status,&error);CHECK(p);XL6_IrModule view={0};CHECK(xlsIrCallLinkApply(p,&view));xrtFree(current.callArguments);xrtFree(b);xrtFree(positions);xlsIrTableUnitRelease(unit);CHECK(live==3&&view.callArgumentCount==256);for(size_t i=0;i<256;++i)CHECK(!view.callArguments[i].expression&&!view.callArguments[i].name);xlsIrCallLinkDestroy(p);CHECK(!live&&!liveBytes);faultActive=0;printf("call cancellation checkpoints small+large=%zu;partial retain rollback and repeated unit retired\n",calls);return 1;
}
typedef struct native_call_map {const XL6_IrModule* old;const XL6_IrModule* current;uint64_t* rows[8];uint64_t* nodes;uint64_t* symbols;uint64_t* types;}native_call_map;
static int nativeMapped(void* user,xls_ir_call_domain d,uint64_t value,uint64_t* out)
{
 native_call_map* map=user;
 if(d<XLS_CALL_NODE){size_t count;callRows((XL6_IrModule*)map->old,d,&count);if(value>=count||map->rows[d][value]==UINT64_MAX)return 0;*out=map->rows[d][value];return 1;}
 if(d==XLS_CALL_TYPE){if(value>map->old->types.count)return 0;*out=map->types[value];return *out!=0;}
 if(d==XLS_CALL_NODE){if(value>map->old->nodeCount)return 0;*out=map->nodes[value];return *out!=0;}
 if(d==XLS_CALL_SYMBOL&&value<map->old->symbolNodeCapacity){*out=map->symbols[value];return *out!=0;}
 *out=value;return 1;
}
static XL6_IrNodeId rowNode(size_t t,const void* p)
{
 switch(t){case 0:return ((const XL6_IrCallSite*)p)->origin;case 1:return ((const XL6_IrCallArgument*)p)->expression;
 case 2:return ((const XL6_IrDynamicCallSite*)p)->origin;case 3:return ((const XL6_IrDynamicCallArgument*)p)->expression;
 case 4:return ((const XL6_IrDynamicMethod*)p)->declaration;case 5:return ((const XL6_IrDynamicMethodParameter*)p)->declaration;
 case 6:return ((const XL6_IrIntrinsicCall*)p)->origin;case 7:return ((const XL6_IrIntrinsicOperand*)p)->expression;default:return 0;}
}
static int nativeTreeMap(native_call_map* map,XL6_IrNodeId from,XL6_IrNodeId to,size_t depth)
{
 CHECK(depth<512&&from&&to&&from<=map->old->nodeCount&&to<=map->current->nodeCount&&!map->nodes[from]);
 const XL6_IrNode *a=&map->old->nodes[from-1],*b=&map->current->nodes[to-1];
 CHECK(a->kind==b->kind&&a->sourceKind==b->sourceKind&&a->childCount==b->childCount&&((!a->text&&!b->text)||(a->text&&b->text&&!strcmp(a->text,b->text))));map->nodes[from]=to;
 for(size_t i=0;i<a->childCount;++i){CHECK(nativeTreeMap(map,map->old->children[a->firstChild+i],map->current->children[b->firstChild+i],depth+1));}return 1;
}
static int mixedNativeCalls(void)
{
 const char* text="class Box\n int value\n func read(int x) as int\n return x+self.value\n end\nend\nfunc target(int x) as int\n return x+1\nend\nfunc main(...) as int\n var object=Box()\n var result=object.read(3)\n return target(int(result))\nend\n";
 const char* prefix="func added(int x) as int\n return target(x)+1\nend\n";
 xstrbuf changed={0};CHECK(xrtStrBufAppend(&changed,xrtStrView(prefix))&&xrtStrBufAppend(&changed,xrtStrView(text)));char* path=xrtPathAbs("ir-call-mixed.xl");CHECK(path);
 xls_semantic_check* old=checked(path,text);xls_semantic_check* current=checked(path,changed.Data);CHECK(old&&current&&xlsSemanticCheckInfo(old).accepted&&xlsSemanticCheckInfo(current).accepted);
 const XL6_IrModule* a=xlsSemanticCheckIr(old);const XL6_IrModule* b=xlsSemanticCheckIr(current);CHECK(a&&b&&a->types.count==b->types.count);native_call_map map={.old=a,.current=b};map.nodes=xrtCalloc(a->nodeCount+1,sizeof(*map.nodes));map.symbols=xrtCalloc(a->symbolNodeCapacity,sizeof(*map.symbols));CHECK(map.nodes&&map.symbols);map.types=xrtCalloc(a->types.count+1,sizeof(*map.types));CHECK(map.types);
 for(size_t i=0;i<a->types.count;++i){const XL6_IrType* x=&a->types.items[i];size_t matches=0;
  for(size_t j=0;j<b->types.count;++j){const XL6_IrType* y=&b->types.items[j];if(x->kind==y->kind&&x->paramCount==y->paramCount&&x->returnCount==y->returnCount&&((!x->name&&!y->name)||(x->name&&y->name&&!strcmp(x->name,y->name)))&&((!x->displayName&&!y->displayName)||(x->displayName&&y->displayName&&!strcmp(x->displayName,y->displayName)))){map.types[i+1]=j+1;++matches;}}
  CHECK(matches==1);
 }
 map.nodes[a->root]=b->root;
 const XL6_IrNode* ar=&a->nodes[a->root-1];const XL6_IrNode* br=&b->nodes[b->root-1];CHECK(br->childCount==ar->childCount+1);
 for(size_t i=0;i<ar->childCount;++i)CHECK(nativeTreeMap(&map,a->children[ar->firstChild+i],b->children[br->firstChild+i+1],0));
 for(size_t i=1;i<a->symbolNodeCapacity;++i){if(!a->symbolNodes[i]){map.symbols[i]=i;continue;}XL6_IrNodeId n=(XL6_IrNodeId)map.nodes[a->symbolNodes[i]];for(size_t j=1;j<b->symbolNodeCapacity;++j)if(b->symbolNodes[j]==n){CHECK(!map.symbols[i]);map.symbols[i]=j;}CHECK(map.symbols[i]);}
 size_t total=0;for(size_t t=0;t<8;++t){size_t n,m;char* from=callRows((XL6_IrModule*)a,t,&n);char* to=callRows((XL6_IrModule*)b,t,&m);total+=m;map.rows[t]=xrtMalloc((n?n:1)*sizeof(uint64_t));CHECK(map.rows[t]);
  for(size_t i=0;i<n;++i){XL6_IrNodeId key=rowNode(t,from+i*callWidths[t]);map.rows[t][i]=UINT64_MAX;CHECK(key);for(size_t j=0;j<m;++j)if(rowNode(t,to+j*callWidths[t])==map.nodes[key] && (t!=4 || ((XL6_IrDynamicMethod*)(from+i*callWidths[t]))->kind==((XL6_IrDynamicMethod*)(to+j*callWidths[t]))->kind)){CHECK(map.rows[t][i]==UINT64_MAX);map.rows[t][i]=j;}CHECK(map.rows[t][i]!=UINT64_MAX);}
 }
 xls_ir_call_binding* bindings=xrtCalloc(total,sizeof(*bindings));CHECK(bindings);xls_ir_graph_status status;size_t used=0,reused=0,fresh=0;
 for(size_t t=0;t<8;++t){size_t n,m;char* from=callRows((XL6_IrModule*)a,t,&n);char* to=callRows((XL6_IrModule*)b,t,&m);
  for(size_t j=0;j<m;++j){size_t selected=SIZE_MAX;for(size_t i=0;i<n;++i)if(map.rows[t][i]==j)selected=i;
   xls_ir_table_capture_input input={.table=xlsIrTableSchemaFind(callNames[t]),.first=selected==SIZE_MAX?j:selected,.count=1,.rows=selected==SIZE_MAX?to+j*callWidths[t]:from+selected*callWidths[t]};xls_ir_table_unit* unit=xlsIrTableUnitCapture(&input,NULL,SIZE_MAX,NULL,NULL,&status,NULL);CHECK(unit);
   bindings[used++]=(xls_ir_call_binding){unit,xlsIrTableUnitProducer(unit),selected==SIZE_MAX?callIdentity:nativeMapped,selected==SIZE_MAX?NULL:&map};if(selected==SIZE_MAX)++fresh;else ++reused;
  }
 }
 xls_ir_call_error error;xls_ir_call_link* link=xlsIrCallLink(b,bindings,total,SIZE_MAX,NULL,NULL,&status,&error);if(!link)fprintf(stderr,"mixed call link reason=%d binding=%zu domain=%d field=%s value=%llu\n",error.reason,error.binding,error.domain,error.field?error.field:"",(unsigned long long)error.value);CHECK(link&&reused>8&&fresh>0);
 size_t movedNodes=0,movedTypes=0;for(size_t i=1;i<=a->nodeCount;++i)movedNodes+=map.nodes[i]!=i;for(size_t i=1;i<=a->types.count;++i)movedTypes+=map.types[i]!=i;CHECK(movedNodes>8&&movedTypes>0);
 capture_view base;CHECK(captureView(b,&base)&&xlsIrCallLinkApply(link,&base.module));char* expected=captureDump(b);CHECK(expected);
 for(size_t i=0;i<total;++i){xlsIrTableUnitRelease(bindings[i].unit);}xrtFree(bindings);for(size_t i=0;i<8;++i){xrtFree(map.rows[i]);}xrtFree(map.nodes);xrtFree(map.symbols);xrtFree(map.types);xlsSemanticCheckDestroy(old);xlsSemanticCheckDestroy(current);xrtStrBufFree(&changed);xrtFree(path);
 CHECK(verifyCapture(&base.module,expected));xrtFree(expected);xlsIrCallLinkDestroy(link);captureViewFree(&base);printf("mixed native calls reused=%zu rebuilt=%zu movedNodes=%zu movedTypes=%zu;source models and maps retired\n",reused,fresh,movedNodes,movedTypes);return 1;
}
static int callNativeTests(void)
{
 xrtAtomic32Init(&checks,0);xrtAtomic32Init(&oracleRuns,0);xrtAtomic32Init(&oracleErrors,0);xrtAtomic32Init(&oracleActive,0);
 if(!callFailures()||!callLifetimes()||!callLargeCancellation()||!mixedNativeCalls()){return 0;}
 char* path=xrtPathAbs("ir-call-link-中文.xl");CHECK(path);xrtAtomic32Store(&oracleActive,1,XMEMORY_RELEASE);int okay=corpus(path)&&regionCorpus(path);xrtAtomic32Store(&oracleActive,0,XMEMORY_RELEASE);CHECK(okay&&!xrtAtomic32Load(&oracleErrors,XMEMORY_RELAXED)&&xrtAtomic32Load(&oracleRuns,XMEMORY_RELAXED)==134);xrtFree(path);
 printf("analysis_ir_call_link_test passed: %u checks;%u complete native verifier/JSON oracles;8tables,explicit references,slices,mixed units,retirement,OOM,cancellation\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED),xrtAtomic32Load(&oracleRuns,XMEMORY_RELAXED));return 1;
}

#ifdef XLS_IR_CALL_LINK_LIBRARY
int xlsIrCallLinkFixtureMain(void){return callNativeTests()?0:1;}
#else
int main(void){return callNativeTests()?0:1;}
#endif
