#include "analysis/frontend_ir.h"
#include <stdio.h>
#include <string.h>
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
static unsigned checks,denyReserve,reserveFailures;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_ir_callsite_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
bool __real_xrtArrayReserve(xarray*,size_t);
bool __wrap_xrtArrayReserve(xarray* array,size_t count)
{if(denyReserve){denyReserve=0;++reserveFailures;return false;}return __real_xrtArrayReserve(array,count);}
static int phase(int members,unsigned mode)
{
    enum {NODES=4096,SITES=2048};
    XL6_IrNode* nodes=xrtCalloc(NODES,sizeof(*nodes));
    XL6_IrNode* original=xrtCalloc(NODES,sizeof(*original));
    XL6_IrNode* reference=xrtCalloc(NODES,sizeof(*reference));
    XL6_IrCallSite* sites=xrtCalloc(SITES,sizeof(*sites));
    CHECK(nodes && original && reference && sites);
    XL6_IrType types[2]={{.cAbi=XL6_IR_C_ABI_MANAGED_STRING},{.cAbi=XL6_IR_C_ABI_I64,.ownershipAbi=XL6_IR_OWNERSHIP_ABI_TRIVIAL}};
    XL6_IrCallArgument argument={.expression=1};
    for(size_t i=0;i<NODES;++i){nodes[i].kind=XL6_IR_NODE_LITERAL;nodes[i].sourceKind=XL6_AST_INT;nodes[i].storageType=i ? 2 : 1;}
    for(size_t i=0;i<SITES;++i)sites[i]=(XL6_IrCallSite){.origin=(XL6_IrNodeId)(1+(i*37)%NODES),
        .variadicAbi=i%7==0 ? XL6_IR_CALL_VARIADIC_C : XL6_IR_CALL_VARIADIC_NONE,.argumentCount=1};
    sites[0].variadicAbi=mode==4 ? XL6_IR_CALL_VARIADIC_C : XL6_IR_CALL_VARIADIC_NONE;
    sites[2].origin=0;sites[SITES-1].origin=sites[0].origin;sites[SITES-1].variadicAbi=XL6_IR_CALL_VARIADIC_C;
    XL6_IrModule module={.nodes=nodes,.nodeCount=NODES,.nodeCapacity=NODES,.types={.items=types,.count=2,.capacity=2},
        .callSites=sites,.callSiteCount=SITES,.callSiteCapacity=SITES,.callArguments=&argument,.callArgumentCount=1,.callArgumentCapacity=1};
    if(mode==1)module.callArguments=NULL;
    if(mode==2)module.callArgumentCapacity=0;
    if(mode==3)--module.callSiteCapacity;
    CHECK(!xl6IrCallSiteGet(NULL,1) && !xl6IrCallSiteGet(&module,0));
    CHECK(xl6IrCallSiteGet(&module,sites[0].origin)==sites);
    memcpy(original,nodes,NODES*sizeof(*nodes));
    /* An actual failed optional index runs the original accessor throughout
     * the same production phase. Compare every resulting node annotation. */
    xlsFrontendIrCallScanReset();denyReserve=1;reserveFailures=0;
    CHECK(members ? xl6IrRefreshMemberAbis(&module) : xl6IrRefreshLoopAbis(&module));
    denyReserve=0;xls_ir_call_scan_work slow=xlsFrontendIrCallScanWork();
    CHECK(!slow.indexes && !slow.indexedVisits && reserveFailures==(mode==3 ? 0u : 1u));
    memcpy(reference,nodes,NODES*sizeof(*nodes));memcpy(nodes,original,NODES*sizeof(*nodes));
    xlsFrontendIrCallScanReset();
    CHECK(members ? xl6IrRefreshMemberAbis(&module) : xl6IrRefreshLoopAbis(&module));
    xls_ir_call_scan_work fast=xlsFrontendIrCallScanWork();
    CHECK(!memcmp(nodes,reference,NODES*sizeof(*nodes)));
    if(mode==3)CHECK(!fast.indexes && !fast.indexedVisits);
    else CHECK(fast.indexes==1 && fast.records==SITES && fast.queries>=NODES && fast.indexedVisits && !fast.linearVisits && fast.peakBytes<=8u*1024u*1024u);
    CHECK(nodes[0].constructionErrorAbi==((mode==4) ? XL6_IR_ERROR_ABI_MAY_RAISE : XL6_IR_ERROR_ABI_INVALID));
    if(mode==1 || mode==2 || mode==3)for(size_t i=0;i<NODES;++i)CHECK(nodes[i].constructionErrorAbi==XL6_IR_ERROR_ABI_INVALID);
    /* Scope teardown must restore ordinary first-match access. */
    xlsFrontendIrCallScanReset();CHECK(xl6IrCallSiteGet(&module,1)==sites);CHECK(!xlsFrontendIrCallScanWork().indexes);
    xrtFree(sites);xrtFree(reference);xrtFree(original);xrtFree(nodes);return 1;
}
int main(void)
{
    for(unsigned mode=0;mode<5;++mode)if(!phase(0,mode) || !phase(1,mode))return 1;
    printf("analysis_ir_callsite_test passed: %u checks; original phase/annotation parity,unsorted duplicate-first origins,live variadic metadata,invalid arenas and actual index reserve failure\n",checks);return 0;
}
#else
int main(void){puts("analysis_ir_callsite_test passed: legacy cohort retains its original accessor");return 0;}
#endif
