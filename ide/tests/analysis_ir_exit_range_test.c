#define XLS_IR_UNIT_CAPTURE_LIBRARY 1
#define __wrap_xl6IrDumpModuleJson rangePriorDumpWrapper
#include "analysis_ir_unit_capture_test.c"
#undef __wrap_xl6IrDumpModuleJson
#include "analysis/ir_exit_range.h"
void* __real_xrtRealloc(void*,size_t);
void* __wrap_xrtRealloc(void* p,size_t n)
{
 if(refuse())return NULL;
 void* q=__real_xrtRealloc(p,n);if(q){forget(p);remember(q,n);}return q;
}
/* Independent assembly oracle: preserve native source order and translate
 * only local output slices. Node/type/symbol/region IDs must not change. */
static int rangeRouted(const XL6_IrExitPlan* p)
{return p->kind==XL6_SEM_EXIT_THROW ||
 ((p->point==XL6_IR_EXIT_POINT_BEFORE_NODE || p->point==XL6_IR_EXIT_POINT_GENERATOR_CLOSE) &&
 (p->kind==XL6_SEM_EXIT_RETURN || p->kind==XL6_SEM_EXIT_BREAK || p->kind==XL6_SEM_EXIT_CONTINUE));}
static int rangeOracle(const XL6_IrModule* source,size_t stride)
{
 capture_view base;CHECK(captureView(source,&base));
 XL6_IrModule m=base.module;
 m.exitPlans=xrtCalloc(source->exitPlanCount+1,sizeof(*m.exitPlans));
 m.cleanupActions=xrtCalloc(source->cleanupActionCount+1,sizeof(*m.cleanupActions));
 m.exitSteps=xrtCalloc(source->exitStepCount+1,sizeof(*m.exitSteps));
 CHECK(m.exitPlans&&m.cleanupActions&&m.exitSteps);
 m.exitPlanCount=m.cleanupActionCount=m.exitStepCount=0;
 m.exitPlanCapacity=source->exitPlanCount+1;m.cleanupActionCapacity=source->cleanupActionCount+1;
 m.exitStepCapacity=source->exitStepCount+1;
 for(size_t first=0;first<source->nodeCount;){
  size_t n=source->nodeCount-first;if(n>stride)n=stride;
  xls_ir_exit_range_status status;xls_ir_exit_range_work work;
  xls_ir_exit_range* result=xlsIrExitRangeProduce(source,first,n,SIZE_MAX,NULL,NULL,&status,&work);
  CHECK(result&&status==XLS_EXIT_RANGE_OK&&work.closeNodes==n&&work.planNodes==n);
  xls_ir_exit_range_view v=xlsIrExitRangeView(result);CHECK(v.first==first&&v.count==n&&work.routePlans==v.planCount);
  CHECK(v.planCount<=source->exitPlanCount-m.exitPlanCount&&v.cleanupCount<=source->cleanupActionCount-m.cleanupActionCount&&v.stepCount<=source->exitStepCount-m.exitStepCount);
  for(size_t i=0;i<n;++i){CHECK(v.close[i]==source->nodes[first+i].generatorClosePointAbi);m.nodes[first+i].generatorClosePointAbi=v.close[i];}
  for(size_t i=0;i<v.planCount;++i){XL6_IrExitPlan p=v.plans[i];CHECK(p.origin>first&&p.origin-first<=n);
   p.firstCleanup+=m.cleanupActionCount;if(rangeRouted(&p))p.firstStep+=m.exitStepCount;
   m.exitPlans[m.exitPlanCount+i]=p;}
  for(size_t i=0;i<v.stepCount;++i){XL6_IrExitStep step=v.steps[i];if(step.kind==XL6_IR_EXIT_STEP_SCOPE)step.firstCleanup+=m.cleanupActionCount;m.exitSteps[m.exitStepCount+i]=step;}
  if(v.cleanupCount)memcpy(m.cleanupActions+m.cleanupActionCount,v.cleanups,v.cleanupCount*sizeof(*v.cleanups));
  m.exitPlanCount+=v.planCount;m.cleanupActionCount+=v.cleanupCount;m.exitStepCount+=v.stepCount;
  xlsIrExitRangeDestroy(result);first+=n;
 }
 CHECK(m.exitPlanCount==source->exitPlanCount&&m.cleanupActionCount==source->cleanupActionCount&&m.exitStepCount==source->exitStepCount);
 char* expected=captureDump(source);CHECK(expected&&verifyCapture(&m,expected));xrtFree(expected);
 xrtFree(m.exitPlans);xrtFree(m.cleanupActions);xrtFree(m.exitSteps);captureViewFree(&base);return 1;
}
void __wrap_xl6IrDumpModuleJson(XL6_Output* out,const XL6_IrModule* m)
{
 if(xrtAtomic32Load(&oracleActive,XMEMORY_ACQUIRE)&&!oracleRecursing){
  oracleRecursing=1;xrtAtomic32FetchAdd(&oracleRuns,1,XMEMORY_RELAXED);
  if(!rangeOracle(m,1)||!rangeOracle(m,17))xrtAtomic32FetchAdd(&oracleErrors,1,XMEMORY_RELAXED);
  oracleRecursing=0;
 }
 __real_xl6IrDumpModuleJson(out,m);
}
static int rangeFailures(void)
{
 const char* text="func guard(int x) as int\n string owner=\"outer\"\n try\n while x>0\n string inner=\"inner\"\n x=x-1\n if x>4\n continue\n end\n break\n end\n return x+1\n finally\n string final=\"final\"\n end\nend\nfunc main(...) as int\n return guard(5)\nend\n";
 char* path=xrtPathAbs("ir-exit-range-中文.xl");CHECK(path);xls_semantic_check* model=checked(path,text);
 CHECK(model&&xlsSemanticCheckInfo(model).accepted);const XL6_IrModule* m=xlsSemanticCheckIr(model);
 char* before=captureDump(m);CHECK(before);XL6_IrModule header=*m;
 XL6_IrNode* originalNodes=xrtMalloc(m->nodeCount*sizeof(*originalNodes));CHECK(originalNodes);
 memcpy(originalNodes,m->nodes,m->nodeCount*sizeof(*originalNodes));
 xls_ir_exit_range_status status;xls_ir_exit_range_work work;
 CHECK(!xlsIrExitRangeProduce(NULL,0,0,SIZE_MAX,NULL,NULL,&status,NULL)&&status==XLS_EXIT_RANGE_INPUT);
 CHECK(!xlsIrExitRangeProduce(m,SIZE_MAX,1,SIZE_MAX,NULL,NULL,&status,NULL)&&status==XLS_EXIT_RANGE_INPUT);
 CHECK(!xlsIrExitRangeProduce(m,1,SIZE_MAX,SIZE_MAX,NULL,NULL,&status,NULL)&&status==XLS_EXIT_RANGE_INPUT);
 CHECK(!xlsIrExitRangeProduce(m,0,m->nodeCount,0,NULL,NULL,&status,NULL)&&status==XLS_EXIT_RANGE_LIMIT);
 xls_ir_exit_range* empty=xlsIrExitRangeProduce(m,m->nodeCount,0,SIZE_MAX,NULL,NULL,&status,&work);
 CHECK(empty&&status==XLS_EXIT_RANGE_OK&&!work.planNodes&&!work.routePlans);xlsIrExitRangeDestroy(empty);
 faultActive=1;faultAt=faultCalls=denied=0;
 xls_ir_exit_range* result=xlsIrExitRangeProduce(m,0,m->nodeCount,SIZE_MAX,NULL,NULL,&status,&work);
 faultActive=0;CHECK(result&&status==XLS_EXIT_RANGE_OK&&!denied&&!overflow);
 size_t allocationsCount=faultCalls,retained=xlsIrExitRangeView(result).bytes;
 CHECK(liveBytes==retained&&retained>100);xlsIrExitRangeDestroy(result);CHECK(!live&&!liveBytes);
 for(size_t i=1;i<=allocationsCount;++i){
  faultActive=1;faultAt=i;faultCalls=denied=0;
  result=xlsIrExitRangeProduce(m,0,m->nodeCount,SIZE_MAX,NULL,NULL,&status,NULL);faultActive=0;
  CHECK(!result&&status!=XLS_EXIT_RANGE_OK&&denied==1&&!live&&!liveBytes&&!overflow);
 }
 capture_stop stop={0};result=xlsIrExitRangeProduce(m,0,m->nodeCount,SIZE_MAX,captureCancelled,&stop,&status,NULL);
 CHECK(result);size_t polls=stop.calls;xlsIrExitRangeDestroy(result);
 for(size_t i=1;i<=polls;++i){stop=(capture_stop){.at=i};faultActive=1;faultAt=faultCalls=denied=0;
  result=xlsIrExitRangeProduce(m,0,m->nodeCount,SIZE_MAX,captureCancelled,&stop,&status,&work);faultActive=0;
  CHECK(!result&&status==XLS_EXIT_RANGE_CANCELLED&&work.cancelled&&!live&&!liveBytes);
 }
 const size_t budgets[]={1,80,512,4096};
 for(size_t i=0;i<sizeof(budgets)/sizeof(*budgets);++i){faultActive=1;faultAt=faultCalls=denied=0;
  result=xlsIrExitRangeProduce(m,0,m->nodeCount,budgets[i],NULL,NULL,&status,&work);faultActive=0;
  CHECK(!result&&status==XLS_EXIT_RANGE_LIMIT&&work.limited&&!live&&!liveBytes);
 }
 result=xlsIrExitRangeProduce(m,0,m->nodeCount,retained,NULL,NULL,&status,NULL);CHECK(result);xlsIrExitRangeDestroy(result);
 CHECK(!xlsIrExitRangeProduce(m,0,m->nodeCount,retained-1,NULL,NULL,&status,NULL)&&status==XLS_EXIT_RANGE_LIMIT);
 CHECK(!memcmp(&header,m,sizeof(header))&&!memcmp(originalNodes,m->nodes,m->nodeCount*sizeof(*originalNodes)));
 xrtFree(originalNodes);char* after=captureDump(m);CHECK(after&&!strcmp(before,after));xrtFree(after);
 CHECK(rangeOracle(m,1)&&rangeOracle(m,17));
 /* Result arrays survive complete retirement of typed/AST/native owners. */
 result=xlsIrExitRangeProduce(m,0,m->nodeCount,SIZE_MAX,NULL,NULL,&status,NULL);CHECK(result);
 capture_view base;CHECK(captureView(m,&base));xlsSemanticCheckDestroy(model);xrtFree(path);
 xls_ir_exit_range_view v=xlsIrExitRangeView(result);base.module.exitPlans=(XL6_IrExitPlan*)v.plans;base.module.exitPlanCount=base.module.exitPlanCapacity=v.planCount;
 base.module.cleanupActions=(XL6_IrCleanupAction*)v.cleanups;base.module.cleanupActionCount=base.module.cleanupActionCapacity=v.cleanupCount;
 base.module.exitSteps=(XL6_IrExitStep*)v.steps;base.module.exitStepCount=base.module.exitStepCapacity=v.stepCount;
 CHECK(verifyCapture(&base.module,before));xrtFree(before);xlsIrExitRangeDestroy(result);captureViewFree(&base);
 printf("range producer: %zu allocation rejection sites, %zu cancellation points, retirement and exact budget boundary\n",allocationsCount,polls);return 1;
}
static int rangeSelectedFunction(void)
{
 xstrbuf text={0};char row[256];
 for(size_t i=0;i<256;++i){snprintf(row,sizeof(row),"func f%zu(int x) as int\n string name=\"value\"\n return x+%zu\nend\n",i,i);CHECK(xrtStrBufAppend(&text,xrtStrView(row)));}
 CHECK(xrtStrBufAppend(&text,xrtStrView("func main(...) as int\n return f128(2)\nend\n")));
 char* path=xrtPathAbs("ir-range-256-functions.xl");CHECK(path);xls_semantic_check* model=checked(path,text.Data);
 CHECK(model&&xlsSemanticCheckInfo(model).accepted);const XL6_IrModule* m=xlsSemanticCheckIr(model);
 size_t first=SIZE_MAX,end=m->nodeCount;
 for(size_t i=0;i<m->nodeCount;++i)if(m->nodes[i].text&&!strcmp(m->nodes[i].text,"f128")&&m->nodes[i].parent&&m->nodes[i].parent<=m->nodeCount&&m->nodes[m->nodes[i].parent-1].kind==XL6_IR_NODE_FUNC_DECL){CHECK(first==SIZE_MAX);first=m->nodes[i].parent-1;}
 CHECK(first!=SIZE_MAX&&m->nodes[first].parent==m->root);
 for(size_t i=first+1;i<m->nodeCount;++i)if(m->nodes[i].parent==m->root){end=i;break;}
 CHECK(end>first&&end-first<m->nodeCount/128);
 xls_ir_exit_range_work work;xls_ir_exit_range_status status;
 xls_ir_exit_range* result=xlsIrExitRangeProduce(m,first,end-first,SIZE_MAX,NULL,NULL,&status,&work);
 CHECK(result&&status==XLS_EXIT_RANGE_OK&&work.closeNodes==end-first&&work.planNodes==end-first);
 xls_ir_exit_range_view v=xlsIrExitRangeView(result);CHECK(v.planCount&&work.routePlans==v.planCount);
 size_t plan=0;while(plan<m->exitPlanCount&&m->exitPlans[plan].origin<=first)++plan;
 CHECK(plan<m->exitPlanCount);size_t clean=m->exitPlans[plan].firstCleanup,steps=SIZE_MAX;
 for(size_t i=0;i<v.planCount;++i)if(rangeRouted(&m->exitPlans[plan+i])){steps=m->exitPlans[plan+i].firstStep;break;}
 CHECK(steps!=SIZE_MAX&&clean>0&&steps>0);
 for(size_t i=0;i<v.planCount;++i){XL6_IrExitPlan p=m->exitPlans[plan+i];CHECK(p.origin>first&&p.origin<=end);p.firstCleanup-=clean;if(rangeRouted(&p))p.firstStep-=steps;
  xstrbuf a={0},b={0};CHECK(book_XL6_IrExitPlan(&a,&p)&&book_XL6_IrExitPlan(&b,&v.plans[i])&&tableBooksEqual(&a,&b));xrtStrBufFree(&a);xrtStrBufFree(&b);}
 CHECK(plan+v.planCount==m->exitPlanCount||m->exitPlans[plan+v.planCount].origin>end);
 for(size_t i=0;i<v.cleanupCount;++i){xstrbuf a={0},b={0};CHECK(book_XL6_IrCleanupAction(&a,&m->cleanupActions[clean+i])&&book_XL6_IrCleanupAction(&b,&v.cleanups[i])&&tableBooksEqual(&a,&b));xrtStrBufFree(&a);xrtStrBufFree(&b);}
 for(size_t i=0;i<v.stepCount;++i){XL6_IrExitStep s=m->exitSteps[steps+i];if(s.kind==XL6_IR_EXIT_STEP_SCOPE)s.firstCleanup-=clean;
  xstrbuf a={0},b={0};CHECK(book_XL6_IrExitStep(&a,&s)&&book_XL6_IrExitStep(&b,&v.steps[i])&&tableBooksEqual(&a,&b));xrtStrBufFree(&a);xrtStrBufFree(&b);}
 printf("selected function f128: %zu/%zu close and plan node entries, %zu/%zu route plans; outside native dependency reads remain allowed\n",work.planNodes,m->nodeCount,work.routePlans,m->exitPlanCount);
 xlsIrExitRangeDestroy(result);xlsSemanticCheckDestroy(model);xrtFree(path);xrtStrBufFree(&text);return 1;
}
static int rangeTests(void)
{
 xrtAtomic32Init(&checks,0);xrtAtomic32Init(&oracleRuns,0);xrtAtomic32Init(&oracleErrors,0);xrtAtomic32Init(&oracleActive,0);
 if(!xlsIrExitRangeAvailable()){puts("PASS range producer unavailable for unaudited cohort");return 1;}
 if(!rangeFailures()||!rangeSelectedFunction())return 0;
 char* path=xrtPathAbs("ir-exit-range-corpus.xl");CHECK(path);xrtAtomic32Store(&oracleActive,1,XMEMORY_RELEASE);
 int okay=corpus(path)&&regionCorpus(path);xrtAtomic32Store(&oracleActive,0,XMEMORY_RELEASE);xrtFree(path);
 CHECK(okay&&!xrtAtomic32Load(&oracleErrors,XMEMORY_RELAXED)&&xrtAtomic32Load(&oracleRuns,XMEMORY_RELAXED)==134);
 printf("analysis_ir_exit_range_test passed: %u checks;134 modules, two node partitions, full native verifier/JSON\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 1;
}
#ifdef XLS_IR_EXIT_RANGE_LIBRARY
int xlsIrExitRangeFixtureMain(void){return rangeTests()?0:1;}
#else
int main(void){return rangeTests()?0:1;}
#endif
