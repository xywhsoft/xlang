#include "analysis/frontend_ir.h"
#include "analysis/frontend_types.h"
#include "src/parser/xl6_parser_api.h"
#include "src/stdlib/xl6_stdlib.h"
#include "src/sem/xl6_sem.h"
#include "src/type/xl6_type_model.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

static xatomic32 checks,checkedBoundaries;
#define CHECK(value) do { xrtAtomic32FetchAdd(&checks,1,XMEMORY_RELAXED); if(!(value)) { fprintf(stderr,"analysis_ir_scan_test:%d: %s\n",__LINE__,#value); return 0; } } while(0)
static char* dump(const XL6_IrModule* ir)
{
    XL6_Output out; if(!xl6OutputOpenMemory(&out)) return NULL;
    xl6IrDumpModuleJson(&out,ir);
    char* result=xl6OutputHasFailed(&out) ? NULL : xl6OutputTakeMemoryText(&out,NULL);
    xl6OutputDiscard(&out); return result;
}
static int sameDiagnostics(const XL6_DiagList* a,const XL6_DiagList* b)
{
    if(a->count!=b->count) return 0;
    for(size_t i=0;i<a->count;++i)
        if(a->items[i].line!=b->items[i].line || a->items[i].column!=b->items[i].column || strcmp(a->items[i].message,b->items[i].message)) return 0;
    return 1;
}
static int compareVerify(XL6_IrModule* ir,int accepted)
{
    XL6_DiagList indexed,reference; xl6DiagInit(&indexed); xl6DiagInit(&reference);
    int a=xl6IrVerifyModule(ir,&indexed),b=xlsFrontendIrReferenceVerify(ir,&reference);
    CHECK(a==b && (accepted<0 || a==accepted) && sameDiagnostics(&indexed,&reference));
    xl6DiagFree(&indexed); xl6DiagFree(&reference); return 1;
}
static int initializerValidationFence(XL6_IrModule* ir)
{
    if(!ir->valueInitializerCount)return 1;
    XL6_IrErrorAbi saved=ir->valueInitializers[0].errorAbi;
    ir->valueInitializers[0].errorAbi=(XL6_IrErrorAbi)INT_MAX;
    xlsFrontendIrInitializerReset();CHECK(compareVerify(ir,0));
    xls_ir_initializer_work work=xlsFrontendIrInitializerWork();
    CHECK(!work.indexes && !work.proofRecords && !xlsFrontendIrInitializerActive());
    ir->valueInitializers[0].errorAbi=saved;
    xlsFrontendIrInitializerReset();CHECK(compareVerify(ir,1));
    work=xlsFrontendIrInitializerWork();CHECK(work.indexes==1 && work.proofRecords==ir->valueInitializerCount);
    return 1;
}
static int compareRegions(XL6_IrModule* ir,int large)
{
    XL6_DiagList a,b; xl6DiagInit(&a); xl6DiagInit(&b);
    xlsFrontendIrRegionReset(); CHECK(xlsFrontendIrRefreshRegionBudget(ir,0));
    xls_ir_region_work original=xlsFrontendIrRegionWork(); char* expected=dump(ir); CHECK(expected);
    xlsFrontendIrRegionReset(); CHECK(xlsFrontendIrRefreshRegionBudget(ir,8u*1024u*1024u));
    xls_ir_region_work indexed=xlsFrontendIrRegionWork(); char* actual=dump(ir);
    CHECK(actual && !strcmp(expected,actual)); xrtFree(actual);
    CHECK(!original.indexes && !original.indexedVisits && indexed.peakBytes<=8u*1024u*1024u);
    if(original.linearVisits) CHECK(indexed.indexes==1 && !indexed.linearVisits && indexed.indexedVisits<original.linearVisits);
    xlsFrontendIrRegionReset(); CHECK(xlsFrontendIrRefreshRegionBudget(ir,1));
    xls_ir_region_work tiny=xlsFrontendIrRegionWork(); actual=dump(ir);
    CHECK(actual && !strcmp(expected,actual) && !tiny.indexes && !tiny.indexedVisits && tiny.linearVisits==original.linearVisits); xrtFree(actual); xrtFree(expected);
    xlsFrontendIrRegionReset(); CHECK(xlsFrontendIrVerifyRegionBudget(ir,&a,0));
    original=xlsFrontendIrRegionWork(); xlsFrontendIrRegionReset(); CHECK(xlsFrontendIrVerifyRegionBudget(ir,&b,8u*1024u*1024u));
    indexed=xlsFrontendIrRegionWork(); CHECK(sameDiagnostics(&a,&b));
    if(original.linearVisits) CHECK(indexed.indexes==1 && !indexed.linearVisits && indexed.indexedVisits<original.linearVisits);
    if(large) printf("IR region block visits indexed=%llu original=%llu, data=%llu bytes\n",(unsigned long long)indexed.indexedVisits,(unsigned long long)original.linearVisits,(unsigned long long)indexed.peakBytes);
    xl6DiagFree(&a); xl6DiagFree(&b); return 1;
}
static int compareScopeProof(XL6_IrModule* ir,int large)
{
    XL6_DiagList indexed,reference; xl6DiagInit(&indexed); xl6DiagInit(&reference);
    xlsFrontendIrScopeProofReset(); CHECK(xl6IrVerifyModule(ir,&indexed));
    xls_ir_scope_proof_work a=xlsFrontendIrScopeProofWork();
    xlsFrontendIrScopeProofReset(); CHECK(xlsFrontendIrReferenceVerify(ir,&reference));
    xls_ir_scope_proof_work b=xlsFrontendIrScopeProofWork();
    CHECK(!indexed.count && !reference.count && a.requests==b.requests && a.requests==a.checks+a.hits);
    CHECK(a.hits && a.checks<b.checks && a.indexes==1 && a.peakBytes<=1024u*1024u);
    CHECK(!b.hits && !b.indexes && b.requests==b.checks);
    xlsFrontendIrScopeProofReset(); CHECK(xlsFrontendIrVerifyPlanBudget(ir,&indexed,1));
    xls_ir_scope_proof_work fallback=xlsFrontendIrScopeProofWork();
    CHECK(!fallback.hits && !fallback.indexes && fallback.checks==b.checks);
    if(large) printf("IR scope action proofs checked=%llu reused=%llu reference=%llu, data=%llu bytes\n",
        (unsigned long long)a.checks,(unsigned long long)a.hits,(unsigned long long)b.checks,(unsigned long long)a.peakBytes);
    xl6DiagFree(&indexed); xl6DiagFree(&reference); return 1;
}
static int compareConversions(XL6_IrModule* ir,int large)
{
    xlsFrontendIrCallScanReset(); CHECK(xlsFrontendIrReferenceConversions(ir));
    xls_ir_call_scan_work reference=xlsFrontendIrCallScanWork();
    char* expected=dump(ir); CHECK(expected);
    xlsFrontendIrCallScanReset(); CHECK(xlsFrontendIrConversionBudget(ir,8u*1024u*1024u));
    xls_ir_call_scan_work indexed=xlsFrontendIrCallScanWork();
    char* actual=dump(ir); CHECK(actual && !strcmp(actual,expected)); xrtFree(actual);
    CHECK(!reference.indexes && !reference.indexedVisits && indexed.queries==reference.queries);
    if(ir->callSiteCount) CHECK(indexed.indexes==1 && !indexed.linearVisits && indexed.peakBytes<=8u*1024u*1024u);
    else CHECK(!indexed.indexes && !indexed.records && !indexed.indexedVisits);
    xlsFrontendIrCallScanReset(); CHECK(xlsFrontendIrConversionBudget(ir,1));
    xls_ir_call_scan_work bounded=xlsFrontendIrCallScanWork();
    actual=dump(ir); CHECK(actual && !strcmp(actual,expected)); xrtFree(actual); xrtFree(expected);
    CHECK(!bounded.indexes && !bounded.indexedVisits && bounded.linearVisits==reference.linearVisits);
    for(size_t i=0;i<ir->callSiteCount;++i) {
        const XL6_IrNode* node=xl6IrNodeGet(ir,ir->callSites[i].origin);
        if(node && node->implicitConversionErrorAbi==XL6_IR_ERROR_ABI_MAY_RAISE)
            xrtAtomic32FetchAdd(&checkedBoundaries,1,XMEMORY_RELAXED);
    }
    if(large) {
        CHECK(reference.linearVisits>indexed.indexedVisits*50 && indexed.indexedVisits);
        printf("IR conversion call candidates indexed=%llu reference=%llu, data=%llu bytes\n",
            (unsigned long long)indexed.indexedVisits,(unsigned long long)reference.linearVisits,(unsigned long long)indexed.peakBytes);
    }
    return 1;
}
static int compareTypeLookups(const XL6_AstNode* root,const char* expected)
{
    XL6_IrModule other; XL6_DiagList diags; xl6IrModuleInit(&other); xl6DiagInit(&diags);
    xlsFrontendIrTypeLookupReset(); xlsFrontendTypeAliasReset();
    CHECK(xlsFrontendIrBuildTypeBudget(&other,root,&diags,0) && !diags.count);
    xls_ir_type_lookup_work reference=xlsFrontendIrTypeLookupWork();
    xls_type_alias_work aliasReference=xlsFrontendTypeAliasWork();
    char* actual=dump(&other); CHECK(actual && !strcmp(actual,expected)); xrtFree(actual);
    CHECK(!reference.indexes && !reference.indexedQueries && reference.queries && reference.linearQueries==reference.queries);
    CHECK(!aliasReference.indexes && !aliasReference.indexedQueries && aliasReference.queries && aliasReference.linearQueries==aliasReference.queries);
    xlsFrontendIrTypeLookupReset(); xlsFrontendTypeAliasReset();
    CHECK(xlsFrontendIrBuildTypeBudget(&other,root,&diags,8u*1024u*1024u) && !diags.count);
    xls_ir_type_lookup_work indexed=xlsFrontendIrTypeLookupWork();
    xls_type_alias_work aliasIndexed=xlsFrontendTypeAliasWork();
    actual=dump(&other); CHECK(actual && !strcmp(actual,expected)); xrtFree(actual);
    CHECK(indexed.indexes==1 && indexed.rootVisits==root->childCount && indexed.queries==reference.queries && indexed.indexedQueries==indexed.queries && !indexed.linearQueries && indexed.peakBytes<=8u*1024u*1024u);
    CHECK(aliasIndexed.indexes==1 && aliasIndexed.queries==aliasReference.queries && aliasIndexed.indexedQueries && indexed.peakBytes+aliasIndexed.peakBytes<=8u*1024u*1024u);
    xlsFrontendIrTypeLookupReset(); xlsFrontendTypeAliasReset();
    CHECK(xlsFrontendIrBuildTypeBudget(&other,root,&diags,1) && !diags.count);
    xls_ir_type_lookup_work bounded=xlsFrontendIrTypeLookupWork();
    xls_type_alias_work aliasBounded=xlsFrontendTypeAliasWork();
    actual=dump(&other); CHECK(actual && !strcmp(actual,expected)); xrtFree(actual);
    if(indexed.declarations) CHECK(!bounded.indexes && !bounded.indexedQueries && bounded.linearQueries==reference.linearQueries);
    else CHECK(bounded.indexes==1 && bounded.indexedQueries==reference.queries && !bounded.peakBytes);
    if(aliasIndexed.aliases) CHECK(!aliasBounded.indexes && !aliasBounded.indexedQueries && aliasBounded.linearQueries==aliasReference.linearQueries);
    else CHECK(aliasBounded.indexes==1 && aliasBounded.indexedQueries && !aliasBounded.peakBytes);
    xl6IrModuleFree(&other); xl6DiagFree(&diags); return 1;
}
static int aliasScopes(void)
{
    XL6_ParseResult a,b,c; xl6ParseResultInit(&a); xl6ParseResultInit(&b); xl6ParseResultInit(&c);
    CHECK(xl6ParseSource("typedef First = int\ntypedef Second = First\nfunc value() as int\n return 0\nend\n","alias-a.xl",&a));
    CHECK(xl6ParseSource("typedef First = string\ntypedef Second = First\ntypedef First = float\n","alias-b.xl",&b));
    CHECK(xl6ParseSource("typedef First = Second\ntypedef Second = First\n","alias-cycle.xl",&c));
    XL6_AstNode type={0}; type.kind=XL6_AST_TYPE; type.text="Second";
    const XL6_AstNode *first=xl6TypeResolveAlias(a.root,&type),*second=xl6TypeResolveAlias(b.root,&type);
    CHECK(first && second && first!=second && !strcmp(first->text,"int") && !strcmp(second->text,"string"));
    xls_type_alias_scope outer,inner,disabled;
    xlsFrontendTypeAliasBegin(&outer,a.root,64);
    CHECK(xl6TypeResolveAlias(a.root,&type)==first && xl6TypeResolveAlias(b.root,&type)==second);
    xlsFrontendTypeAliasBegin(&inner,b.root,64);
    CHECK(xl6TypeResolveAlias(b.root,&type)==second && xl6TypeResolveAlias(a.root,&type)==first);
    xlsFrontendTypeAliasBegin(&disabled,b.root,0);
    CHECK(xl6TypeResolveAlias(b.root,&type)==second); xlsFrontendTypeAliasEnd(&disabled);
    CHECK(xl6TypeResolveAlias(b.root,&type)==second); xlsFrontendTypeAliasEnd(&inner);
    CHECK(xl6TypeResolveAlias(a.root,&type)==first); xlsFrontendTypeAliasEnd(&outer);
    CHECK(xl6TypeResolveAlias(a.root,&type)==first && xl6TypeResolveAlias(b.root,&type)==second);
    xlsFrontendTypeAliasBegin(&outer,a.root,1);
    CHECK(xl6TypeResolveAlias(a.root,&type)==first); xlsFrontendTypeAliasEnd(&outer);
    const XL6_AstNode* cycle=xl6TypeResolveAlias(c.root,&type); CHECK(cycle);
    xlsFrontendTypeAliasBegin(&outer,c.root,64);
    CHECK(xl6TypeResolveAlias(c.root,&type)==cycle); xlsFrontendTypeAliasEnd(&outer);
    xl6ParseResultFree(&a); xl6ParseResultFree(&b); xl6ParseResultFree(&c); return 1;
}
static int corpus(const char* source,int large)
{
    XL6_ParseResult parsed; xl6ParseResultInit(&parsed); XL6_IrModule ir; xl6IrModuleInit(&ir);
    int okay=xl6ParseSource(source,"ir-scan-memory.xl",&parsed) && xl6StdlibExpandModule(parsed.root,&parsed.diags) &&
        xl6SemCheckModuleWithIndex(parsed.root,&parsed.diags,"scan",1);
    if(!okay) xl6DiagPrintText(&parsed.diags,"ir-scan-memory.xl");
    CHECK(okay);
    xlsFrontendIrScanReset(); xlsFrontendIrPlanScanReset();
    okay=xl6IrBuildModuleFromAst(&ir,parsed.root,&parsed.diags);
    if(!okay) xl6DiagPrintText(&parsed.diags,"ir-scan-memory.xl");
    CHECK(okay && !parsed.diags.count);
    xls_ir_scan_work indexed=xlsFrontendIrScanWork();
    CHECK(indexed.indexes>=2 && indexed.queries && !indexed.linearVisits && indexed.peakBytes<=16u*1024u*1024u);
    xls_ir_plan_scan_work planIndexed=xlsFrontendIrPlanScanWork();
    CHECK(planIndexed.indexes==1 && planIndexed.queries && !planIndexed.linearVisits && planIndexed.peakBytes<=8u*1024u*1024u);
    char* actual=dump(&ir); CHECK(actual && compareTypeLookups(parsed.root,actual));
    xlsFrontendIrScanReset(); xlsFrontendIrPlanScanReset();
    CHECK(xlsFrontendIrReferenceRefresh(&ir) && xlsFrontendIrReferenceVerify(&ir,&parsed.diags));
    xls_ir_scan_work reference=xlsFrontendIrScanWork(); CHECK(!reference.indexes && reference.linearVisits && !reference.indexedVisits);
    xls_ir_plan_scan_work planReference=xlsFrontendIrPlanScanWork();
    CHECK(!planReference.indexes && !planReference.indexedVisits && planReference.linearVisits);
    char* expected=dump(&ir);
    CHECK(expected && !strcmp(actual,expected)); xrtFree(actual); xrtFree(expected);
    CHECK(initializerValidationFence(&ir));
    CHECK(compareVerify(&ir,1) && compareScopeProof(&ir,large) && compareConversions(&ir,0) && compareRegions(&ir,large));
    if(ir.callSiteCount) {
        size_t count=ir.callSiteCount,capacity=ir.callSiteCapacity;
        XL6_IrCallSite saved=ir.callSites[0];
        ir.callSites[0].origin=0; CHECK(compareConversions(&ir,0) && compareVerify(&ir,-1)); ir.callSites[0]=saved;
        ir.callSites[0].firstArgument=ir.callArgumentCount+1;
        CHECK(compareConversions(&ir,0) && compareVerify(&ir,-1)); ir.callSites[0]=saved;
        XL6_IrCallSite* original=ir.callSites;
        XL6_IrCallSite* duplicate=xrtCalloc(count+1,sizeof(*duplicate)); CHECK(duplicate);
        memcpy(duplicate,original,count*sizeof(*duplicate)); duplicate[count]=original[0];
        ir.callSites=duplicate; ir.callSiteCount=count+1; ir.callSiteCapacity=count+1;
        CHECK(compareConversions(&ir,0) && compareVerify(&ir,-1));
        ir.callSites=original; ir.callSiteCount=count; ir.callSiteCapacity=capacity; xrtFree(duplicate);
        if(count>1) {
            XL6_IrCallSite last=ir.callSites[count-1]; ir.callSites[count-1]=saved; ir.callSites[0]=last;
            CHECK(compareConversions(&ir,0) && compareVerify(&ir,-1)); ir.callSites[0]=saved; ir.callSites[count-1]=last;
        }
        CHECK(compareConversions(&ir,large==2) && compareVerify(&ir,1));
    }
    XL6_DiagList bounded; xl6DiagInit(&bounded); xlsFrontendIrPlanScanReset();
    CHECK(xlsFrontendIrVerifyPlanBudget(&ir,&bounded,1) && !bounded.count);
    xls_ir_plan_scan_work fallback=xlsFrontendIrPlanScanWork();
    CHECK(!fallback.indexes && !fallback.indexedVisits && fallback.linearVisits); xl6DiagFree(&bounded);
    if(large==1) {
        CHECK(reference.linearVisits>indexed.indexedVisits*50 && ir.cleanupActionCount>100);
        printf("IR scan visits indexed=%llu reference=%llu, peak index=%llu bytes\n",(unsigned long long)indexed.indexedVisits,
            (unsigned long long)reference.linearVisits,(unsigned long long)indexed.peakBytes);
    }
    if(large) {
        CHECK(planReference.linearVisits>planIndexed.indexedVisits*20);
        printf("IR exit-plan candidate visits indexed=%llu reference=%llu, data=%llu bytes\n",
            (unsigned long long)planIndexed.indexedVisits,(unsigned long long)planReference.linearVisits,(unsigned long long)planIndexed.peakBytes);
    }
    if(ir.cleanupActionCount) {
        XL6_IrCleanupAction saved=ir.cleanupActions[0]; ir.cleanupActions[0].origin=0;
        CHECK(compareVerify(&ir,0)); ir.cleanupActions[0]=saved;
        ir.cleanupActions[0].targetSymbol=0; CHECK(compareVerify(&ir,0)); ir.cleanupActions[0]=saved;
    }
    if(ir.exitPlanCount) {
        XL6_IrExitPlan saved=ir.exitPlans[0]; ir.exitPlans[0].firstCleanup=ir.cleanupActionCount+1;
        CHECK(compareVerify(&ir,0)); ir.exitPlans[0]=saved;
        ir.exitPlans[0].origin=0; CHECK(compareVerify(&ir,0)); ir.exitPlans[0]=saved;
        --ir.exitPlanCount; CHECK(compareVerify(&ir,0)); ++ir.exitPlanCount;
        XL6_IrExitPlan* original=ir.exitPlans; size_t capacity=ir.exitPlanCapacity,count=ir.exitPlanCount;
        XL6_IrExitPlan* duplicated=xrtCalloc(count+1,sizeof(*duplicated)); CHECK(duplicated);
        memcpy(duplicated,original,count*sizeof(*duplicated)); duplicated[count]=original[0];
        ir.exitPlans=duplicated; ir.exitPlanCapacity=count+1; ir.exitPlanCount=count+1;
        CHECK(compareVerify(&ir,0)); ir.exitPlans=original; ir.exitPlanCapacity=capacity; ir.exitPlanCount=count; xrtFree(duplicated);
        if(count>1) {
            XL6_IrExitPlan last=ir.exitPlans[count-1]; ir.exitPlans[count-1]=ir.exitPlans[0]; ir.exitPlans[0]=last;
            CHECK(compareVerify(&ir,-1)); ir.exitPlans[0]=saved; ir.exitPlans[count-1]=last;
        }
    }
    for(size_t i=0;i<ir.exitPlanCount;++i) if(ir.exitPlans[i].cleanupCount) {
        /* A shortened but in-bounds slice must fail the actual action proof,
         * even after another plan for the same origin was already proven. */
        size_t count=ir.exitPlans[i].cleanupCount; --ir.exitPlans[i].cleanupCount;
        CHECK(compareVerify(&ir,0)); ir.exitPlans[i].cleanupCount=count; break;
    }
    for(size_t i=0;i<ir.nodeCount;++i) if(ir.nodes[i].kind==XL6_IR_NODE_BLOCK) {
        uint32 region=ir.nodes[i].region; ir.nodes[i].region=UINT32_MAX;
        CHECK(compareVerify(&ir,-1)); ir.nodes[i].region=region; break;
    }
    /* The optional preflight/index must never follow an adversarial parent
     * cycle before the shared verifier can issue its normal rejection. */
    XL6_IrNodeId saved=ir.nodes[ir.root-1].parent; ir.nodes[ir.root-1].parent=ir.root;
    CHECK(compareVerify(&ir,0)); ir.nodes[ir.root-1].parent=saved;
    CHECK(compareVerify(&ir,1));
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parsed); return 1;
}
typedef struct reader { const char* source; int passed; } reader;
static int32 readThread(void* user)
{ reader* item=user; item->passed=aliasScopes() && corpus(item->source,0); return 0; }
static const XL6_IrValueInitializer* initializerReference(const XL6_IrModule* module,XL6_IrNodeId id)
{
    xls_ir_initializer_scope disabled;xlsFrontendIrInitializerBegin(&disabled,NULL);
    const XL6_IrValueInitializer* result=xl6IrValueInitializerGet(module,id);
    xlsFrontendIrInitializerEnd(&disabled);return result;
}
static int initializerLookups(void)
{
    XL6_IrNode nodes[8]={0};for(size_t i=0;i<8;++i)nodes[i].kind=XL6_IR_NODE_NAME;
    XL6_IrValueInitializer values[7]={0},other[7]={0};
    const XL6_IrNodeId declarations[]={0,2,2,4,6,8,UINT32_MAX};
    for(size_t i=0;i<7;++i){values[i].declaration=declarations[i];values[i].errorAbi=i==2 || i==3 ? XL6_IR_ERROR_ABI_MAY_RAISE : XL6_IR_ERROR_ABI_NO_RAISE;}
    memcpy(other,values,sizeof(values));
    XL6_IrModule module={.nodes=nodes,.nodeCount=8,.nodeCapacity=8,.valueInitializers=values,.valueInitializerCount=7,.valueInitializerCapacity=7};
    CHECK(!xl6IrValueInitializerGet(NULL,1));
    for(unsigned mode=0;mode<5;++mode){
        if(mode==1){XL6_IrValueInitializer temporary=values[1];values[1]=values[3];values[3]=temporary;}
        if(mode==2)memcpy(values,other,sizeof(values));
        module.valueInitializerCount=mode==3 ? 0 : 7;
        module.valueInitializerCapacity=mode==4 ? 6 : 7;
        xlsFrontendIrInitializerReset();xls_ir_initializer_scope scope;xlsFrontendIrInitializerBegin(&scope,&module);
        for(XL6_IrNodeId id=0;id<=8;++id){
            const XL6_IrValueInitializer* expected=initializerReference(&module,id);
            CHECK(xl6IrValueInitializerGet(&module,id)==expected);
            int wanted=expected && expected->errorAbi==XL6_IR_ERROR_ABI_MAY_RAISE;
            CHECK(xl6IrNodeRequiresThrowExitPlan(&module,id)==wanted);
        }
        CHECK(xl6IrValueInitializerGet(&module,UINT32_MAX)==initializerReference(&module,UINT32_MAX));
        xls_ir_initializer_work work=xlsFrontendIrInitializerWork();
        CHECK(work.queries==work.indexedQueries+work.linearQueries);
        if(mode==1 || mode==4)CHECK(!work.indexes && !work.indexedQueries && work.linearQueries && work.proofRecords<7);
        else CHECK(work.indexes==1 && work.indexedQueries && work.proofRecords==module.valueInitializerCount);
        if(mode==2){
            CHECK(!xl6IrNodeRequiresThrowExitPlan(&module,2));
            values[1].errorAbi=XL6_IR_ERROR_ABI_MAY_RAISE;
            CHECK(xl6IrNodeRequiresThrowExitPlan(&module,2));
            values[1].errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;
        }
        xlsFrontendIrInitializerEnd(&scope);
        xlsFrontendIrInitializerReset();CHECK(xl6IrValueInitializerGet(&module,4)==initializerReference(&module,4));
        work=xlsFrontendIrInitializerWork();CHECK(!work.indexes && !work.indexedQueries && work.linearQueries==2);
    }
    module.valueInitializerCount=module.valueInitializerCapacity=7;
    xls_ir_initializer_scope outer,inner;xlsFrontendIrInitializerBegin(&outer,&module);
    CHECK(xl6IrValueInitializerGet(&module,4)==&values[3]);
    XL6_IrModule second=module;second.valueInitializers=other;
    xlsFrontendIrInitializerBegin(&inner,&second);CHECK(xl6IrValueInitializerGet(&second,4)==&other[3]);
    CHECK(xl6IrValueInitializerGet(&module,4)==&values[3]);xlsFrontendIrInitializerEnd(&inner);
    CHECK(xl6IrValueInitializerGet(&module,4)==&values[3]);
    --module.valueInitializerCount;CHECK(xl6IrValueInitializerGet(&module,4)==initializerReference(&module,4));
    ++module.valueInitializerCount;xlsFrontendIrInitializerReset();CHECK(xl6IrValueInitializerGet(&module,4)==&values[3]);
    CHECK(!xlsFrontendIrInitializerWork().indexedQueries);xlsFrontendIrInitializerEnd(&outer);
    xlsFrontendIrInitializerBegin(&outer,&module);CHECK(xl6IrValueInitializerGet(&module,4)==&values[3]);
    module.valueInitializers=other;CHECK(xl6IrValueInitializerGet(&module,4)==&other[3]);
    module.valueInitializers=values;xlsFrontendIrInitializerReset();CHECK(xl6IrValueInitializerGet(&module,4)==&values[3]);
    CHECK(!xlsFrontendIrInitializerWork().indexedQueries);xlsFrontendIrInitializerEnd(&outer);
    /* Actual absent-key probes exercise the former nodes x initializers
       scan,without allocating another table or changing its first match. */
    XL6_IrValueInitializer* large=xrtCalloc(4096,sizeof(*large));CHECK(large);
    for(size_t i=0;i<4096;++i)large[i].declaration=(XL6_IrNodeId)(i*2+2);
    module.valueInitializers=large;module.valueInitializerCount=module.valueInitializerCapacity=4096;
    xlsFrontendIrInitializerReset();xlsFrontendIrInitializerBegin(&outer,&module);
    for(XL6_IrNodeId id=1;id<8193;++id)CHECK(xl6IrValueInitializerGet(&module,id)==(id%2 ? NULL : &large[id/2-1]));
    xlsFrontendIrInitializerEnd(&outer);xls_ir_initializer_work work=xlsFrontendIrInitializerWork();
    CHECK(work.indexes==1 && work.proofRecords==4096 && work.indexedQueries==8192 && !work.linearQueries && work.probes<8192*15u);
    printf("IR initializer probes=%llu;8192queries/4096records,first-match andlive ABI preserved\n",(unsigned long long)work.probes);
    xrtFree(large);return 1;
}
static int run(void)
{
    CHECK(initializerLookups());
    CHECK(aliasScopes());
    const char* sources[]={
        "func main(...) as int\n int value = 7\n return value\nend\n",
        "string global = \"global\"\nfunc choose(bool yes) as string\n string outer = \"outer\"\n if yes\n string branch = \"branch\"\n return branch\n else\n string other = \"other\"\n return other\n end\nend\n",
        "func loops() as int\n int result = 0\n for outer in range(2)\n string first = \"first\"\n for inner in range(3)\n string second = \"second\"\n if inner == 1 then\n break 2\n end\n result += inner\n end\n end\n return result\nend\n",
        "func closure() as func() as int\n array values = [41]\n return func() as int\n return int(values[0])\n end\nend\n",
        "func guarded() as int\n string outer = \"outer\"\n try\n string inner = \"inner\"\n return 7\n catch e\n string message = e.message\n return 8\n finally\n string final = \"final\"\n end\nend\n",
        "func values(int count) as generator<string>\n string suffix = \"owned\"\n for i in range(count)\n string item = suffix + string(i)\n yield item\n end\nend\n",
        "class Box\n string item = \"owned\"\n func value() as string\n string local = self.item\n return local\n end\nend\n",
        "protocol Named\n func name() as string\nend\nfunc named(Named item) as int\n return 0\nend\n",
        "typedef Count = int\ntypedef Other = Count\nfunc aliases(Other value) as Count\n return value\nend\n",
        "enum Choice\n Some(string value)\n None\nend\nfunc chosen(Choice item) as string\n match item\n case Some(value)\n return value\n case None\n return \"empty\"\n end\nend\n",
        "func apply(func(int) as int action, int value) as int\n return action(value)\nend\nfunc run() as int\n return apply(func(int value) as int\n return value + 1\n end, 7)\nend\n",
        "func accept(array items) as int\n return int(items[0])\nend\nfunc erased() as int\n return accept([1, 2, 3])\nend\n",
        "func accept(func() as int callback) as int\n return callback()\nend\nfunc erased(var boxed) as int\n return accept(boxed)\nend\n"
    };
    for(size_t i=0;i<sizeof(sources)/sizeof(sources[0]);++i) { printf("IR corpus %zu\n",i); CHECK(corpus(sources[i],0)); }
    xstrbuf large; xrtStrBufInit(&large);
    for(unsigned i=0;i<250;++i) CHECK(xrtStrBufAppendFormat(&large,
        "func helper_%u() as int\n string item = \"owned\"\n int result = %u\n return result\nend\n",i,i));
    CHECK(corpus(large.Data,1));
    xrtStrBufFree(&large); xrtStrBufInit(&large);
    CHECK(xrtStrBufAppendFormat(&large,"func choose<T>(T value) as T\n return value\nend\n"));
    for(unsigned i=0;i<30;++i) {
        CHECK(xrtStrBufAppendFormat(&large,"func generic_%u(int value) as int\n int result = value\n",i));
        for(unsigned j=0;j<50;++j) CHECK(xrtStrBufAppendFormat(&large," result = choose::<int>(result + 1)\n"));
        CHECK(xrtStrBufAppendFormat(&large," return result\nend\n"));
    }
    CHECK(corpus(large.Data,2));
    reader readers[4]; xthread* threads[4];
    for(unsigned i=0;i<4;++i) { readers[i]=(reader){sources[2+i],0}; threads[i]=xrtThreadCreate(readThread,&readers[i],0); CHECK(threads[i]); }
    for(unsigned i=0;i<4;++i) { CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].passed); xrtThreadDestroy(threads[i]); }
    CHECK(xrtAtomic32Load(&checkedBoundaries,XMEMORY_RELAXED)>0);
    xrtStrBufFree(&large); return 1;
}
int main(void)
{ xrtAtomic32Init(&checks,0); xrtAtomic32Init(&checkedBoundaries,0); if(!run()) return 1; printf("analysis_ir_scan_test passed: %u checks; full IR dump/reference-scan parity, cleanup/plan/call order, successful per-verification proofs, checked conversion boundaries, corrupt input rejection, tiny-budget fallback and concurrent TLS isolation.\n",(unsigned)xrtAtomic32Load(&checks,XMEMORY_RELAXED)); return 0; }
