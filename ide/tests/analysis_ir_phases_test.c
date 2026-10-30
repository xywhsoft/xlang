#include "analysis/frontend_internal.h"
#include "analysis/frontend_ir.h"
#include "analysis/frontend_cancel.h"
#include "analysis/ast_clone.h"
#include "analysis_ast_same.h"
#include "src/sem/xl6_sem.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
static unsigned phaseChecks;
static _Thread_local unsigned phaseReserveFault,phaseReserveCalls;
static _Thread_local int phaseReserveTrack;
bool __real_xrtArrayReserve(xarray*,size_t);
bool __wrap_xrtArrayReserve(xarray* array,size_t count)
{if(phaseReserveTrack && ++phaseReserveCalls==phaseReserveFault)return false;return __real_xrtArrayReserve(array,count);}
int xlsReferenceIrBuildModuleFromAst(XL6_IrModule*,const XL6_AstNode*,XL6_DiagList*);
int xlsReferenceIrVerifyModule(const XL6_IrModule*,XL6_DiagList*);
int xlsReferenceIrRefreshExitPlans(XL6_IrModule*);
void xlsReferenceIrModuleFree(XL6_IrModule*);
void xlsReferenceIrDumpModuleJson(XL6_Output*,const XL6_IrModule*);
#define PHASE_CHECK(v) do{++phaseChecks;if(!(v)){fprintf(stderr,"analysis_ir_phases_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static uint64_t phaseLive(void)
{xmemstats stats;xrtMemStatsGet(&stats);return stats.BlockAllocCalls-stats.BlockFreeCalls;}
static char* phaseDump(const XL6_IrModule* module,int original)
{
    XL6_Output out;if(!xl6OutputOpenMemory(&out))return NULL;
    if(original)xlsReferenceIrDumpModuleJson(&out,module);else xl6IrDumpModuleJson(&out,module);
    char* value=xl6OutputHasFailed(&out) ? NULL : xl6OutputTakeMemoryText(&out,NULL);xl6OutputDiscard(&out);return value;
}
static int phaseDiags(const XL6_DiagList* a,const XL6_DiagList* b)
{
    if(a->count!=b->count)return 0;
    for(size_t i=0;i<a->count;++i)if(a->items[i].line!=b->items[i].line || a->items[i].column!=b->items[i].column || strcmp(a->items[i].message,b->items[i].message))return 0;
    return 1;
}
static int phaseVerify(XL6_IrModule* module,int accepted)
{
    XL6_DiagList reference;xl6DiagInit(&reference);int original=xlsReferenceIrVerifyModule(module,&reference);
    if(original!=accepted){for(size_t i=0;i<reference.count;++i)fprintf(stderr,"original verifier: %s\n",reference.items[i].message);}
    PHASE_CHECK(original==accepted);
    const size_t budgets[]={0,1,8u*1024u*1024u};
    for(unsigned i=0;i<3;++i){XL6_DiagList diags;xl6DiagInit(&diags);PHASE_CHECK(xlsFrontendIrVerifyPlanBudget(module,&diags,budgets[i])==accepted && phaseDiags(&diags,&reference));xl6DiagFree(&diags);}
    XL6_DiagList diags;xl6DiagInit(&diags);PHASE_CHECK(xlsFrontendIrVerifyPlanDirectBudget(module,&diags,0)==accepted && phaseDiags(&diags,&reference));xl6DiagFree(&diags);xl6DiagFree(&reference);return 1;
}
typedef struct phase_trigger {enum xls_frontend_cancel_kind kind;uint64_t after,seen;int fired;} phase_trigger;
static int phasePredicate(void* data)
{
    phase_trigger* trigger=data;xls_frontend_cancel_work work=xlsFrontendCancelWork();
    if(!trigger->fired && work.kind==trigger->kind && work.points[trigger->kind]>=trigger->after){trigger->fired=1;trigger->seen=work.points[trigger->kind];return 1;}return 0;
}
static int phaseCancel(const XL6_AstNode* root,XL6_IrModule* held)
{
    char* before=phaseDump(held,0);PHASE_CHECK(before);size_t nodes=held->nodeCount;PHASE_CHECK(nodes>128);
    /* Entry and all 29 original transformation boundaries, including the
     * dispatch-table NULL result arm. Each abort frees the partial module. */
    for(unsigned at=1;at<=30;++at){
        uint64_t live=phaseLive();XL6_IrModule module={0};XL6_DiagList diags;xl6DiagInit(&diags);phase_trigger trigger={.kind=XLS_CANCEL_IR_PHASE,.after=at};xls_frontend_cancel_scope scope;
        xlsFrontendCancelBegin(&scope,phasePredicate,&trigger);int accepted=xl6IrBuildModuleFromAst(&module,root,&diags);xls_frontend_cancel_work work;xlsFrontendCancelEnd(&scope,&work);
        PHASE_CHECK(!accepted && trigger.fired && trigger.seen>=at && work.cancelled && !module.nodes && !module.nodeCount);
        PHASE_CHECK(!xlsActiveFrontendCancel && !xlsFrontendIrInitializerActive());xl6IrModuleFree(&module);xl6DiagFree(&diags);PHASE_CHECK(phaseLive()==live);
    }
    for(unsigned at=0;at<2;++at){
        uint64_t live=phaseLive();XL6_IrModule module={0};XL6_DiagList diags;xl6DiagInit(&diags);phase_trigger trigger={.kind=at ? XLS_CANCEL_IR_PHASE : XLS_CANCEL_IR_NODE,.after=at ? 31 : 65};xls_frontend_cancel_scope scope;
        xlsFrontendCancelBegin(&scope,phasePredicate,&trigger);int accepted=xl6IrBuildModuleFromAst(&module,root,&diags);xlsFrontendCancelEnd(&scope,NULL);
        PHASE_CHECK(!accepted && trigger.fired && !xlsActiveFrontendCancel && !xlsFrontendIrInitializerActive());xl6IrModuleFree(&module);xl6DiagFree(&diags);PHASE_CHECK(phaseLive()==live);
    }
    /* Four complete node loops precede the incoming arena. Exercise both
     * loops that own it and the loop after its ordinary release. */
    for(unsigned loop=4;loop<7;++loop){
        uint64_t live=phaseLive();XL6_DiagList diags;xl6DiagInit(&diags);phase_trigger trigger={.kind=XLS_CANCEL_IR_VERIFY,.after=loop*nodes+65};xls_frontend_cancel_scope scope;
        xlsFrontendCancelBegin(&scope,phasePredicate,&trigger);int accepted=xl6IrVerifyModule(held,&diags);xlsFrontendCancelEnd(&scope,NULL);
        PHASE_CHECK(!accepted && trigger.fired && !xlsActiveFrontendCancel && !xlsFrontendIrInitializerActive());xl6DiagFree(&diags);PHASE_CHECK(phaseLive()==live && phaseVerify(held,1));
    }
    char* after=phaseDump(held,0);PHASE_CHECK(after && !strcmp(before,after));xrtFree(before);xrtFree(after);return 1;
}
int xlsTestCurrentIrPhases(void)
{
    xstrbuf source={0};PHASE_CHECK(xrtStrBufAppend(&source,xrtStrView("func leaf(int value) as int\n return value+1\nend\n")));
    for(unsigned f=0;f<4;++f){
        PHASE_CHECK(xrtStrBufAppendFormat(&source,"func work_%u(int value) as int\n string retained=\"held\"\n int total=value\n try\n",f));
        for(unsigned n=0;n<24;++n)PHASE_CHECK(xrtStrBufAppend(&source,xrtStrView(" total=leaf(total)\n")));
        PHASE_CHECK(xrtStrBufAppend(&source,xrtStrView(" return total\n catch e\n string message=e.message\n return 8\n finally\n string final=\"final\"\n end\nend\n")));
    }
    char* path=xrtPathAbs("current-ir-phases.xl");PHASE_CHECK(path);XL6_ParseResult parsed;xl6ParseResultInit(&parsed);
    PHASE_CHECK(xl6ParseSource(source.Data,path,&parsed) && xl6SemCheckModuleWithIndex(parsed.root,&parsed.diags,"phases",1));XL6_AstNode* unchanged=xlsAstClone(parsed.root,NULL,NULL);PHASE_CHECK(unchanged);
    XL6_DiagList a,b;xl6DiagInit(&a);xl6DiagInit(&b);XL6_IrModule actual={0},original={0};PHASE_CHECK(xlsReferenceIrBuildModuleFromAst(&original,parsed.root,&b));
    xlsFrontendIrScanReset();xlsFrontendIrPlanScanReset();xlsFrontendIrCallScanReset();xlsFrontendIrScopeProofReset();
    PHASE_CHECK(xl6IrBuildModuleFromAst(&actual,parsed.root,&a) && !a.count && !b.count);
    PHASE_CHECK(xlsFrontendIrScanWork().indexedVisits>0 && xlsFrontendIrPlanScanWork().directQueries>0 && xlsFrontendIrCallScanWork().indexedVisits>0 && xlsFrontendIrScopeProofWork().hits>0);
    char *left=phaseDump(&actual,0),*right=phaseDump(&original,1);PHASE_CHECK(left && right && !strcmp(left,right));xrtFree(left);xrtFree(right);PHASE_CHECK(phaseVerify(&actual,1));
    XL6_IrNodeId savedRoot=actual.root;actual.root=0;PHASE_CHECK(phaseVerify(&actual,0));actual.root=savedRoot;
    size_t savedCapacity=actual.callSiteCapacity;PHASE_CHECK(actual.callSiteCount>0);actual.callSiteCapacity=actual.callSiteCount-1;PHASE_CHECK(phaseVerify(&actual,0));actual.callSiteCapacity=savedCapacity;
    size_t count=actual.callSiteCount;XL6_IrCallSite* saved=actual.callSites;XL6_IrCallSite* edited=xrtCalloc(count+1,sizeof(*edited));PHASE_CHECK(edited);memcpy(edited,saved,count*sizeof(*edited));edited[count]=saved[0];actual.callSites=edited;actual.callSiteCount=actual.callSiteCapacity=count+1;PHASE_CHECK(phaseVerify(&actual,0));
    /* Argument-bearing descriptors cannot be permuted independently of the
     * canonical argument arena. Keep the original rejection. The existing
     * scan corpus separately admits argument-free descriptor permutations. */
    actual.callSiteCount=count;for(size_t i=0;i<count;++i)edited[i]=saved[count-1-i];PHASE_CHECK(phaseVerify(&actual,0));actual.callSites=saved;actual.callSiteCapacity=savedCapacity;xrtFree(edited);
    PHASE_CHECK(actual.valueInitializerCount>0);XL6_IrErrorAbi error=actual.valueInitializers[0].errorAbi;actual.valueInitializers[0].errorAbi=(XL6_IrErrorAbi)INT_MAX;xlsFrontendIrInitializerReset();PHASE_CHECK(phaseVerify(&actual,0));PHASE_CHECK(!xlsFrontendIrInitializerWork().indexes && !xlsFrontendIrInitializerWork().proofRecords);actual.valueInitializers[0].errorAbi=error;
    for(unsigned fault=1;fault<=3;++fault){uint64_t live=phaseLive();phaseReserveTrack=1;phaseReserveFault=fault;phaseReserveCalls=0;PHASE_CHECK(phaseVerify(&actual,1));PHASE_CHECK(phaseReserveCalls>=fault);phaseReserveTrack=0;PHASE_CHECK(phaseLive()==live);}
    PHASE_CHECK(phaseCancel(parsed.root,&actual));PHASE_CHECK(xl6IrRefreshExitPlans(&actual) && xlsReferenceIrRefreshExitPlans(&original));left=phaseDump(&actual,0);right=phaseDump(&original,1);PHASE_CHECK(left && right && !strcmp(left,right));xrtFree(left);xrtFree(right);PHASE_CHECK(sameAst(unchanged,parsed.root));
    xl6IrModuleFree(&actual);xlsReferenceIrModuleFree(&original);xl6AstFree(unchanged);xl6ParseResultFree(&parsed);xl6DiagFree(&a);xl6DiagFree(&b);xrtFree(path);xrtStrBufFree(&source);
    printf("analysis_ir_phases_test passed: %u checks; original IR/diagnostics,budget and allocation fallbacks,all 29 transformation aborts,owned verifier arena cancellation\n",phaseChecks);return 1;
}
