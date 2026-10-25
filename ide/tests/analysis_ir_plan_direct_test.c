#define main irPlanDirectBaseMain
#include "analysis_ir_scan_test.c"
#undef main
static size_t directCalls,directFail;
static bool directInit(xarray* array,size_t size)
{return ++directCalls!=directFail && xrtArrayInit(array,size);}
static bool directResize(xarray* array,size_t size)
{return ++directCalls!=directFail && xrtArrayResize(array,size);}
#define xrtArrayInit directInit
#define xrtArrayResize directResize
#include "analysis/frontend_ir_plan_direct.h"
#undef xrtArrayInit
#undef xrtArrayResize
static uint64 directLive(void)
{xmemstats work;xrtMemStatsGet(&work);return work.BlockAllocCalls-work.BlockFreeCalls;}
static int directEnumeration(void)
{
    const size_t nodes=513,count=4096;XL6_IrExitPlan* plans=xrtCalloc(count,sizeof(*plans));CHECK(plans);
    for(size_t i=0;i<count;++i)plans[i].origin=(XL6_IrNodeId)((i*197u+i/11u)%nodes);
    XL6_IrModule module={.nodeCount=nodes,.exitPlans=plans,.exitPlanCount=count,.exitPlanCapacity=count};
    xls_ir_plan_direct index={0};uint64 before=directLive();directCalls=directFail=0;
    CHECK(xlsIrPlanDirectBuild(&index,&module,1024*1024) && index.bytes<=1024*1024);size_t operations=directCalls;
    for(XL6_IrNodeId origin=0;origin<nodes+2;++origin)for(unsigned variant=0;variant<5;++variant){
        size_t limit=(size_t[]){0,1,73,1003,count}[variant],at=xlsIrPlanDirectFirst(&index,origin,limit);
        for(size_t i=0;i<limit;++i)if(plans[i].origin==origin){CHECK(at==i);at=xlsIrPlanDirectNext(&index,at,limit);}
        CHECK(at==limit);
    }
    CHECK(xlsIrPlanDirectNext(&index,count,SIZE_MAX)==SIZE_MAX);xlsIrPlanDirectClear(&index);CHECK(directLive()==before);
    for(size_t fail=1;fail<=operations;++fail){directCalls=0;directFail=fail;CHECK(!xlsIrPlanDirectBuild(&index,&module,1024*1024));CHECK(!index.heads.Data && !index.next.Data && !index.bytes && directLive()==before);}
    directCalls=directFail=0;
    for(unsigned variant=0;variant<4;++variant){size_t bytes=(size_t[]){0,1,255,4096}[variant];CHECK(!xlsIrPlanDirectBuild(&index,&module,bytes) && directLive()==before);}
    XL6_IrNodeId saved=plans[count/2].origin;plans[count/2].origin=UINT32_MAX;
    CHECK(!xlsIrPlanDirectBuild(&index,&module,1024*1024) && directLive()==before);plans[count/2].origin=saved;
    CHECK(xlsIrPlanDirectBuild(&index,&module,1024*1024));module.exitPlanCount=count+1;
    CHECK(!xlsIrPlanDirectBuild(&index,&module,1024*1024) && !index.heads.Data && !index.next.Data && directLive()==before);module.exitPlanCount=count;
    module.nodeCount=SIZE_MAX;CHECK(!xlsIrPlanDirectBuild(&index,&module,SIZE_MAX) && directLive()==before);
    /* A dense arena may fit even when the previous sorted representation's
     * conservative admission would refuse it. Its own budget is authoritative. */
    module.nodeCount=1;module.exitPlanCount=module.exitPlanCapacity=512;
    for(size_t i=0;i<512;++i)plans[i].origin=(XL6_IrNodeId)(i%2);
    CHECK(xlsIrPlanDirectBuild(&index,&module,16384) && index.bytes<=16384);
    for(XL6_IrNodeId origin=0;origin<2;++origin){size_t at=xlsIrPlanDirectFirst(&index,origin,512);
        for(size_t i=origin;i<512;i+=2){CHECK(at==i);at=xlsIrPlanDirectNext(&index,at,512);}
        CHECK(at==512);
    }
    xlsIrPlanDirectClear(&index);xrtFree(plans);printf("Direct plan index original-order/duplicate/limit queries and %zu allocation seams checked\n",operations);return 1;
}
static int directModule(void)
{
    xstrbuf source={0};for(unsigned i=0;i<48;++i)CHECK(xrtStrBufAppendFormat(&source,"func worker_%u(int value) as int\n int local=value\n if local>0 then\n return local+1\n end\n return 0\nend\n",i));
    CHECK(xrtStrBufAppend(&source,xrtStrView("func main(...) as int\n return worker_0(7)\nend\n")));
    XL6_ParseResult parsed;xl6ParseResultInit(&parsed);XL6_IrModule ir;xl6IrModuleInit(&ir);
    CHECK(xl6ParseSource(source.Data,"direct-plan.xl",&parsed) && xl6StdlibExpandModule(parsed.root,&parsed.diags) && xl6SemCheckModuleWithIndex(parsed.root,&parsed.diags,"direct",1) && xl6IrBuildModuleFromAst(&ir,parsed.root,&parsed.diags));
    char* original=dump(&ir);CHECK(original);
    struct direct_sorted_record {XL6_IrNodeId origin;size_t ordinal;};
    size_t sortedBudget=2*ir.exitPlanCount*(sizeof(struct direct_sorted_record)+sizeof(size_t));
    for(unsigned b=0;b<4;++b){size_t budget=(size_t[]){0,1,sortedBudget,8u*1024u*1024u}[b];XL6_DiagList a,z;xl6DiagInit(&a);xl6DiagInit(&z);xlsFrontendIrPlanScanReset();
        CHECK(xlsFrontendIrVerifyPlanBudget(&ir,&a,budget));xls_ir_plan_scan_work work=xlsFrontendIrPlanScanWork();
        CHECK(xlsFrontendIrReferenceVerify(&ir,&z) && sameDiagnostics(&a,&z) && !a.count && !z.count);
        if(b<2)CHECK(!work.indexes && !work.directIndexes && !work.directQueries && work.linearVisits);
        if(b==2)CHECK(work.indexes==1 && !work.directIndexes && !work.directQueries && work.indexedVisits);
        if(b==3)CHECK(work.indexes==1 && work.directIndexes==1 && work.directQueries>work.indexedVisits && work.indexedVisits && work.peakBytes<=budget);
        char* current=dump(&ir);CHECK(current && !strcmp(current,original));xrtFree(current);xl6DiagFree(&a);xl6DiagFree(&z);
        printf("Native plan budget=%zu nodes=%zu plans=%zu direct=%llu queries=%llu capacity=%llu\n",budget,ir.nodeCount,ir.exitPlanCount,(unsigned long long)work.directIndexes,(unsigned long long)work.directQueries,(unsigned long long)work.peakBytes);
    }
    for(unsigned b=0;b<3;++b){size_t budget=(size_t[]){0,1,8u*1024u*1024u}[b];XL6_DiagList a,z;xl6DiagInit(&a);xl6DiagInit(&z);xlsFrontendIrPlanScanReset();
        CHECK(xlsFrontendIrVerifyPlanDirectBudget(&ir,&a,budget));xls_ir_plan_scan_work work=xlsFrontendIrPlanScanWork();
        CHECK(xlsFrontendIrReferenceVerify(&ir,&z) && sameDiagnostics(&a,&z) && !a.count && !z.count);
        CHECK(work.indexes==1 && work.indexedVisits && work.directIndexes==(b==2) && (work.directQueries>0)==(b==2));
        char* current=dump(&ir);CHECK(current && !strcmp(current,original));xrtFree(current);xl6DiagFree(&a);xl6DiagFree(&z);
    }
    for(size_t i=0;i<ir.exitPlanCount;i+=ir.exitPlanCount/7+1){XL6_IrNodeId old=ir.exitPlans[i].origin;ir.exitPlans[i].origin=0;CHECK(compareVerify(&ir,0));ir.exitPlans[i].origin=old;CHECK(compareVerify(&ir,1));}
    if(ir.exitPlanCount>1){XL6_IrExitPlan saved=ir.exitPlans[1];ir.exitPlans[1]=ir.exitPlans[0];CHECK(compareVerify(&ir,0));ir.exitPlans[1]=saved;CHECK(compareVerify(&ir,1));}
    xrtFree(original);xl6IrModuleFree(&ir);xl6ParseResultFree(&parsed);xrtStrBufFree(&source);return 1;
}
typedef struct direct_reader {int passed;} direct_reader;
static int32 directRead(void* user)
{direct_reader* reader=user;reader->passed=directModule();return 0;}
int main(void)
{
    xrtAtomic32Init(&checks,0);xrtMemStatsEnable(true);uint64 before=directLive();
    if(!directEnumeration() || !directModule())return 1;
    direct_reader readers[4];xthread* threads[4];for(unsigned i=0;i<4;++i){readers[i]=(direct_reader){0};threads[i]=xrtThreadCreate(directRead,&readers[i],0);CHECK(threads[i]);}
    for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].passed);xrtThreadDestroy(threads[i]);}
    CHECK(directLive()==before);printf("analysis_ir_plan_direct_test passed: %u checks;original enumeration,quota/allocator fallback,native original diagnostics/IR identity,corruption/recovery,concurrent independent verifier scopes and balanced storage\n",(unsigned)xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;
}
