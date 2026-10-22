/* Included in the shared IR TU by an audited build overlay. The original
 * rules stay below; indexes omit unrelated candidates and proofs are local
 * to one immutable verification. Compiler materials are never modified. */
#include "analysis/frontend_ir.h"
#include "analysis/frontend_types.h"

#define XLS_IR_SCAN_BYTES (16u*1024u*1024u)
#define XLS_IR_PLAN_BYTES (8u*1024u*1024u)
#define XLS_IR_SCOPE_BYTES (1u*1024u*1024u)
#define XLS_IR_CALL_BYTES (8u*1024u*1024u)
typedef struct xls_ir_candidate { XL6_IrNodeId owner,node; } xls_ir_candidate;
typedef struct xls_ir_plan_candidate { XL6_IrNodeId origin; size_t index; } xls_ir_plan_candidate;
typedef struct xls_ir_scan {
    const XL6_IrModule* module;
    const XL6_IrNode* nodes;
    size_t count;
    xarray all,routed;
    xarray plans,positions,scopeProof;
    const XL6_IrExitPlan* planSource;
    const XL6_IrExitPlan* scopeSource;
    size_t planCount,planBudget;
    size_t scopeCount;
    int ready,topology,plansReady,plansAttempted,verifying,scopeAttempted,scopeReady;
    struct xls_ir_scan* previous;
} xls_ir_scan;
static _Thread_local xls_ir_scan* xlsActiveIrScan;
static _Thread_local xls_ir_scan_work xlsIrScanWork;
static _Thread_local xls_ir_plan_scan_work xlsIrPlanWork;
static _Thread_local xls_ir_scope_proof_work xlsIrScopeWork;
static XL6_IrNodeId xl6IrCleanupEnclosingCallable(const XL6_IrModule*,XL6_IrNodeId);
static int xl6IrVerifyNodeTopology(const XL6_IrModule*,XL6_DiagList*);
static int xlsIrRefreshExitPlansOriginal(XL6_IrModule*);
static int xlsIrVerifyModuleOriginal(const XL6_IrModule*,XL6_DiagList*);
static int xlsIrVerifyScopeExitActionsOriginal(const XL6_IrModule*,const XL6_IrExitPlan*);
static int xlsIrBuildImplicitConversionAbisOriginal(XL6_IrModule*);
static int xlsIrBuildModuleOriginal(XL6_IrModule*,const XL6_AstNode*,XL6_DiagList*);
static const XL6_AstNode* xlsIrFindTypeDeclOriginal(const XL6_AstNode*,const XL6_AstNode*);

