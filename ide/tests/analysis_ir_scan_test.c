#include "analysis/frontend_ir.h"
#include "src/parser/xl6_parser_api.h"
#include "src/stdlib/xl6_stdlib.h"
#include "src/sem/xl6_sem.h"
#include <stdio.h>
#include <string.h>

static xatomic32 checks;
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
    char* actual=dump(&ir); CHECK(actual);
    xlsFrontendIrScanReset(); xlsFrontendIrPlanScanReset();
    CHECK(xlsFrontendIrReferenceRefresh(&ir) && xlsFrontendIrReferenceVerify(&ir,&parsed.diags));
    xls_ir_scan_work reference=xlsFrontendIrScanWork(); CHECK(!reference.indexes && reference.linearVisits && !reference.indexedVisits);
    xls_ir_plan_scan_work planReference=xlsFrontendIrPlanScanWork();
    CHECK(!planReference.indexes && !planReference.indexedVisits && planReference.linearVisits);
    char* expected=dump(&ir);
    CHECK(expected && !strcmp(actual,expected)); xrtFree(actual); xrtFree(expected);
    CHECK(compareVerify(&ir,1));
    XL6_DiagList bounded; xl6DiagInit(&bounded); xlsFrontendIrPlanScanReset();
    CHECK(xlsFrontendIrVerifyPlanBudget(&ir,&bounded,1) && !bounded.count);
    xls_ir_plan_scan_work fallback=xlsFrontendIrPlanScanWork();
    CHECK(!fallback.indexes && !fallback.indexedVisits && fallback.linearVisits); xl6DiagFree(&bounded);
    if(large) {
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
    /* The optional preflight/index must never follow an adversarial parent
     * cycle before the shared verifier can issue its normal rejection. */
    XL6_IrNodeId saved=ir.nodes[ir.root-1].parent; ir.nodes[ir.root-1].parent=ir.root;
    CHECK(compareVerify(&ir,0)); ir.nodes[ir.root-1].parent=saved;
    CHECK(compareVerify(&ir,1));
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parsed); return 1;
}
typedef struct reader { const char* source; int passed; } reader;
static int32 readThread(void* user)
{ reader* item=user; item->passed=corpus(item->source,0); return 0; }
static int run(void)
{
    const char* sources[]={
        "func main(...) as int\n int value = 7\n return value\nend\n",
        "string global = \"global\"\nfunc choose(bool yes) as string\n string outer = \"outer\"\n if yes\n string branch = \"branch\"\n return branch\n else\n string other = \"other\"\n return other\n end\nend\n",
        "func loops() as int\n int result = 0\n for outer in range(2)\n string first = \"first\"\n for inner in range(3)\n string second = \"second\"\n if inner == 1 then\n break 2\n end\n result += inner\n end\n end\n return result\nend\n",
        "func closure() as func() as int\n array values = [41]\n return func() as int\n return int(values[0])\n end\nend\n",
        "func guarded() as int\n string outer = \"outer\"\n try\n string inner = \"inner\"\n return 7\n catch e\n string message = e.message\n return 8\n finally\n string final = \"final\"\n end\nend\n",
        "func values(int count) as generator<string>\n string suffix = \"owned\"\n for i in range(count)\n string item = suffix + string(i)\n yield item\n end\nend\n",
        "class Box\n string item = \"owned\"\n func value() as string\n string local = self.item\n return local\n end\nend\n",
        "enum Choice\n Some(string value)\n None\nend\nfunc chosen(Choice item) as string\n match item\n case Some(value)\n return value\n case None\n return \"empty\"\n end\nend\n"
    };
    for(size_t i=0;i<sizeof(sources)/sizeof(sources[0]);++i) { printf("IR corpus %zu\n",i); CHECK(corpus(sources[i],0)); }
    xstrbuf large; xrtStrBufInit(&large);
    for(unsigned i=0;i<250;++i) CHECK(xrtStrBufAppendFormat(&large,
        "func helper_%u() as int\n string item = \"owned\"\n int result = %u\n return result\nend\n",i,i));
    CHECK(corpus(large.Data,1));
    reader readers[4]; xthread* threads[4];
    for(unsigned i=0;i<4;++i) { readers[i]=(reader){sources[2+i],0}; threads[i]=xrtThreadCreate(readThread,&readers[i],0); CHECK(threads[i]); }
    for(unsigned i=0;i<4;++i) { CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].passed); xrtThreadDestroy(threads[i]); }
    xrtStrBufFree(&large); return 1;
}
int main(void)
{ xrtAtomic32Init(&checks,0); if(!run()) return 1; printf("analysis_ir_scan_test passed: %u checks; full IR dump/reference-scan parity, cleanup/plan order and missing/duplicate/corrupt plan rejection, tiny-budget fallback, actual candidate work and concurrent TLS isolation.\n",(unsigned)xrtAtomic32Load(&checks,XMEMORY_RELAXED)); return 0; }
