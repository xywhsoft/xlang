/* Actual full verifier/emission gates for live and unused async declarations.
 * Reuse the checked-output test helper, not a second compiler/verifier. */
#define main tcpContractRegressionMain
#include "xlir_tcp_managed_contract.c"
#undef main
static size_t machineChecks,factChecks;
static void deployment(const char* path,bool live)
{
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,path,1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));
    XL6_IrModuleContinuationAbi abi=module->abi.continuationAbi;
    assert(abi==(live?XL6_IR_MODULE_CONTINUATION_FIXED_WORKERS_V1:XL6_IR_MODULE_CONTINUATION_NONE));
    module->abi.continuationAbi=live?XL6_IR_MODULE_CONTINUATION_NONE:XL6_IR_MODULE_CONTINUATION_FIXED_WORKERS_V1;
    refused(module,NULL,0);module->abi.continuationAbi=abi;
    assert(module->asyncMachineCount>0);
    for(size_t i=0;i<module->asyncMachineCount;++i){
        XL6_IrAsyncExecutionAbi saved=module->asyncMachines[i].executionAbi;
        module->asyncMachines[i].executionAbi=0;
        refused(module,NULL,1);module->asyncMachines[i].executionAbi=saved;++machineChecks;
    }
    if(live){
        XL6_IrReachability* functions=xrtMalloc(module->functionCount*sizeof(*functions));
        XL6_IrReachability* targets=xrtMalloc(module->callTargetCount*sizeof(*targets));assert(functions&&targets);
        for(size_t i=0;i<module->functionCount;++i){functions[i]=module->functions[i].reachability;
            module->functions[i].reachability=XL6_IR_REACHABILITY_UNREACHABLE;}
        for(size_t i=0;i<module->callTargetCount;++i){targets[i]=module->callTargets[i].reachability;
            module->callTargets[i].reachability=XL6_IR_REACHABILITY_UNREACHABLE;}
        module->abi.continuationAbi=XL6_IR_MODULE_CONTINUATION_NONE;
        assert(xl6IrModuleContinuationRequirement(module)==XL6_IR_MODULE_CONTINUATION_NONE);
        /* Classification is NOT permission: recomputed closed reachability
         * must reject the forged deployment facts before any C is emitted. */
        refused(module,NULL,2);++factChecks;
        for(size_t i=0;i<module->functionCount;++i)module->functions[i].reachability=functions[i];
        for(size_t i=0;i<module->callTargetCount;++i)module->callTargets[i].reachability=targets[i];
        module->abi.continuationAbi=abi;xrtFree(functions);xrtFree(targets);
    }
    assert(xl6IrVerifyModule(module,NULL));xl6CompileUnitFree(&unit);
}
int main(int argc,char** argv)
{
    assert(argc==3);deployment(argv[1],false);deployment(argv[2],true);
    assert(machineChecks>=2&&factChecks==1&&emissionGuards==3);
    printf("continuation deployment: %zu machine contracts, %zu forged facts; all 3 violation kinds refused before C/header\n",machineChecks,factChecks);
    return 0;
}
