/* Included in the shared IR TU by an audited build overlay. Only iteration
 * candidates change; the original cleanup rules and independent verifier
 * still run. The immutable compiler materials are never modified. */
#include "analysis/frontend_ir.h"

#define XLS_IR_SCAN_BYTES (16u*1024u*1024u)
typedef struct xls_ir_candidate { XL6_IrNodeId owner,node; } xls_ir_candidate;
typedef struct xls_ir_scan {
    const XL6_IrModule* module;
    const XL6_IrNode* nodes;
    size_t count;
    xarray all,routed;
    int ready;
    struct xls_ir_scan* previous;
} xls_ir_scan;
static _Thread_local xls_ir_scan* xlsActiveIrScan;
static _Thread_local xls_ir_scan_work xlsIrScanWork;
static XL6_IrNodeId xl6IrCleanupEnclosingCallable(const XL6_IrModule*,XL6_IrNodeId);
static int xl6IrVerifyNodeTopology(const XL6_IrModule*,XL6_DiagList*);
static int xlsIrRefreshExitPlansOriginal(XL6_IrModule*);
static int xlsIrVerifyModuleOriginal(const XL6_IrModule*,XL6_DiagList*);

static int xlsIrCandidateCompare(const void* a,const void* b)
{
    const xls_ir_candidate *left=a,*right=b;
    if(left->owner!=right->owner) return left->owner<right->owner ? -1 : 1;
    return left->node<right->node ? -1 : left->node!=right->node;
}
static void xlsIrScanInit(xls_ir_scan* scan,const XL6_IrModule* module)
{
    memset(scan,0,sizeof(*scan)); scan->previous=xlsActiveIrScan;
    if(!module || !module->nodes || !module->nodeCount || !module->root || module->root>module->nodeCount ||
       module->nodeCount>UINT32_MAX) return;
    /* Run the compiler's own linear rooted-tree preflight before asking any
     * parent-chain question, including for adversarial verification input.
     * On failure the unchanged original path supplies its normal diagnostic. */
    if(!xl6IrVerifyNodeTopology(module,NULL)) return;
    size_t count=0;
    for(size_t i=0;i<module->nodeCount;++i)
        count+=module->nodes[i].kind==XL6_IR_NODE_VALUE_DECL || module->nodes[i].kind==XL6_IR_NODE_PATTERN;
    /* XRT's geometric capacities can reserve up to twice the requested
     * count. A failed optional index always uses the original linear scan. */
    if(count>XLS_IR_SCAN_BYTES/(2*(sizeof(XL6_IrNodeId)+sizeof(xls_ir_candidate))) ||
       !xrtArrayInit(&scan->all,sizeof(XL6_IrNodeId)) || !xrtArrayInit(&scan->routed,sizeof(xls_ir_candidate)) ||
       !xrtArrayReserve(&scan->all,count) || !xrtArrayReserve(&scan->routed,count)) return;
    for(size_t i=0;i<module->nodeCount;++i) {
        const XL6_IrNode* node=&module->nodes[i];
        if(node->kind!=XL6_IR_NODE_VALUE_DECL && node->kind!=XL6_IR_NODE_PATTERN) continue;
        XL6_IrNodeId id=(XL6_IrNodeId)(i+1);
        xls_ir_candidate candidate={xl6IrCleanupEnclosingCallable(module,id),id};
        if(!xrtArrayPush(&scan->all,&id) || !xrtArrayPush(&scan->routed,&candidate)) return;
    }
    if(!xrtArraySort(&scan->routed,xlsIrCandidateCompare)) return;
    scan->module=module; scan->nodes=module->nodes; scan->count=module->nodeCount; scan->ready=1;
    ++xlsIrScanWork.indexes; xlsIrScanWork.candidates+=count;
    uint64_t bytes=(uint64_t)scan->all.Capacity*sizeof(XL6_IrNodeId)+(uint64_t)scan->routed.Capacity*sizeof(xls_ir_candidate);
    if(bytes>xlsIrScanWork.peakBytes) xlsIrScanWork.peakBytes=bytes;
}
static void xlsIrScanDone(xls_ir_scan* scan)
{ xlsActiveIrScan=scan->previous; xrtArrayUnit(&scan->all); xrtArrayUnit(&scan->routed); }
static XL6_IrNodeId xlsIrPreviousCandidate(const XL6_IrModule* module,
    XL6_IrNodeId owner,XL6_IrNodeId before,int routed)
{
    ++xlsIrScanWork.queries;
    const xls_ir_scan* scan=xlsActiveIrScan;
    if(!scan || !scan->ready || scan->module!=module || scan->nodes!=module->nodes || scan->count!=module->nodeCount) {
        if(before>1) ++xlsIrScanWork.linearVisits;
        return before ? before-1 : 0;
    }
    size_t low=0,high=routed ? scan->routed.Count : scan->all.Count;
    while(low<high) {
        size_t mid=low+(high-low)/2; int less;
        if(routed) {
            const xls_ir_candidate* candidate=xrtArrayConstGet(&scan->routed,mid);
            less=candidate->owner<owner || (candidate->owner==owner && candidate->node<before);
        } else less=*(const XL6_IrNodeId*)xrtArrayConstGet(&scan->all,mid)<before;
        if(less) low=mid+1; else high=mid;
    }
    XL6_IrNodeId result=0;
    if(low) {
        if(routed) { const xls_ir_candidate* candidate=xrtArrayConstGet(&scan->routed,low-1); if(candidate->owner==owner) result=candidate->node; }
        else result=*(const XL6_IrNodeId*)xrtArrayConstGet(&scan->all,low-1);
    }
    if(result) ++xlsIrScanWork.indexedVisits;
    return result;
}
int xl6IrRefreshExitPlans(XL6_IrModule* module)
{
    xls_ir_scan scan; xlsIrScanInit(&scan,module); xlsActiveIrScan=&scan;
    int okay=xlsIrRefreshExitPlansOriginal(module); xlsIrScanDone(&scan); return okay;
}
int xl6IrVerifyModule(const XL6_IrModule* module,XL6_DiagList* diags)
{
    xls_ir_scan scan; xlsIrScanInit(&scan,module); xlsActiveIrScan=&scan;
    int okay=xlsIrVerifyModuleOriginal(module,diags); xlsIrScanDone(&scan); return okay;
}
void xlsFrontendIrScanReset(void) { memset(&xlsIrScanWork,0,sizeof(xlsIrScanWork)); }
xls_ir_scan_work xlsFrontendIrScanWork(void) { return xlsIrScanWork; }
int xlsFrontendIrReferenceRefresh(XL6_IrModule* module)
{
    xls_ir_scan* prior=xlsActiveIrScan; xlsActiveIrScan=NULL;
    int okay=xlsIrRefreshExitPlansOriginal(module); xlsActiveIrScan=prior; return okay;
}
int xlsFrontendIrReferenceVerify(const XL6_IrModule* module,XL6_DiagList* diags)
{
    xls_ir_scan* prior=xlsActiveIrScan; xlsActiveIrScan=NULL;
    int okay=xlsIrVerifyModuleOriginal(module,diags); xlsActiveIrScan=prior; return okay;
}
