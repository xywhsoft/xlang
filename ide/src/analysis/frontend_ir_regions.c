#include "analysis/frontend_ir_regions.h"
#include <string.h>
typedef struct region_row { uint32 region; XL6_IrNodeId index; } region_row;
static _Thread_local xls_ir_region_work work;
void xlsFrontendIrRegionReset(void) { memset(&work,0,sizeof(work)); }
xls_ir_region_work xlsFrontendIrRegionWork(void) { return work; }
static int order(const void* left,const void* right)
{
    const region_row *a=left,*b=right;
    if(a->region!=b->region) return a->region<b->region ? -1 : 1;
    return (a->index>b->index)-(a->index<b->index);
}
void xlsIrRegionClear(xls_ir_region_index* index)
{ if(index) { xrtArrayUnit(&index->rows); memset(index,0,sizeof(*index)); } }
int xlsIrRegionBuild(xls_ir_region_index* index,const XL6_IrModule* module,size_t bytes)
{
    if(!index) return 0;
    xlsIrRegionClear(index);
    if(bytes>XLS_IR_REGION_BYTES) bytes=XLS_IR_REGION_BYTES;
    if(!bytes || !module || module->nodeCount>module->nodeCapacity || module->nodeCount>UINT32_MAX ||
       (module->nodeCount && !module->nodes)) return 0;
    size_t count=0;
    for(size_t i=0;i<module->nodeCount;++i) { ++work.rootVisits; if(module->nodes[i].kind==XL6_IR_NODE_BLOCK) ++count; }
    /* Bound completed data,including XRT's geometric/minimum reserve.
     * An optional index may decline without changing the original checker. */
    if(count>bytes/(2*sizeof(region_row)) || !xrtArrayInit(&index->rows,sizeof(region_row)) ||
       !xrtArrayReserve(&index->rows,count) || index->rows.Capacity>bytes/sizeof(region_row)) goto failed;
    for(size_t i=0;i<module->nodeCount;++i) {
        ++work.rootVisits; if(module->nodes[i].kind!=XL6_IR_NODE_BLOCK) continue;
        region_row row={module->nodes[i].region,(XL6_IrNodeId)i};
        if(!xrtArrayPush(&index->rows,&row)) goto failed;
    }
    if(!xrtArraySort(&index->rows,order)) goto failed;
    index->module=module; index->nodes=module->nodes; index->count=module->nodeCount; index->ready=1;
    ++work.indexes; work.records+=count;
    size_t used=index->rows.Capacity*sizeof(region_row); if(used>work.peakBytes) work.peakBytes=used;
    return 1;
failed: xlsIrRegionClear(index); return 0;
}
static int valid(const xls_ir_region_index* index,const XL6_IrModule* module)
{ return index && index->ready && module && index->module==module && index->nodes==module->nodes && index->count==module->nodeCount; }
static size_t find(const xls_ir_region_index* index,const XL6_IrModule* module,uint32 region,size_t after)
{
    size_t low=0,high=index->rows.Count;
    while(low<high) {
        size_t middle=low+(high-low)/2; const region_row* row=xrtArrayConstGet(&index->rows,middle);
        if(row->region<region || (row->region==region && row->index<after)) low=middle+1; else high=middle;
    }
    const region_row* row=xrtArrayConstGet(&index->rows,low);
    if(!row || row->region!=region) return module->nodeCount;
    ++work.indexedVisits; return row->index;
}
size_t xlsIrRegionFirst(const xls_ir_region_index* index,const XL6_IrModule* module,uint32 region)
{
    ++work.queries;
    if(valid(index,module)) return find(index,module,region,0);
    if(module && module->nodeCount) ++work.linearVisits;
    return 0;
}
size_t xlsIrRegionNext(const xls_ir_region_index* index,const XL6_IrModule* module,uint32 region,size_t current)
{
    ++work.queries; size_t limit=module ? module->nodeCount : 0;
    if(current>=limit) return limit;
    if(valid(index,module)) return find(index,module,region,current+1);
    if(current+1<limit) ++work.linearVisits;
    return current+1;
}
