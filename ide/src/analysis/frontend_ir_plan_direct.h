#ifndef XLANG_ANALYSIS_FRONTEND_IR_PLAN_DIRECT_H
#define XLANG_ANALYSIS_FRONTEND_IR_PLAN_DIRECT_H
#include "src/ir/xl6_ir.h"
#include <xrt.h>
/* Private candidates for one immutable plan arena. Values are original
 * array indices plus one;zero is absence. Order/duplicates/limits remain
 * those of the original linear loop. This helper never validates language
 * or dereferences nodes. Caller supplies the read-only verification fence. */
typedef struct xls_ir_plan_direct {
    xarray heads,next;
    size_t nodes,plans,bytes;
} xls_ir_plan_direct;
static inline void xlsIrPlanDirectClear(xls_ir_plan_direct* index)
{
    if(!index)return;
    xrtArrayUnit(&index->heads);xrtArrayUnit(&index->next);*index=(xls_ir_plan_direct){0};
}
static inline int xlsIrPlanDirectBuild(xls_ir_plan_direct* index,const XL6_IrModule* module,size_t bytes)
{
    if(!index)return 0;
    xlsIrPlanDirectClear(index);
    size_t slots=bytes/(2*sizeof(size_t));
    if(!module || !module->exitPlanCount || !module->exitPlans ||
       module->exitPlanCount>module->exitPlanCapacity || module->nodeCount>=SIZE_MAX ||
       module->nodeCount+1>slots || module->exitPlanCount>slots-module->nodeCount-1)return 0;
    if(!xrtArrayInit(&index->heads,sizeof(size_t)) || !xrtArrayInit(&index->next,sizeof(size_t)) ||
       !xrtArrayResize(&index->heads,module->nodeCount+1) || !xrtArrayResize(&index->next,module->exitPlanCount))goto failed;
    if(index->heads.Capacity>bytes/sizeof(size_t) ||
       index->next.Capacity>bytes/sizeof(size_t)-index->heads.Capacity)goto failed;
    size_t* heads=(size_t*)index->heads.Data;size_t* next=(size_t*)index->next.Data;
    for(size_t i=module->exitPlanCount;i;--i){
        XL6_IrNodeId origin=module->exitPlans[i-1].origin;
        if(origin>module->nodeCount)goto failed;
        next[i-1]=heads[origin];heads[origin]=i;
    }
    index->nodes=module->nodeCount;index->plans=module->exitPlanCount;
    index->bytes=(index->heads.Capacity+index->next.Capacity)*sizeof(size_t);return 1;
failed:xlsIrPlanDirectClear(index);return 0;
}
static inline size_t xlsIrPlanDirectFirst(const xls_ir_plan_direct* index,XL6_IrNodeId origin,size_t limit)
{
    if(!index || !index->heads.Data || origin>index->nodes)return limit;
    size_t value=((const size_t*)index->heads.Data)[origin];return value && value-1<limit ? value-1 : limit;
}
static inline size_t xlsIrPlanDirectNext(const xls_ir_plan_direct* index,size_t current,size_t limit)
{
    if(!index || !index->next.Data || current>=index->plans)return limit;
    size_t value=((const size_t*)index->next.Data)[current];return value && value-1<limit ? value-1 : limit;
}
#endif
