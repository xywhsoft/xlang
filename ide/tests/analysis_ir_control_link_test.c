#define XLS_IR_CALL_LINK_LIBRARY 1
#define XLS_CALL_LINK_DUMP_NAME priorCallLinkDump
#include "analysis_ir_call_link_test.c"
#include "analysis/ir_control_link.h"
#include "ir_enum_fixture.inc"

static const char* controlNames[4]={"cleanupActions","exitSteps","exitPlans","valueInitializers"};
static const size_t controlWidths[4]={sizeof(XL6_IrCleanupAction),sizeof(XL6_IrExitStep),sizeof(XL6_IrExitPlan),sizeof(XL6_IrValueInitializer)};
static void* controlRows(XL6_IrModule* m,size_t t,size_t* n)
{switch(t){case 0:*n=m->cleanupActionCount;return m->cleanupActions;case 1:*n=m->exitStepCount;return m->exitSteps;case 2:*n=m->exitPlanCount;return m->exitPlans;case 3:*n=m->valueInitializerCount;return m->valueInitializers;default:*n=0;return NULL;}}
static void setControlRows(XL6_IrModule* m,size_t t,void* p,size_t n)
{switch(t){case 0:m->cleanupActions=p;m->cleanupActionCount=m->cleanupActionCapacity=n;break;case 1:m->exitSteps=p;m->exitStepCount=m->exitStepCapacity=n;break;case 2:m->exitPlans=p;m->exitPlanCount=m->exitPlanCapacity=n;break;case 3:m->valueInitializers=p;m->valueInitializerCount=m->valueInitializerCapacity=n;break;}}
static void controlOriginal(size_t t,void* p)
{
 switch(t){
 case 0:*(XL6_IrCleanupAction*)p=(XL6_IrCleanupAction){.kind=XL6_IR_CLEANUP_END_STORAGE,.type=1,.targetSymbol=2,.ownerDeclaration=3,.calleeSymbol=4,.origin=5};break;
 case 1:*(XL6_IrExitStep*)p=(XL6_IrExitStep){.kind=XL6_IR_EXIT_STEP_SCOPE,.origin=1,.firstCleanup=0,.cleanupCount=2,.loopTargetCount=17,.failureTarget=2,.failureScopeDepth=19,.failureRegion=3};break;
 case 2:*(XL6_IrExitPlan*)p=(XL6_IrExitPlan){.origin=1,.point=XL6_IR_EXIT_POINT_AFTER_SCOPE,.kind=XL6_SEM_EXIT_RETURN,.targetScopeDepth=23,.targetRegion=2,.firstCleanup=0,.cleanupCount=2,.firstStep=0,.stepCount=2};break;
 case 3:*(XL6_IrValueInitializer*)p=(XL6_IrValueInitializer){.declaration=1,.type=2,.expression=3,.abi=XL6_IR_INITIALIZER_EXPLICIT,.scalarConversionAbi=1,.runtimeOp=1,.runtimeResourceOp=1,.payloadType=4,.runtimeResourcePayloadAbi=1,.runtimeResourceKeyType=5,.runtimeResourceKeyPayloadAbi=1,.runtimeResourceKeyOrderAbi=1,.callableErasureAbi=1,.errorAbi=1};break;
 }
}
static void controlExpected(size_t t,void* p)
{
 switch(t){
 case 0:{XL6_IrCleanupAction* r=p;r->type+=20;r->targetSymbol+=30;r->ownerDeclaration+=10;r->calleeSymbol+=30;r->origin+=10;break;}
 case 1:{XL6_IrExitStep* r=p;r->origin+=10;r->firstCleanup++;r->failureTarget+=10;r->failureRegion+=40;break;}
 case 2:{XL6_IrExitPlan* r=p;r->origin+=10;r->targetRegion+=40;r->firstCleanup++;r->firstStep++;break;}
 case 3:{XL6_IrValueInitializer* r=p;r->declaration+=10;r->type+=20;r->expression+=10;r->payloadType+=20;r->runtimeResourceKeyType+=20;break;}
 }
}
typedef struct control_map {int shift,bad,mode;size_t calls[XLS_RECORD_DOMAIN_COUNT];}control_map;
static int controlMapped(void* user,xls_ir_record_domain d,uint64_t from,uint64_t* to)
{
 control_map* m=user;++m->calls[d];
 if((int)d==m->bad){if(m->mode==2){*to=from+1+(from==1?1:0);return 1;}if(!m->mode)return 0;*to=UINT64_MAX;return 1;}
 uint64_t shift=d>=XLS_RECORD_CLEANUP&&d<=XLS_RECORD_INITIALIZER?1:d==XLS_RECORD_NODE?10:d==XLS_RECORD_TYPE?20:d==XLS_RECORD_SYMBOL?30:d==XLS_RECORD_REGION?40:0;*to=from+(m->shift?shift:0);return 1;
}
typedef struct control_fixture {XL6_IrModule current;xls_ir_record_binding bindings[12];control_map old,fresh;void* arrays[4];}control_fixture;
static int controlFixture(control_fixture* f)
{
 memset(f,0,sizeof(*f));f->old=(control_map){.shift=1,.bad=-1};f->fresh=(control_map){.bad=-1};f->current.nodeCount=f->current.types.count=128;
 for(size_t t=0;t<4;++t){f->arrays[t]=xrtCalloc(3,controlWidths[t]);CHECK(f->arrays[t]);setControlRows(&f->current,t,f->arrays[t],3);
  for(size_t i=0;i<3;++i){void* p=(char*)f->arrays[t]+i*controlWidths[t];if(i)controlOriginal(t,p);else if(t==1)((XL6_IrExitStep*)p)->firstCleanup=SIZE_MAX;else if(t==2){((XL6_IrExitPlan*)p)->firstCleanup=SIZE_MAX;((XL6_IrExitPlan*)p)->firstStep=SIZE_MAX;}
   xls_ir_table_capture_input input={.table=xlsIrTableSchemaFind(controlNames[t]),.first=i?i-1:0,.count=1,.rows=p};xls_ir_graph_status status;xls_ir_table_unit* unit=xlsIrTableUnitCapture(&input,NULL,SIZE_MAX,NULL,NULL,&status,NULL);CHECK(unit);
   f->bindings[t*3+i]=(xls_ir_record_binding){unit,xlsIrTableUnitProducer(unit),controlMapped,i?&f->old:&f->fresh};if(i)controlExpected(t,p);else if(t==1)((XL6_IrExitStep*)p)->firstCleanup=3;else if(t==2){((XL6_IrExitPlan*)p)->firstCleanup=3;((XL6_IrExitPlan*)p)->firstStep=3;}
  }
 }return 1;
}
static void controlFixtureFree(control_fixture* f)
{for(size_t i=0;i<12;++i)xlsIrTableUnitRelease(f->bindings[i].unit);for(size_t i=0;i<4;++i)xrtFree(f->arrays[i]);}
static int controlBook(xstrbuf* book,const XL6_IrModule* m)
{for(size_t t=0;t<4;++t){size_t n;const char* p=controlRows((XL6_IrModule*)m,t,&n);for(size_t i=0;i<n;++i){const void* row=p+i*controlWidths[t];int okay=t==0?book_XL6_IrCleanupAction(book,row):t==1?book_XL6_IrExitStep(book,row):t==2?book_XL6_IrExitPlan(book,row):book_XL6_IrValueInitializer(book,row);if(!okay)return 0;}}return 1;}
typedef struct control_field {size_t table,offset,width;}control_field;
#define CONTROL_FIELD(t,ty,f) {t,offsetof(ty,f),sizeof(((ty*)0)->f)}
static const control_field controlFields[]={
CONTROL_FIELD(0,XL6_IrCleanupAction,kind),CONTROL_FIELD(0,XL6_IrCleanupAction,type),CONTROL_FIELD(0,XL6_IrCleanupAction,targetSymbol),CONTROL_FIELD(0,XL6_IrCleanupAction,ownerDeclaration),CONTROL_FIELD(0,XL6_IrCleanupAction,calleeSymbol),CONTROL_FIELD(0,XL6_IrCleanupAction,origin),
CONTROL_FIELD(1,XL6_IrExitStep,kind),CONTROL_FIELD(1,XL6_IrExitStep,origin),CONTROL_FIELD(1,XL6_IrExitStep,firstCleanup),CONTROL_FIELD(1,XL6_IrExitStep,cleanupCount),CONTROL_FIELD(1,XL6_IrExitStep,loopTargetCount),CONTROL_FIELD(1,XL6_IrExitStep,failureTarget),CONTROL_FIELD(1,XL6_IrExitStep,failureScopeDepth),CONTROL_FIELD(1,XL6_IrExitStep,failureRegion),
CONTROL_FIELD(2,XL6_IrExitPlan,origin),CONTROL_FIELD(2,XL6_IrExitPlan,point),CONTROL_FIELD(2,XL6_IrExitPlan,kind),CONTROL_FIELD(2,XL6_IrExitPlan,targetScopeDepth),CONTROL_FIELD(2,XL6_IrExitPlan,targetRegion),CONTROL_FIELD(2,XL6_IrExitPlan,firstCleanup),CONTROL_FIELD(2,XL6_IrExitPlan,cleanupCount),CONTROL_FIELD(2,XL6_IrExitPlan,firstStep),CONTROL_FIELD(2,XL6_IrExitPlan,stepCount),
CONTROL_FIELD(3,XL6_IrValueInitializer,declaration),CONTROL_FIELD(3,XL6_IrValueInitializer,type),CONTROL_FIELD(3,XL6_IrValueInitializer,expression),CONTROL_FIELD(3,XL6_IrValueInitializer,abi),CONTROL_FIELD(3,XL6_IrValueInitializer,scalarConversionAbi),CONTROL_FIELD(3,XL6_IrValueInitializer,runtimeOp),CONTROL_FIELD(3,XL6_IrValueInitializer,runtimeResourceOp),CONTROL_FIELD(3,XL6_IrValueInitializer,payloadType),CONTROL_FIELD(3,XL6_IrValueInitializer,runtimeResourcePayloadAbi),CONTROL_FIELD(3,XL6_IrValueInitializer,runtimeResourceKeyType),CONTROL_FIELD(3,XL6_IrValueInitializer,runtimeResourceKeyPayloadAbi),CONTROL_FIELD(3,XL6_IrValueInitializer,runtimeResourceKeyOrderAbi),CONTROL_FIELD(3,XL6_IrValueInitializer,callableErasureAbi),CONTROL_FIELD(3,XL6_IrValueInitializer,errorAbi),
};
#undef CONTROL_FIELD
static int controlFailures(void)
{
 control_fixture f;CHECK(controlFixture(&f));xls_ir_graph_status status;xls_ir_record_error error;
 faultActive=1;faultAt=faultCalls=denied=peakBytes=0;xls_ir_control_link* p=xlsIrControlLink(&f.current,f.bindings,12,SIZE_MAX,NULL,NULL,&status,&error);faultActive=0;
 CHECK(p&&status==XLS_IR_GRAPH_OK&&faultCalls==2&&live==1&&!overflow);size_t peak=xlsIrRecordLinkPeakBytes(p);CHECK(peak==peakBytes&&xlsIrRecordLinkBytes(p)==liveBytes);xlsIrControlLinkDestroy(p);CHECK(!live);
 p=xlsIrControlLink(&f.current,f.bindings,12,peak,NULL,NULL,&status,&error);CHECK(p);xlsIrControlLinkDestroy(p);CHECK(!xlsIrControlLink(&f.current,f.bindings,12,peak-1,NULL,NULL,&status,&error)&&status==XLS_IR_GRAPH_BUDGET);
 for(size_t at=1;at<=2;++at){faultActive=1;faultAt=at;faultCalls=denied=0;p=xlsIrControlLink(&f.current,f.bindings,12,SIZE_MAX,NULL,NULL,&status,&error);faultActive=0;CHECK(!p&&status==XLS_IR_GRAPH_MEMORY&&denied==1&&!live);}
 for(size_t at=1;;++at){call_cancel cancel={.at=at};p=xlsIrControlLink(&f.current,f.bindings,12,SIZE_MAX,callCancelled,&cancel,&status,&error);if(p){xlsIrControlLinkDestroy(p);break;}CHECK(status==XLS_IR_GRAPH_CANCELLED);}
 for(size_t d=0;d<XLS_RECORD_DOMAIN_COUNT;++d)if(f.old.calls[d]){for(int mode=0;mode<2;++mode){f.old.bad=(int)d;f.old.mode=mode;CHECK(!xlsIrControlLink(&f.current,f.bindings,12,SIZE_MAX,NULL,NULL,&status,&error)&&status==XLS_IR_GRAPH_INVALID&&error.reason==(mode?XLS_RECORD_RANGE:XLS_RECORD_UNMAPPED));}}
 f.old.bad=-1;
 f.old.bad=XLS_RECORD_CLEANUP;f.old.mode=2;CHECK(!xlsIrControlLink(&f.current,f.bindings,12,SIZE_MAX,NULL,NULL,&status,&error)&&error.reason==XLS_RECORD_RANGE);f.old.bad=-1;
 const void* producer=f.bindings[0].producer;f.bindings[0].producer=&f;CHECK(!xlsIrControlLink(&f.current,f.bindings,12,SIZE_MAX,NULL,NULL,&status,&error)&&error.reason==XLS_RECORD_NAMESPACE);f.bindings[0].producer=producer;
 xls_ir_record_binding saved=f.bindings[2];f.bindings[2]=f.bindings[1];CHECK(!xlsIrControlLink(&f.current,f.bindings,12,SIZE_MAX,NULL,NULL,&status,&error)&&error.reason==XLS_RECORD_DUPLICATE);f.bindings[2]=saved;
 f.current.exitSteps[1].loopTargetCount++;CHECK(!xlsIrControlLink(&f.current,f.bindings,12,SIZE_MAX,NULL,NULL,&status,&error)&&error.reason==XLS_RECORD_DIFFERENT);f.current.exitSteps[1].loopTargetCount--;
 f.current.exitPlans[1].targetScopeDepth++;CHECK(!xlsIrControlLink(&f.current,f.bindings,12,SIZE_MAX,NULL,NULL,&status,&error)&&error.reason==XLS_RECORD_DIFFERENT);f.current.exitPlans[1].targetScopeDepth--;
 for(size_t i=0;i<sizeof(controlFields)/sizeof(*controlFields);++i){const control_field* field=&controlFields[i];unsigned char* target=(unsigned char*)f.arrays[field->table]+controlWidths[field->table]+field->offset;unsigned char saved=*target;*target^=1;
  CHECK(!xlsIrControlLink(&f.current,f.bindings,12,SIZE_MAX,NULL,NULL,&status,&error)&&status==XLS_IR_GRAPH_INVALID);*target=saved;
 }
 XL6_IrModule empty={0};CHECK(!xlsIrRecordLink(&empty,NULL,0,0,SIZE_MAX,NULL,NULL,&status,&error)&&status==XLS_IR_GRAPH_INVALID);CHECK(!xlsIrRecordLink(&empty,NULL,0,8,SIZE_MAX,NULL,NULL,&status,&error)&&status==XLS_IR_GRAPH_INVALID);
 CHECK(!xlsIrControlLink(NULL,NULL,0,SIZE_MAX,NULL,NULL,&status,&error));CHECK(!xlsIrControlLink(&f.current,NULL,12,SIZE_MAX,NULL,NULL,&status,&error));CHECK(!xlsIrControlLinkApply(NULL,&empty)&&!xlsIrCallLinkApply(NULL,&empty)&&!xlsIrRecordLinkApply(NULL,&empty));
 xstrbuf expected={0},actual={0};CHECK(controlBook(&expected,&f.current));p=xlsIrControlLink(&f.current,f.bindings,12,SIZE_MAX,NULL,NULL,&status,&error);CHECK(p);XL6_IrModule view={0};view.callSiteCount=21;view.callSites=(void*)&view;
 CHECK(!xlsIrCallLinkApply(p,&view)&&xlsIrControlLinkApply(p,&view)&&view.callSiteCount==21&&view.callSites==(void*)&view);controlFixtureFree(&f);CHECK(controlBook(&actual,&view)&&tableBooksEqual(&expected,&actual));xrtStrBufFree(&expected);xrtStrBufFree(&actual);xlsIrControlLinkDestroy(p);
 p=xlsIrControlLink(&empty,NULL,0,SIZE_MAX,NULL,NULL,&status,&error);CHECK(p&&xlsIrControlLinkApply(p,&empty));xlsIrControlLinkDestroy(p);return 1;
}
static int controlGaps(void)
{
 control_fixture f;CHECK(controlFixture(&f));void* rows=xrtCalloc(4,controlWidths[0]);CHECK(rows);memcpy(rows,f.arrays[0],2*controlWidths[0]);memcpy((char*)rows+3*controlWidths[0],(char*)f.arrays[0]+2*controlWidths[0],controlWidths[0]);xrtFree(f.arrays[0]);f.arrays[0]=rows;setControlRows(&f.current,0,rows,4);
 xls_ir_record_binding bindings[13];memcpy(bindings,f.bindings,sizeof(f.bindings));xls_ir_table_capture_input input={.table=xlsIrTableSchemaFind("cleanupActions"),.first=2,.count=1,.rows=(char*)rows+2*controlWidths[0]};xls_ir_graph_status status;xls_ir_table_unit* unit=xlsIrTableUnitCapture(&input,NULL,SIZE_MAX,NULL,NULL,&status,NULL);CHECK(unit);bindings[12]=(xls_ir_record_binding){unit,xlsIrTableUnitProducer(unit),callIdentity,NULL};f.old.bad=XLS_RECORD_CLEANUP;f.old.mode=2;
 xls_ir_record_error error;xls_ir_control_link* p=xlsIrControlLink(&f.current,bindings,13,SIZE_MAX,NULL,NULL,&status,&error);CHECK(!p&&status==XLS_IR_GRAPH_INVALID&&error.reason==XLS_RECORD_RANGE);xlsIrTableUnitRelease(unit);controlFixtureFree(&f);return 1;
}
static int controlCombined(void)
{
 call_fixture calls;control_fixture f;CHECK(callFixture(&calls)&&controlFixture(&f));XL6_IrModule combined=calls.current;for(size_t t=0;t<4;++t){size_t n;void* p=controlRows(&f.current,t,&n);setControlRows(&combined,t,p,n);}xls_ir_record_binding bindings[36];memcpy(bindings,calls.bindings,sizeof(calls.bindings));memcpy(bindings+24,f.bindings,sizeof(f.bindings));
 xstrbuf expected={0},actual={0};CHECK(callBook(&expected,&combined)&&controlBook(&expected,&combined));xls_ir_graph_status status;xls_ir_record_link* p=xlsIrRecordLink(&combined,bindings,36,XLS_RECORD_ALL,SIZE_MAX,NULL,NULL,&status,NULL);CHECK(p&&xlsIrRecordLinkFamilies(p)==XLS_RECORD_ALL);XL6_IrModule view={0};CHECK(!xlsIrControlLinkApply(p,&view)&&!xlsIrCallLinkApply(p,&view)&&xlsIrRecordLinkApply(p,&view));callFixtureFree(&calls);controlFixtureFree(&f);CHECK(callBook(&actual,&view)&&controlBook(&actual,&view)&&tableBooksEqual(&expected,&actual));xrtStrBufFree(&expected);xrtStrBufFree(&actual);xlsIrRecordLinkDestroy(p);return 1;
}
static xls_ir_record_link* controlNativeLink(const XL6_IrModule* source,unsigned families)
{
 size_t total=0;for(size_t t=0;t<15;++t)if((t<8?XLS_RECORD_CALLS:t<12?XLS_RECORD_CONTROL:XLS_RECORD_ENUMS)&families){size_t n;if(t<8)callRows((XL6_IrModule*)source,t,&n);else if(t<12)controlRows((XL6_IrModule*)source,t-8,&n);else enumRecordRows((XL6_IrModule*)source,t-12,&n);total+=n;}
 xls_ir_record_binding* bindings=xrtCalloc(total?total:1,sizeof(*bindings));if(!bindings)return NULL;size_t used=0;xls_ir_graph_status status;
 for(size_t t=0;t<15;++t)if((t<8?XLS_RECORD_CALLS:t<12?XLS_RECORD_CONTROL:XLS_RECORD_ENUMS)&families){size_t n;char* rows=t<8?callRows((XL6_IrModule*)source,t,&n):t<12?controlRows((XL6_IrModule*)source,t-8,&n):enumRecordRows((XL6_IrModule*)source,t-12,&n);size_t width=t<8?callWidths[t]:t<12?controlWidths[t-8]:enumRecordWidths[t-12];const char* name=t<8?callNames[t]:t<12?controlNames[t-8]:enumRecordNames[t-12];
  for(size_t i=0;i<n;++i){xls_ir_table_capture_input input={.table=xlsIrTableSchemaFind(name),.first=i,.count=1,.rows=rows+i*width};xls_ir_table_unit* unit=xlsIrTableUnitCapture(&input,NULL,SIZE_MAX,NULL,NULL,&status,NULL);if(!unit)goto failed;bindings[used++]=(xls_ir_record_binding){unit,xlsIrTableUnitProducer(unit),callIdentity,NULL};}}
 {xls_ir_record_error error;xls_ir_record_link* result=xlsIrRecordLink(source,bindings,total,families,SIZE_MAX,NULL,NULL,&status,&error);for(size_t i=0;i<used;++i)xlsIrTableUnitRelease(bindings[i].unit);xrtFree(bindings);return result;}
failed:for(size_t i=0;i<used;++i)xlsIrTableUnitRelease(bindings[i].unit);xrtFree(bindings);return NULL;
}
static int controlOracle(const XL6_IrModule* source)
{for(unsigned family=XLS_RECORD_CONTROL;family<=XLS_RECORD_ALL;++family){capture_view base;CHECK(captureView(source,&base));xls_ir_record_link* link=controlNativeLink(source,family);CHECK(link&&xlsIrRecordLinkApply(link,&base.module));char* expected=captureDump(source);CHECK(expected&&verifyCapture(&base.module,expected));xrtFree(expected);xlsIrRecordLinkDestroy(link);captureViewFree(&base);}return 1;}
void __wrap_xl6IrDumpModuleJson(XL6_Output* out,const XL6_IrModule* module)
{if(xrtAtomic32Load(&oracleActive,XMEMORY_ACQUIRE)&&!oracleRecursing){oracleRecursing=1;xrtAtomic32FetchAdd(&oracleRuns,1,XMEMORY_RELAXED);if(!controlOracle(module))xrtAtomic32FetchAdd(&oracleErrors,1,XMEMORY_RELAXED);oracleRecursing=0;}__real_xl6IrDumpModuleJson(out,module);}
typedef struct native_control_map {native_call_map common;uint64_t* rows[4];}native_control_map;
typedef struct native_control_row {native_control_map* map;size_t table,from,to;}native_control_row;
static int controlNativeMapped(void* user,xls_ir_record_domain d,uint64_t value,uint64_t* out)
{
 native_control_row* row=user;native_control_map* map=row->map;
 if(d>=XLS_RECORD_CLEANUP&&d<=XLS_RECORD_INITIALIZER){size_t t=d-XLS_RECORD_CLEANUP,n;controlRows((XL6_IrModule*)map->common.old,t,&n);if(value>=n||map->rows[t][value]==UINT64_MAX)return 0;*out=map->rows[t][value];return 1;}
 if(d==XLS_RECORD_REGION){if(row->table==1){const XL6_IrExitStep* a=&map->common.old->exitSteps[row->from],*b=&map->common.current->exitSteps[row->to];if(value!=a->failureRegion)return 0;*out=b->failureRegion;return *out!=0;}if(row->table==2){const XL6_IrExitPlan* a=&map->common.old->exitPlans[row->from],*b=&map->common.current->exitPlans[row->to];if(value!=a->targetRegion)return 0;*out=b->targetRegion;return *out!=0;}return 0;}
 return nativeMapped(&map->common,d,value,out);
}
static int assignControlRange(native_control_map* map,size_t t,size_t from,size_t to,size_t count)
{size_t n,m;controlRows((XL6_IrModule*)map->common.old,t,&n);controlRows((XL6_IrModule*)map->common.current,t,&m);CHECK(from<=n&&count<=n-from&&to<=m&&count<=m-to);for(size_t i=0;i<count;++i){CHECK(map->rows[t][from+i]==UINT64_MAX||map->rows[t][from+i]==to+i);map->rows[t][from+i]=to+i;}return 1;}
static int mixedNativeControl(void)
{
 const char* text="func guarded(int count) as int\n string outer=\"outer\"\n while count>0\n string iteration=\"loop\"\n count=count-1\n if count>5\n continue\n end\n break\n end\n try\n string inner=\"inner\"\n return 7\n catch e\n string message=e.message\n return 8\n finally\n string final=\"final\"\n end\nend\nfunc main(...) as int\n return guarded(7)\nend\n";
 const char* prefix="func added(int x) as int\n string added=\"new\"\n return x+1\nend\n";
 xstrbuf changed={0};CHECK(xrtStrBufAppend(&changed,xrtStrView(prefix))&&xrtStrBufAppend(&changed,xrtStrView(text)));char* path=xrtPathAbs("ir-control-mixed.xl");CHECK(path);xls_semantic_check* old=checked(path,text);xls_semantic_check* current=checked(path,changed.Data);CHECK(old&&current&&xlsSemanticCheckInfo(old).accepted&&xlsSemanticCheckInfo(current).accepted);
 const XL6_IrModule* a=xlsSemanticCheckIr(old);const XL6_IrModule* b=xlsSemanticCheckIr(current);CHECK(a&&b);native_control_map map={.common={.old=a,.current=b}};native_call_map* common=&map.common;common->nodes=xrtCalloc(a->nodeCount+1,sizeof(uint64_t));common->types=xrtCalloc(a->types.count+1,sizeof(uint64_t));common->symbols=xrtCalloc(a->symbolNodeCapacity,sizeof(uint64_t));CHECK(common->nodes&&common->types&&common->symbols);
 for(size_t i=0;i<a->types.count;++i){const XL6_IrType* x=&a->types.items[i];size_t matches=0;for(size_t j=0;j<b->types.count;++j){const XL6_IrType* y=&b->types.items[j];if(x->kind==y->kind&&x->paramCount==y->paramCount&&x->returnCount==y->returnCount&&((!x->name&&!y->name)||(x->name&&y->name&&!strcmp(x->name,y->name)))&&((!x->displayName&&!y->displayName)||(x->displayName&&y->displayName&&!strcmp(x->displayName,y->displayName)))){common->types[i+1]=j+1;++matches;}}CHECK(matches==1);}
 common->nodes[a->root]=b->root;const XL6_IrNode* ar=&a->nodes[a->root-1];const XL6_IrNode* br=&b->nodes[b->root-1];CHECK(br->childCount==ar->childCount+1);for(size_t i=0;i<ar->childCount;++i)CHECK(nativeTreeMap(common,a->children[ar->firstChild+i],b->children[br->firstChild+i+1],0));
 for(size_t i=1;i<a->symbolNodeCapacity;++i){if(!a->symbolNodes[i]){common->symbols[i]=i;continue;}XL6_IrNodeId n=(XL6_IrNodeId)common->nodes[a->symbolNodes[i]];for(size_t j=1;j<b->symbolNodeCapacity;++j)if(b->symbolNodes[j]==n){CHECK(!common->symbols[i]);common->symbols[i]=j;}CHECK(common->symbols[i]);}
 size_t total=0;for(size_t t=0;t<4;++t){size_t n,m;controlRows((XL6_IrModule*)a,t,&n);controlRows((XL6_IrModule*)b,t,&m);total+=m;map.rows[t]=xrtMalloc((n?n:1)*sizeof(uint64_t));CHECK(map.rows[t]);for(size_t i=0;i<n;++i)map.rows[t][i]=UINT64_MAX;}
 for(size_t i=0;i<a->exitPlanCount;++i){const XL6_IrExitPlan* x=&a->exitPlans[i];for(size_t j=0;j<b->exitPlanCount;++j){const XL6_IrExitPlan* y=&b->exitPlans[j];if(common->nodes[x->origin]==y->origin&&x->point==y->point&&x->kind==y->kind){CHECK(map.rows[2][i]==UINT64_MAX&&x->targetScopeDepth==y->targetScopeDepth&&x->cleanupCount==y->cleanupCount&&x->stepCount==y->stepCount);map.rows[2][i]=j;CHECK(assignControlRange(&map,0,x->firstCleanup,y->firstCleanup,x->cleanupCount)&&assignControlRange(&map,1,x->firstStep,y->firstStep,x->stepCount));}}CHECK(map.rows[2][i]!=UINT64_MAX);}
 for(size_t i=0;i<a->exitStepCount;++i){CHECK(map.rows[1][i]!=UINT64_MAX);const XL6_IrExitStep* x=&a->exitSteps[i];const XL6_IrExitStep* y=&b->exitSteps[map.rows[1][i]];CHECK(x->kind==y->kind&&x->cleanupCount==y->cleanupCount&&assignControlRange(&map,0,x->firstCleanup,y->firstCleanup,x->cleanupCount));}
 for(size_t i=0;i<a->valueInitializerCount;++i){for(size_t j=0;j<b->valueInitializerCount;++j)if(common->nodes[a->valueInitializers[i].declaration]==b->valueInitializers[j].declaration){CHECK(map.rows[3][i]==UINT64_MAX);map.rows[3][i]=j;}CHECK(map.rows[3][i]!=UINT64_MAX);}
 xls_ir_record_binding* bindings=xrtCalloc(total,sizeof(*bindings));native_control_row* contexts=xrtCalloc(total,sizeof(*contexts));CHECK(bindings&&contexts);xls_ir_graph_status status;size_t used=0,reused=0,fresh=0;
 for(size_t t=0;t<4;++t){size_t n,m;char* from=controlRows((XL6_IrModule*)a,t,&n);char* to=controlRows((XL6_IrModule*)b,t,&m);for(size_t i=0;i<n;++i)CHECK(map.rows[t][i]!=UINT64_MAX);
  for(size_t j=0;j<m;++j){size_t selected=SIZE_MAX;for(size_t i=0;i<n;++i)if(map.rows[t][i]==j){CHECK(selected==SIZE_MAX);selected=i;}
   xls_ir_table_capture_input input={.table=xlsIrTableSchemaFind(controlNames[t]),.first=selected==SIZE_MAX?j:selected,.count=1,.rows=selected==SIZE_MAX?to+j*controlWidths[t]:from+selected*controlWidths[t]};xls_ir_table_unit* unit=xlsIrTableUnitCapture(&input,NULL,SIZE_MAX,NULL,NULL,&status,NULL);CHECK(unit);contexts[used]=(native_control_row){&map,t,selected,j};bindings[used]=(xls_ir_record_binding){unit,xlsIrTableUnitProducer(unit),selected==SIZE_MAX?callIdentity:controlNativeMapped,selected==SIZE_MAX?NULL:contexts+used};++used;if(selected==SIZE_MAX)++fresh;else ++reused;
  }
 }
 xls_ir_record_error error;xls_ir_control_link* link=xlsIrControlLink(b,bindings,total,SIZE_MAX,NULL,NULL,&status,&error);if(!link)fprintf(stderr,"mixed control link reason=%d binding=%zu domain=%d field=%s value=%llu\n",error.reason,error.binding,error.domain,error.field?error.field:"",(unsigned long long)error.value);CHECK(link&&reused>8&&fresh>0);
 size_t movedNodes=0,movedRegions=0;for(size_t i=1;i<=a->nodeCount;++i)movedNodes+=common->nodes[i]!=i;for(size_t i=0;i<a->exitPlanCount;++i)movedRegions+=a->exitPlans[i].targetRegion!=b->exitPlans[map.rows[2][i]].targetRegion;CHECK(movedNodes>8);capture_view base;CHECK(captureView(b,&base)&&xlsIrControlLinkApply(link,&base.module));char* expected=captureDump(b);CHECK(expected);
 for(size_t i=0;i<total;++i){xlsIrTableUnitRelease(bindings[i].unit);}xrtFree(bindings);xrtFree(contexts);for(size_t t=0;t<4;++t){xrtFree(map.rows[t]);}xrtFree(common->nodes);xrtFree(common->types);xrtFree(common->symbols);xlsSemanticCheckDestroy(old);xlsSemanticCheckDestroy(current);xrtStrBufFree(&changed);xrtFree(path);
 CHECK(verifyCapture(&base.module,expected));xrtFree(expected);xlsIrControlLinkDestroy(link);captureViewFree(&base);printf("mixed native control reused=%zu rebuilt=%zu movedNodes=%zu movedRegions=%zu;source models and maps retired\n",reused,fresh,movedNodes,movedRegions);return 1;
}
typedef struct control_reader {const control_fixture* fixture;const xstrbuf* expected;int okay;}control_reader;
static int32 controlRead(void* user)
{
 control_reader* r=user;r->okay=1;for(size_t i=0;i<16;++i){xls_ir_record_binding bindings[12];memcpy(bindings,r->fixture->bindings,sizeof(bindings));control_map old={.shift=1,.bad=-1},fresh={.bad=-1};for(size_t j=0;j<12;++j)bindings[j].user=j%3?&old:&fresh;xls_ir_graph_status status;xls_ir_control_link* p=xlsIrControlLink(&r->fixture->current,bindings,12,SIZE_MAX,NULL,NULL,&status,NULL);XL6_IrModule view={0};xstrbuf book={0};int okay=p&&xlsIrControlLinkApply(p,&view)&&controlBook(&book,&view)&&tableBooksEqual(&book,r->expected);xrtStrBufFree(&book);xlsIrControlLinkDestroy(p);if(!okay){r->okay=0;break;}}return r->okay?0:1;
}
static int controlLifetimes(void)
{
 control_fixture f;CHECK(controlFixture(&f));xstrbuf expected={0};CHECK(controlBook(&expected,&f.current));control_reader readers[4];xthread* threads[4];for(size_t i=0;i<4;++i){readers[i]=(control_reader){&f,&expected,0};threads[i]=xrtThreadCreate(controlRead,readers+i,0);CHECK(threads[i]);}for(size_t i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK&&readers[i].okay);xrtThreadDestroy(threads[i]);}controlFixtureFree(&f);
 for(size_t i=0;i<32;++i){faultActive=1;faultAt=faultCalls=denied=0;CHECK(controlFixture(&f));size_t baseline=live;CHECK(baseline==28);xls_ir_graph_status status;xls_ir_control_link* p=xlsIrControlLink(&f.current,f.bindings,12,SIZE_MAX,NULL,NULL,&status,NULL);CHECK(p);XL6_IrModule view={0};CHECK(xlsIrControlLinkApply(p,&view));controlFixtureFree(&f);CHECK(live==25);faultActive=0;xstrbuf book={0};CHECK(controlBook(&book,&view)&&tableBooksEqual(&book,&expected));xrtStrBufFree(&book);xlsIrControlLinkDestroy(p);CHECK(!live&&!liveBytes&&!overflow);}xrtStrBufFree(&expected);return 1;
}

