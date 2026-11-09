#include "analysis/frontend_internal.h"
#include "analysis/frontend_cancel.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
static size_t topologyCalls,traceNodes;
static int tracing,denyTopology;
static void* trackTopologyCalloc(size_t count,size_t bytes,const char* expression)
{
    if(tracing && count==traceNodes && bytes==1 && !strcmp(expression,"module->nodeCount")){
        ++topologyCalls;if(denyTopology){denyTopology=0;return NULL;}
    }
    return xrtCalloc(count,bytes);
}
#define xrtCalloc(count,bytes) trackTopologyCalloc((count),(bytes),#count)
#include "xl6_ir.c"
#undef xrtCalloc
#ifdef XLS_NATIVE_IR_TOPOLOGY_OBSERVER
#include "src/parser/xl6_parser_api.h"
#include "src/stdlib/xl6_stdlib.h"
#include "src/sem/xl6_sem.h"
static unsigned checks,modules,comparisons,guarded;
#define CHECK(value) do{++checks;if(!(value)){fprintf(stderr,"ir-topology-test:%d:%s\n",__LINE__,#value);return 0;}}while(0)
static int sameDiags(const XL6_DiagList* a,const XL6_DiagList* b)
{
    if(a->count!=b->count)return 0;
    for(size_t i=0;i<a->count;++i)if(a->items[i].line!=b->items[i].line || a->items[i].column!=b->items[i].column ||
        strcmp(a->items[i].message,b->items[i].message))return 0;
    return 1;
}
static char* moduleJson(const XL6_IrModule* module)
{
    XL6_Output out;if(!xl6OutputOpenMemory(&out))return NULL;xl6IrDumpModuleJson(&out,module);
    char* result=xl6OutputHasFailed(&out)?NULL:xl6OutputTakeMemoryText(&out,NULL);xl6OutputDiscard(&out);return result;
}
static int verifyPair(const XL6_IrModule* module,int expected,size_t calls,int fail,int indexed)
{
    XL6_DiagList a,b;xl6DiagInit(&a);xl6DiagInit(&b);traceNodes=module?module->nodeCount:0;
    xlsFrontendIrScanReset();tracing=1;topologyCalls=0;denyTopology=fail;
    int actual=xl6IrVerifyModule(module,&a);size_t actualCalls=topologyCalls;int actualDenied=denyTopology;
    xls_ir_scan_work work=xlsFrontendIrScanWork();tracing=0;
    xlsFrontendIrScanReset();tracing=1;topologyCalls=0;denyTopology=fail;
    int reference=xlsFrontendIrReferenceVerify(module,&b);size_t referenceCalls=topologyCalls;int referenceDenied=denyTopology;
    tracing=0;denyTopology=0;
    CHECK(actual==reference && actual==expected && sameDiags(&a,&b));
    CHECK(actualCalls==calls && referenceCalls==calls && actualDenied==referenceDenied);
    CHECK(indexed?work.indexes==1:work.indexes==0);
    CHECK(!xlsActiveIrScan);++comparisons;xl6DiagFree(&a);xl6DiagFree(&b);return 1;
}
static int hookGuards(const XL6_IrModule* module)
{
    xls_ir_scan* previous=xlsActiveIrScan;
    for(unsigned mode=0;mode<3;++mode){
        xls_ir_scan scan; xlsIrScanBegin(&scan);scan.verifying=mode!=0;scan.topologyPending=mode!=1;
        scan.module=mode==2?NULL:module;xlsActiveIrScan=&scan;
        xlsIrScanVerifyTopologyReady(module);CHECK(!scan.topology && !scan.ready && !scan.all.Data && !scan.routed.Data);
        xlsIrScanDone(&scan);CHECK(xlsActiveIrScan==previous);
    }
    return 1;
}
static int corpus(const char* source)
{
    XL6_ParseResult parsed;xl6ParseResultInit(&parsed);XL6_IrModule module;xl6IrModuleInit(&module);
    int okay=xl6ParseSource(source,"topology-unit.xl",&parsed) && xl6StdlibExpandModule(parsed.root,&parsed.diags) &&
        xl6SemCheckModuleWithIndex(parsed.root,&parsed.diags,"topology",1) && xl6IrBuildModuleFromAst(&module,parsed.root,&parsed.diags);
    if(!okay)xl6DiagPrintText(&parsed.diags,"topology-unit.xl");CHECK(okay && !parsed.diags.count && module.nodeCount>8);
    char* before=moduleJson(&module);CHECK(before);
    CHECK(verifyPair(&module,1,1,0,1));CHECK(verifyPair(&module,0,1,1,0));CHECK(hookGuards(&module));
    /* The normal/reference selectors remain correctly nested. */
    xls_ir_scan outer={.originalClassifiers=1};xlsActiveIrScan=&outer;XL6_DiagList nested;xl6DiagInit(&nested);
    CHECK(xl6IrVerifyModule(&module,&nested) && !nested.count && xlsActiveIrScan==&outer && outer.originalClassifiers);
    xl6DiagFree(&nested);xlsActiveIrScan=NULL;
    for(size_t i=0;i<module.nodeCount;++i){
        XL6_IrNode saved=module.nodes[i];
        module.nodes[i].parent=UINT32_MAX;CHECK(verifyPair(&module,0,1,0,0));module.nodes[i]=saved;
        module.nodes[i].firstChild=module.childCount+1;CHECK(verifyPair(&module,0,1,0,0));module.nodes[i]=saved;
    }
    for(size_t i=0;i<module.childCount;++i){XL6_IrNodeId saved=module.children[i];module.children[i]=UINT32_MAX;
        CHECK(verifyPair(&module,0,1,0,0));module.children[i]=saved;}
    char* after=moduleJson(&module);CHECK(after && !strcmp(before,after));xrtFree(before);xrtFree(after);
    ++modules;xl6IrModuleFree(&module);xl6ParseResultFree(&parsed);return 1;
}
static int cancelNow(void* user){(void)user;return 1;}
static int cancelledGuard(const XL6_IrModule* module)
{
    for(unsigned mode=0;mode<2;++mode){
        xls_frontend_cancel_scope scope;xls_frontend_cancel_work work;
        xlsFrontendCancelBegin(&scope,cancelNow,NULL);xlsFrontendIrScanReset();tracing=1;traceNodes=module->nodeCount;topologyCalls=0;
        XL6_DiagList diags;xl6DiagInit(&diags);int result=mode?xlsFrontendIrReferenceVerify(module,&diags):xl6IrVerifyModule(module,&diags);
        tracing=0;xlsFrontendCancelEnd(&scope,&work);CHECK(!result && !diags.count && work.cancelled && !topologyCalls && !xlsFrontendIrScanWork().indexes);
        xl6DiagFree(&diags);
    }
    return 1;
}
static int guardedStorage(void)
{
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    void* memory=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_NOACCESS);CHECK(memory);
    XL6_IrModule module={0};module.nodes=memory;module.nodeCount=module.nodeCapacity=1;module.root=1;module.stage=XL6_IR_STAGE_TYPED;
    CHECK(verifyPair(&module,0,0,0,0));++guarded;
    module.ready=1;module.stage=0;CHECK(verifyPair(&module,0,0,0,0));++guarded;
    module.stage=XL6_IR_STAGE_TYPED;module.root=0;CHECK(verifyPair(&module,0,0,0,0));++guarded;
    module.root=1;CHECK(cancelledGuard(&module));++guarded;
    CHECK(VirtualFree(memory,0,MEM_RELEASE));
