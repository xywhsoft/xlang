/* Included at the end of the native IR translation unit. All input borrows
 * are read-only: the qualified helpers write only the three output arrays. */
#include "analysis/ir_exit_range.h"
struct xls_ir_exit_range { xls_ir_exit_range_view view; };
int xlsIrExitRangeAvailable(void)
{
#ifdef XLS_NATIVE_IR_RANGE_PRODUCTION
 return 1;
#else
 return 0;
#endif
}
void xlsIrExitRangeDestroy(xls_ir_exit_range* result)
{
 if(!result)return;
 xrtFree((void*)result->view.close);xrtFree((void*)result->view.plans);
 xrtFree((void*)result->view.cleanups);xrtFree((void*)result->view.steps);
 xrtFree(result);
}
xls_ir_exit_range_view xlsIrExitRangeView(const xls_ir_exit_range* result)
{return result?result->view:(xls_ir_exit_range_view){0};}
#ifdef XLS_NATIVE_IR_RANGE_PRODUCTION
static int xlsIrExitRangeStop(const XL6_IrModule* m,size_t limit,
 int(*cancelled)(void*),void* user,xls_ir_exit_range_work* work)
{
 if((cancelled&&cancelled(user)) || xlsFrontendCancelObserved()){
  work->cancelled=1;return 1;
 }
 if(m->exitPlanCapacity>limit/sizeof(*m->exitPlans))goto limited;
 limit-=m->exitPlanCapacity*sizeof(*m->exitPlans);
 if(m->cleanupActionCapacity>limit/sizeof(*m->cleanupActions))goto limited;
 limit-=m->cleanupActionCapacity*sizeof(*m->cleanupActions);
 if(m->exitStepCapacity>limit/sizeof(*m->exitSteps))goto limited;
 return 0;
limited:work->limited=1;return 1;
}
#include "ir_exit_range_generated.inc"
#endif
xls_ir_exit_range* xlsIrExitRangeProduce(const XL6_IrModule* input,size_t first,
 size_t count,size_t limit,int(*cancelled)(void*),void* user,
 xls_ir_exit_range_status* status,xls_ir_exit_range_work* outputWork)
{
 xls_ir_exit_range_status code=XLS_EXIT_RANGE_INPUT;
 xls_ir_exit_range_work work={0};xls_ir_exit_range* result=NULL;
#ifdef XLS_NATIVE_IR_RANGE_PRODUCTION
 XL6_IrModule module={0};
 if(!input || input->instructionCount || input->nodeCount>UINT32_MAX || input->nodeCount>input->nodeCapacity ||
    (input->nodeCount&&!input->nodes) || input->childCount>input->childCapacity ||
    (input->childCount&&!input->children) || first>input->nodeCount ||
    count>input->nodeCount-first)goto done;
 if((cancelled&&cancelled(user)) || xlsFrontendCancelObserved()){
  code=XLS_EXIT_RANGE_CANCELLED;work.cancelled=1;goto done;
 }
 if(limit<sizeof(*result) || count>(limit-sizeof(*result))/sizeof(XL6_IrGeneratorClosePointAbi)){
  code=XLS_EXIT_RANGE_LIMIT;work.limited=1;goto done;
 }
 result=xrtCalloc(1,sizeof(*result));if(!result){code=XLS_EXIT_RANGE_MEMORY;goto done;}
 result->view.first=first;result->view.count=count;
 result->view.bytes=sizeof(*result)+count*sizeof(XL6_IrGeneratorClosePointAbi);
 limit-=result->view.bytes;
 if(count){result->view.close=xrtMalloc(count*sizeof(*result->view.close));
  if(!result->view.close){code=XLS_EXIT_RANGE_MEMORY;goto failed;}}
 module=*input;
 module.exitPlans=NULL;module.exitPlanCount=module.exitPlanCapacity=0;
 module.cleanupActions=NULL;module.cleanupActionCount=module.cleanupActionCapacity=0;
 module.exitSteps=NULL;module.exitStepCount=module.exitStepCapacity=0;
 for(size_t i=0;i<count;++i){
  if(xlsIrExitRangeStop(&module,limit,cancelled,user,&work))goto producerFailed;
  ((XL6_IrGeneratorClosePointAbi*)result->view.close)[i]=xl6IrGeneratorClosePointForNode(&module,(XL6_IrNodeId)(first+i+1));
  ++work.closeNodes;
 }
 if(!xlsIrExitRangePlans(&module,first,first+count,limit,cancelled,user,&work))goto producerFailed;
 for(size_t i=0;i<module.exitPlanCount;++i){
  if(xlsIrExitRangeStop(&module,limit,cancelled,user,&work))goto producerFailed;
  XL6_IrExitPlan* plan=&module.exitPlans[i];++work.routePlans;
  plan->firstStep=0;plan->stepCount=0;
  if(!xl6IrExitPlanHasRoute(plan))continue;
  plan->firstStep=module.exitStepCount;
  if(!xl6IrWalkExitRoute(&module,plan,xl6IrAppendExitStep,&module,&plan->stepCount))goto producerFailed;
 }
 if(xlsIrExitRangeStop(&module,limit,cancelled,user,&work))goto producerFailed;
 result->view.plans=module.exitPlans;result->view.planCount=module.exitPlanCount;
 result->view.cleanups=module.cleanupActions;result->view.cleanupCount=module.cleanupActionCount;
 result->view.steps=module.exitSteps;result->view.stepCount=module.exitStepCount;
 result->view.bytes+=module.exitPlanCapacity*sizeof(*module.exitPlans)+
  module.cleanupActionCapacity*sizeof(*module.cleanupActions)+module.exitStepCapacity*sizeof(*module.exitSteps);
 code=XLS_EXIT_RANGE_OK;goto done;
producerFailed:
 code=work.cancelled?XLS_EXIT_RANGE_CANCELLED:work.limited?XLS_EXIT_RANGE_LIMIT:XLS_EXIT_RANGE_PRODUCER;
 xrtFree(module.exitPlans);xrtFree(module.cleanupActions);xrtFree(module.exitSteps);
failed:xlsIrExitRangeDestroy(result);result=NULL;
done:
#else
 (void)input;(void)first;(void)count;(void)limit;(void)cancelled;(void)user;
 code=XLS_EXIT_RANGE_UNAVAILABLE;
#endif
 if(status)*status=code;
 if(outputWork)*outputWork=work;
 return result;
}