static int controlTests(void)
{
 xrtAtomic32Init(&checks,0);xrtAtomic32Init(&oracleRuns,0);xrtAtomic32Init(&oracleErrors,0);xrtAtomic32Init(&oracleActive,0);CHECK(controlFailures()&&controlGaps()&&controlCombined()&&controlLifetimes()&&mixedNativeControl());
 char* path=xrtPathAbs("ir-control-link-中文.xl");CHECK(path);xrtAtomic32Store(&oracleActive,1,XMEMORY_RELEASE);int okay=corpus(path)&&regionCorpus(path);xrtAtomic32Store(&oracleActive,0,XMEMORY_RELEASE);CHECK(okay&&!xrtAtomic32Load(&oracleErrors,XMEMORY_RELAXED)&&xrtAtomic32Load(&oracleRuns,XMEMORY_RELAXED)==134);xrtFree(path);
 printf("analysis_ir_control_link_test passed: %u checks;%u native modules,six CONTROL/ENUMS/CALLS combinations,complete verifier/JSON oracles\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED),xrtAtomic32Load(&oracleRuns,XMEMORY_RELAXED));return 1;
}
#ifdef XLS_IR_CONTROL_LINK_LIBRARY
int xlsIrControlLinkFixtureMain(void){return controlTests()?0:1;}
#else
int main(void){return controlTests()?0:1;}
#endif