#endif
    CHECK(verifyPair(NULL,0,0,0,0));XL6_IrModule empty={0};CHECK(verifyPair(&empty,0,0,0,0));return 1;
}
static int run(void)
{
    const char* sources[]={
      "func leaf(int value) as int\n int local=value+1\n return local\nend\nfunc main(...) as int\n int value=leaf(2)\n return value\nend\n",
      "func values(int count) as generator<string>\n string suffix=\"owned\"\n for i in range(count)\n string item=suffix+string(i)\n yield item\n end\nend\n",
      "func guarded() as int\n string outer=\"outer\"\n try\n string inner=\"inner\"\n return 7\n catch e\n string message=e.message\n return 8\n finally\n string final=\"final\"\n end\nend\n"
    };
    for(size_t i=0;i<sizeof(sources)/sizeof(*sources);++i)CHECK(corpus(sources[i]));
    CHECK(guardedStorage());return 1;
}
#endif
int main(void)
{
#ifdef XLS_NATIVE_IR_TOPOLOGY_OBSERVER
    if(!run())return 1;
    printf("analysis_ir_topology_test passed:%u checks;%u modules,%u comparisons,%u guarded paths;single native topology validation,full diagnostic parity,allocation rejection,all node/child corruptions,unready/stage/cancellation guards and observer scope admission\n",checks,modules,comparisons,guarded);return 0;
#else
    puts("analysis_ir_topology_test passed:observer unavailable in legacy native cohort");return 0;
#endif
}
