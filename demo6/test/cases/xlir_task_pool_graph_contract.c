#include "xlir_continuation_owner_test.h"
int main(void)
{
    const char* sources[]={"int value = 1\n",
        "func work() as int\nreturn 7\nend\nfunc() as int job = work\nauto result = task.spawn(job)\n",
        "extern \"observe\" func observe()\nasync func work()\nobserve()\nend\nauto result = work()\n"};
    for(unsigned n=0;n<4;++n){
        XL6_ParseResult parse;XL6_IrModule ir;XL6_Output out;char* text;XL6_IrCgenOptions options={1,0};
        xl6ParseResultInit(&parse);xl6IrModuleInit(&ir);
        assert(n==3?xl6ParseFile("test/cases/xlir_module_executor_mixed.xl",&parse):xl6ParseSource(sources[n],"pool_graph.xl",&parse));
        assert(xl6StdlibExpandModule(parse.root,&parse.diags)&&xl6SemCheckModule(parse.root,&parse.diags,"pool_graph")&&
            xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags));
        assert(ir.abi.taskPoolOwnershipAbi==XL6_IR_MODULE_TASK_POOL_OWNERSHIP_NONE);
        ir.abi.taskPoolOwnershipAbi=XL6_IR_MODULE_TASK_POOL_OWNERSHIP_RESIDENT_NATIVE_JOBS_JOIN_V1;
        reject(&ir,"module task pool ownership ABI");
        ir.abi.taskPoolOwnershipAbi=(XL6_IrModuleTaskPoolOwnershipAbi)99;reject(&ir,"module task pool ownership ABI");
        ir.abi.taskPoolOwnershipAbi=XL6_IR_MODULE_TASK_POOL_OWNERSHIP_NONE;
        assert(xl6IrLowerModule(&ir,&parse.diags));verify(&ir);
        XL6_IrModuleTaskPoolOwnershipAbi owned=ir.abi.taskPoolOwnershipAbi;
        XL6_IrModuleTaskPoolAbi execution=ir.abi.taskPoolAbi;
        XL6_IrModuleRetirementAbi retirement=ir.abi.retirementAbi;
        assert(owned==(n?XL6_IR_MODULE_TASK_POOL_OWNERSHIP_RESIDENT_NATIVE_JOBS_JOIN_V1:XL6_IR_MODULE_TASK_POOL_OWNERSHIP_NONE));
        assert(retirement==(n?XL6_IR_MODULE_RETIREMENT_OWNED_SERVICES_GRAPH_V3:XL6_IR_MODULE_RETIREMENT_PREPARED_GRAPH_V2));
        ir.abi.taskPoolOwnershipAbi=n?XL6_IR_MODULE_TASK_POOL_OWNERSHIP_NONE:XL6_IR_MODULE_TASK_POOL_OWNERSHIP_RESIDENT_NATIVE_JOBS_JOIN_V1;
        reject(&ir,"module task pool ownership ABI");
        ir.abi.taskPoolAbi=n?XL6_IR_MODULE_TASK_POOL_NONE:XL6_IR_MODULE_TASK_POOL_XRT_GUARDED_QUIESCE;
        ir.abi.retirementAbi=n?XL6_IR_MODULE_RETIREMENT_PREPARED_GRAPH_V2:XL6_IR_MODULE_RETIREMENT_OWNED_SERVICES_GRAPH_V3;
        /* A forged resource field can fail finalizer verification first. */
        reject(&ir,NULL);
        ir.abi.taskPoolAbi=execution;ir.abi.retirementAbi=retirement;
        ir.abi.taskPoolOwnershipAbi=(XL6_IrModuleTaskPoolOwnershipAbi)99;reject(&ir,"module task pool ownership ABI");
        ir.abi.taskPoolOwnershipAbi=owned;
        ir.abi.retirementAbi=n?XL6_IR_MODULE_RETIREMENT_PREPARED_GRAPH_V2:XL6_IR_MODULE_RETIREMENT_OWNED_SERVICES_GRAPH_V3;
        reject(&ir,"module retirement ABI");ir.abi.retirementAbi=retirement;verify(&ir);
        assert(xl6OutputOpenMemory(&out)&&xl6IrCgenEmitCEx(&out,&ir,&parse.diags,&options));
        text=xl6OutputTakeMemoryText(&out,NULL);assert(text);
        assert(!!strstr(text,"visit(xl6TaskPoolOwnership(xlir_async_pool), context)")==!!n);
        assert(!strstr(text,"xl6TaskPoolTryQuiesceV1(xlir_async_pool)")&&
            !strstr(text,"if (!teardown && !xl6ContinuationExecutorIsIdle")&&
            !strstr(text,"if (!xl6TaskRuntimeV1Get()->PoolIsClosing"));
        if(n)assert(strstr(text,"xl_script_module_retirement_v3(void)")&&
            strstr(text,"if (teardown && !xlir_async_pool_quiesce())"));
        xrtFree(text);
        assert(xl6OutputOpenMemory(&out));xl6IrDumpModuleText(&out,&ir);text=xl6OutputTakeMemoryText(&out,NULL);
        assert(text&&strstr(text,n?"module-task-pool-ownership: 1":"module-task-pool-ownership: 0"));xrtFree(text);
        assert(xl6OutputOpenMemory(&out));xl6IrDumpModuleJson(&out,&ir);text=xl6OutputTakeMemoryText(&out,NULL);
        assert(text&&strstr(text,n?"\"moduleTaskPoolOwnershipAbi\": 1":"\"moduleTaskPoolOwnershipAbi\": 0"));xrtFree(text);
        xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);
    }
    assert(mutations==24);
    puts("Task pool graph IR: 4 partitions, 24 C/header-gated mutations, independent operation-derived ownership, V3 trace/preflight and exact text/JSON");
    return 0;
}