#define XLS_IR_TYPE_LOOKUP_BYTES (8u*1024u*1024u)
typedef struct xls_ir_type_lookup {
    struct xls_ir_type_lookup* previous;
    const XL6_AstNode* root;
    XL6_AstNode** sourceChildren;
    size_t sourceCount;
    XL6_AstNode filtered;
    XL6_AstNode** items;
    int ready;
} xls_ir_type_lookup;
static _Thread_local xls_ir_type_lookup* xlsActiveTypeLookup;
static _Thread_local xls_ir_type_lookup_work xlsIrTypeLookupWork;
static int xlsIrNominalCandidate(const XL6_AstNode* node)
{
    return node && (node->kind==XL6_AST_CLASS || node->kind==XL6_AST_PROTOCOL ||
        node->kind==XL6_AST_UNION || node->kind==XL6_AST_ENUM);
}
static void xlsIrTypeLookupBegin(xls_ir_type_lookup* scan,const XL6_AstNode* root,size_t bytes)
{
    memset(scan,0,sizeof(*scan)); scan->root=root; scan->previous=xlsActiveTypeLookup; xlsActiveTypeLookup=scan;
    if(!root || !bytes) return;
    if(bytes>XLS_IR_TYPE_LOOKUP_BYTES) bytes=XLS_IR_TYPE_LOOKUP_BYTES;
    size_t count=0;
    for(size_t i=0;i<root->childCount;++i) {
        ++xlsIrTypeLookupWork.rootVisits;
        if(xlsIrNominalCandidate(root->children[i]) && ++count>bytes/sizeof(*scan->items)) return;
    }
    size_t charged=count*sizeof(*scan->items);
    scan->items=count ? xrtMalloc(charged) : NULL;
    if(count && !scan->items) return;
    size_t next=0;
    for(size_t i=0;i<root->childCount;++i)
        if(xlsIrNominalCandidate(root->children[i])) scan->items[next++]=root->children[i];
    scan->sourceChildren=root->children; scan->sourceCount=root->childCount;
    scan->filtered=*root; scan->filtered.children=scan->items;
    scan->filtered.childCount=scan->filtered.childCapacity=count; scan->ready=1;
    ++xlsIrTypeLookupWork.indexes; xlsIrTypeLookupWork.declarations+=count;
    if(charged>xlsIrTypeLookupWork.peakBytes) xlsIrTypeLookupWork.peakBytes=charged;
}
static const XL6_AstNode* xl6IrFindTypeDecl(const XL6_AstNode* root,const XL6_AstNode* type)
{
    xls_ir_type_lookup* scan=xlsActiveTypeLookup; ++xlsIrTypeLookupWork.queries;
    if(scan && scan->ready && scan->root==root && root->children==scan->sourceChildren && root->childCount==scan->sourceCount) {
        ++xlsIrTypeLookupWork.indexedQueries; return xlsIrFindTypeDeclOriginal(&scan->filtered,type);
    }
    ++xlsIrTypeLookupWork.linearQueries; return xlsIrFindTypeDeclOriginal(root,type);
}
int xlsFrontendIrBuildTypeBudget(XL6_IrModule* module,const XL6_AstNode* root,XL6_DiagList* diags,size_t bytes)
{
    if(bytes>XLS_IR_TYPE_LOOKUP_BYTES) bytes=XLS_IR_TYPE_LOOKUP_BYTES;
    xls_ir_type_lookup scan; xlsIrTypeLookupBegin(&scan,module ? root : NULL,bytes);
    size_t charged=scan.ready ? scan.filtered.childCount*sizeof(*scan.items) : 0;
    xls_type_alias_scope aliases;
    xlsFrontendTypeAliasBegin(&aliases,module ? root : NULL,bytes-charged);
    int okay=xlsIrBuildModuleOriginal(module,root,diags);
    xlsFrontendTypeAliasEnd(&aliases);
    xlsActiveTypeLookup=scan.previous; xrtFree(scan.items); return okay;
}
int xl6IrBuildModuleFromAst(XL6_IrModule* module,const XL6_AstNode* root,XL6_DiagList* diags)
{ return xlsFrontendIrBuildTypeBudget(module,root,diags,XLS_IR_TYPE_LOOKUP_BYTES); }
void xlsFrontendIrTypeLookupReset(void) { memset(&xlsIrTypeLookupWork,0,sizeof(xlsIrTypeLookupWork)); }
xls_ir_type_lookup_work xlsFrontendIrTypeLookupWork(void) { return xlsIrTypeLookupWork; }

typedef struct xls_ir_call_scan {
    const XL6_IrModule* module;
    const XL6_IrCallSite* source;
    size_t count,budget;
    xarray calls,positions;
    int attempted,ready;
    struct xls_ir_call_scan* previous;
} xls_ir_call_scan;
static _Thread_local xls_ir_call_scan* xlsActiveCallScan;
static _Thread_local xls_ir_call_scan_work xlsIrCallWork;

static void xlsIrCallScanBegin(xls_ir_call_scan* scan,const XL6_IrModule* module,size_t bytes)
{
    memset(scan,0,sizeof(*scan)); scan->module=module;
    scan->budget=bytes<XLS_IR_CALL_BYTES ? bytes : XLS_IR_CALL_BYTES;
    scan->previous=xlsActiveCallScan; xlsActiveCallScan=scan;
}
static void xlsIrCallScanDone(xls_ir_call_scan* scan)
{
    xlsActiveCallScan=scan->previous; xrtArrayUnit(&scan->calls); xrtArrayUnit(&scan->positions);
}
static int xlsIrCallCompare(const void* a,const void* b)
{
    const xls_ir_plan_candidate *left=a,*right=b;
    if(left->origin!=right->origin) return left->origin<right->origin ? -1 : 1;
    return left->index<right->index ? -1 : left->index!=right->index;
}
static const xls_ir_call_scan* xlsIrCallScan(const XL6_IrModule* module)
{
    xls_ir_call_scan* scan=xlsActiveCallScan;
    if(!scan || scan->module!=module || !module) return NULL;
    if(!scan->attempted) {
        scan->attempted=1;
        /* Only origin and physical index are collected. Bounds precede the
         * first read; malformed arguments/types are left to the original
         * query. No negative result or conversion effect is cached. */
        size_t count=module->callSiteCount;
        if(!module->callSites || !count || count>module->callSiteCapacity ||
           count>scan->budget/(2*(sizeof(xls_ir_plan_candidate)+sizeof(size_t))) ||
           !xrtArrayInit(&scan->calls,sizeof(xls_ir_plan_candidate)) ||
           !xrtArrayInit(&scan->positions,sizeof(size_t)) ||
           !xrtArrayReserve(&scan->calls,count) || !xrtArrayResize(&scan->positions,count)) goto failed;
        uint64_t bytes=(uint64_t)scan->calls.Capacity*sizeof(xls_ir_plan_candidate)+
            (uint64_t)scan->positions.Capacity*sizeof(size_t);
        if(bytes>scan->budget) goto failed;
        for(size_t i=0;i<count;++i) {
            xls_ir_plan_candidate item={module->callSites[i].origin,i};
            if(!xrtArrayPush(&scan->calls,&item)) goto failed;
        }
        if(!xrtArraySort(&scan->calls,xlsIrCallCompare)) goto failed;
        for(size_t i=0;i<count;++i) {
            const xls_ir_plan_candidate* item=xrtArrayConstGet(&scan->calls,i);
            *(size_t*)xrtArrayGet(&scan->positions,item->index)=i;
        }
        scan->source=module->callSites; scan->count=count; scan->ready=1;
        ++xlsIrCallWork.indexes; xlsIrCallWork.records+=count;
        if(bytes>xlsIrCallWork.peakBytes) xlsIrCallWork.peakBytes=bytes;
    }
    return scan->ready && scan->source==module->callSites && scan->count==module->callSiteCount ? scan : NULL;
failed:
    xrtArrayUnit(&scan->calls); xrtArrayUnit(&scan->positions); return NULL;
}
static size_t xlsIrFirstCallCandidate(const XL6_IrModule* module,XL6_IrNodeId origin)
{
    ++xlsIrCallWork.queries;
    const xls_ir_call_scan* scan=xlsIrCallScan(module);
    if(!scan) { if(module->callSiteCount) ++xlsIrCallWork.linearVisits; return 0; }
    size_t low=0,high=scan->calls.Count;
    while(low<high) {
        size_t mid=low+(high-low)/2;
        const xls_ir_plan_candidate* item=xrtArrayConstGet(&scan->calls,mid);
        if(item->origin<origin) low=mid+1; else high=mid;
    }
    const xls_ir_plan_candidate* item=xrtArrayConstGet(&scan->calls,low);
    if(!item || item->origin!=origin) return module->callSiteCount;
    ++xlsIrCallWork.indexedVisits; return item->index;
}
static size_t xlsIrNextCallCandidate(const XL6_IrModule* module,XL6_IrNodeId origin,size_t current)
{
    const xls_ir_call_scan* scan=xlsIrCallScan(module);
    if(!scan) { if(current+1<module->callSiteCount) ++xlsIrCallWork.linearVisits; return current+1; }
    const size_t* position=xrtArrayConstGet(&scan->positions,current);
    const xls_ir_plan_candidate* item=position ? xrtArrayConstGet(&scan->calls,*position+1) : NULL;
    if(!item || item->origin!=origin) return module->callSiteCount;
    ++xlsIrCallWork.indexedVisits; return item->index;
}
int xlsFrontendIrConversionBudget(XL6_IrModule* module,size_t bytes)
{
    xls_ir_call_scan scan; xlsIrCallScanBegin(&scan,module,bytes);
    int okay=xlsIrBuildImplicitConversionAbisOriginal(module); xlsIrCallScanDone(&scan); return okay;
}
static int xl6IrBuildImplicitConversionAbis(XL6_IrModule* module)
{ return xlsFrontendIrConversionBudget(module,XLS_IR_CALL_BYTES); }
void xlsFrontendIrCallScanReset(void) { memset(&xlsIrCallWork,0,sizeof(xlsIrCallWork)); }
xls_ir_call_scan_work xlsFrontendIrCallScanWork(void) { return xlsIrCallWork; }
int xlsFrontendIrReferenceConversions(XL6_IrModule* module)
{
    xls_ir_call_scan* previous=xlsActiveCallScan; xlsActiveCallScan=NULL;
    int okay=xlsIrBuildImplicitConversionAbisOriginal(module); xlsActiveCallScan=previous; return okay;
}

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
    scan->topology=1; scan->module=module; scan->nodes=module->nodes; scan->count=module->nodeCount;
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
{
    xlsActiveIrScan=scan->previous; xrtArrayUnit(&scan->all); xrtArrayUnit(&scan->routed);
    xrtArrayUnit(&scan->plans); xrtArrayUnit(&scan->positions);
    xrtArrayUnit(&scan->scopeProof);
}
static size_t xlsIrScopeProofSlot(xls_ir_scan* scan,const XL6_IrModule* module,const XL6_IrExitPlan* plan)
{
    if(!scan || !scan->verifying || !scan->topology || scan->module!=module || !plan ||
       scan->nodes!=module->nodes || scan->count!=module->nodeCount || !module->exitPlans ||
       !module->exitPlanCount || module->exitPlanCount>module->exitPlanCapacity) return SIZE_MAX;
    /* Only physical arena entries qualify. Local/temporary plans still run
     * the original rule. Integer offsets avoid unrelated-pointer arithmetic. */
    uintptr_t base=(uintptr_t)module->exitPlans,at=(uintptr_t)plan;
    if(at<base || (at-base)%sizeof(*plan)) return SIZE_MAX;
    uintptr_t offset=(at-base)/sizeof(*plan);
    if(offset>=module->exitPlanCount) return SIZE_MAX;
    if(!scan->scopeAttempted) {
        scan->scopeAttempted=1;
        size_t limit=scan->planBudget<XLS_IR_SCOPE_BYTES ? scan->planBudget : XLS_IR_SCOPE_BYTES;
        if(module->exitPlanCount>limit/2 || !xrtArrayInit(&scan->scopeProof,sizeof(unsigned char)) ||
           !xrtArrayResize(&scan->scopeProof,module->exitPlanCount) || scan->scopeProof.Capacity>limit) {
            xrtArrayUnit(&scan->scopeProof); return SIZE_MAX;
        }
        scan->scopeSource=module->exitPlans; scan->scopeCount=module->exitPlanCount; scan->scopeReady=1;
        ++xlsIrScopeWork.indexes;
        if(scan->scopeProof.Capacity>xlsIrScopeWork.peakBytes) xlsIrScopeWork.peakBytes=scan->scopeProof.Capacity;
    }
    return scan->scopeReady && scan->scopeSource==module->exitPlans && scan->scopeCount==module->exitPlanCount ? (size_t)offset : SIZE_MAX;
}
static int xl6IrVerifyScopeExitActions(const XL6_IrModule* module,const XL6_IrExitPlan* plan)
{
    ++xlsIrScopeWork.requests;
    xls_ir_scan* scan=xlsActiveIrScan;
    size_t slot=xlsIrScopeProofSlot(scan,module,plan);
    if(slot!=SIZE_MAX && scan->scopeProof.Data[slot]) { ++xlsIrScopeWork.hits; return 1; }
    ++xlsIrScopeWork.checks;
    int okay=xlsIrVerifyScopeExitActionsOriginal(module,plan);
    /* The shared pure check has run on the same immutable module/plan. Only
     * success is remembered, within this Verify call; route walking and all
     * other verification remain independent and unchanged. */
    if(okay && slot!=SIZE_MAX) scan->scopeProof.Data[slot]=1;
    return okay;
}
static int xlsIrPlanCompare(const void* a,const void* b)
{
    const xls_ir_plan_candidate *left=a,*right=b;
    if(left->origin!=right->origin) return left->origin<right->origin ? -1 : 1;
    return left->index<right->index ? -1 : left->index!=right->index;
}
static void xlsIrPlanScanInit(xls_ir_scan* scan,const XL6_IrModule* module,size_t bytes)
{
    /* Build only for the const verification phase, after the shared rooted
     * topology check. Reject inconsistent arena bounds before reading it.
     * Invalid origins remain present; the original verifier diagnoses them. */
    if(bytes>XLS_IR_PLAN_BYTES) bytes=XLS_IR_PLAN_BYTES;
    if(!bytes || !scan->topology || !module || module->exitPlanCount>module->exitPlanCapacity ||
       (module->exitPlanCount && !module->exitPlans) ||
       module->exitPlanCount>bytes/(2*(sizeof(xls_ir_plan_candidate)+sizeof(size_t)))) return;
    if(!xrtArrayInit(&scan->plans,sizeof(xls_ir_plan_candidate)) ||
       !xrtArrayInit(&scan->positions,sizeof(size_t)) ||
       !xrtArrayReserve(&scan->plans,module->exitPlanCount) ||
       !xrtArrayResize(&scan->positions,module->exitPlanCount)) goto failed;
    uint64_t owned=(uint64_t)scan->plans.Capacity*sizeof(xls_ir_plan_candidate)+(uint64_t)scan->positions.Capacity*sizeof(size_t);
    if(owned>bytes) goto failed;
    for(size_t i=0;i<module->exitPlanCount;++i) {
        xls_ir_plan_candidate candidate={module->exitPlans[i].origin,i};
        if(!xrtArrayPush(&scan->plans,&candidate)) goto failed;
    }
    if(!xrtArraySort(&scan->plans,xlsIrPlanCompare)) goto failed;
    for(size_t i=0;i<scan->plans.Count;++i) {
        const xls_ir_plan_candidate* candidate=xrtArrayConstGet(&scan->plans,i);
        *(size_t*)xrtArrayGet(&scan->positions,candidate->index)=i;
    }
    scan->planSource=module->exitPlans; scan->planCount=module->exitPlanCount; scan->plansReady=1;
    ++xlsIrPlanWork.indexes; xlsIrPlanWork.records+=module->exitPlanCount;
    if(owned>xlsIrPlanWork.peakBytes) xlsIrPlanWork.peakBytes=owned;
    return;
failed:
    xrtArrayUnit(&scan->plans); xrtArrayUnit(&scan->positions);
}
static const xls_ir_scan* xlsIrPlanScan(const XL6_IrModule* module)
{
    const xls_ir_scan* scan=xlsActiveIrScan;
    return scan && scan->plansReady && scan->module==module && scan->nodes==module->nodes &&
        scan->count==module->nodeCount && scan->planSource==module->exitPlans && scan->planCount==module->exitPlanCount ? scan : NULL;
}
static void xlsIrEnsurePlanScan(const XL6_IrModule* module)
{
    xls_ir_scan* scan=xlsActiveIrScan;
    if(!scan || scan->plansAttempted || !scan->topology || scan->module!=module ||
       scan->nodes!=module->nodes || scan->count!=module->nodeCount) return;
    /* Lazy construction starts only at the original plan enumeration, after
     * all preceding validation. A rejected module must not make the index
     * read arenas which the original verifier would never inspect. */
    scan->plansAttempted=1; xlsIrPlanScanInit(scan,module,scan->planBudget);
}
static size_t xlsIrFirstPlanCandidate(const XL6_IrModule* module,XL6_IrNodeId origin,size_t limit)
{
    ++xlsIrPlanWork.queries;
    xlsIrEnsurePlanScan(module);
    const xls_ir_scan* scan=xlsIrPlanScan(module);
    if(!scan) { if(limit) ++xlsIrPlanWork.linearVisits; return 0; }
    size_t low=0,high=scan->plans.Count;
    while(low<high) {
        size_t mid=low+(high-low)/2;
        const xls_ir_plan_candidate* candidate=xrtArrayConstGet(&scan->plans,mid);
        if(candidate->origin<origin) low=mid+1; else high=mid;
    }
    const xls_ir_plan_candidate* candidate=xrtArrayConstGet(&scan->plans,low);
    if(!candidate || candidate->origin!=origin || candidate->index>=limit) return limit;
    ++xlsIrPlanWork.indexedVisits; return candidate->index;
}
static size_t xlsIrNextPlanCandidate(const XL6_IrModule* module,XL6_IrNodeId origin,size_t current,size_t limit)
{
    ++xlsIrPlanWork.queries;
    const xls_ir_scan* scan=xlsIrPlanScan(module);
    if(!scan) { if(current+1<limit) ++xlsIrPlanWork.linearVisits; return current+1; }
    const size_t* position=xrtArrayConstGet(&scan->positions,current);
    const xls_ir_plan_candidate* candidate=position ? xrtArrayConstGet(&scan->plans,*position+1) : NULL;
    if(!candidate || candidate->origin!=origin || candidate->index>=limit) return limit;
    ++xlsIrPlanWork.indexedVisits; return candidate->index;
}
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
    xls_ir_call_scan calls; xlsIrCallScanBegin(&calls,module,XLS_IR_CALL_BYTES);
    xls_ir_scan scan; xlsIrScanInit(&scan,module); xlsActiveIrScan=&scan;
    int okay=xlsIrRefreshExitPlansOriginal(module); xlsIrScanDone(&scan); xlsIrCallScanDone(&calls); return okay;
}
int xlsFrontendIrVerifyPlanBudget(const XL6_IrModule* module,XL6_DiagList* diags,size_t bytes)
{
    xls_ir_call_scan calls; xlsIrCallScanBegin(&calls,module,bytes);
    xls_ir_scan scan; xlsIrScanInit(&scan,module); scan.planBudget=bytes; scan.verifying=1; xlsActiveIrScan=&scan;
    int okay=xlsIrVerifyModuleOriginal(module,diags); xlsIrScanDone(&scan); xlsIrCallScanDone(&calls); return okay;
}
int xl6IrVerifyModule(const XL6_IrModule* module,XL6_DiagList* diags)
{ return xlsFrontendIrVerifyPlanBudget(module,diags,XLS_IR_PLAN_BYTES); }
void xlsFrontendIrScanReset(void) { memset(&xlsIrScanWork,0,sizeof(xlsIrScanWork)); }
xls_ir_scan_work xlsFrontendIrScanWork(void) { return xlsIrScanWork; }
void xlsFrontendIrPlanScanReset(void) { memset(&xlsIrPlanWork,0,sizeof(xlsIrPlanWork)); }
xls_ir_plan_scan_work xlsFrontendIrPlanScanWork(void) { return xlsIrPlanWork; }
void xlsFrontendIrScopeProofReset(void) { memset(&xlsIrScopeWork,0,sizeof(xlsIrScopeWork)); }
xls_ir_scope_proof_work xlsFrontendIrScopeProofWork(void) { return xlsIrScopeWork; }
int xlsFrontendIrReferenceRefresh(XL6_IrModule* module)
{
    xls_ir_call_scan* previousCalls=xlsActiveCallScan; xlsActiveCallScan=NULL;
    xls_ir_scan* prior=xlsActiveIrScan; xlsActiveIrScan=NULL;
    int okay=xlsIrRefreshExitPlansOriginal(module); xlsActiveIrScan=prior; xlsActiveCallScan=previousCalls; return okay;
}
int xlsFrontendIrReferenceVerify(const XL6_IrModule* module,XL6_DiagList* diags)
{
    xls_ir_call_scan* previousCalls=xlsActiveCallScan; xlsActiveCallScan=NULL;
    xls_ir_scan* prior=xlsActiveIrScan; xlsActiveIrScan=NULL;
    int okay=xlsIrVerifyModuleOriginal(module,diags); xlsActiveIrScan=prior; xlsActiveCallScan=previousCalls; return okay;
}
