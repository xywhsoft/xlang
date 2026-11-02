#define main original_owner_main
#include "xlir_owner_consumption_contract.c"
#undef main
int main(void)
{
    XL6_ParseResult parse;XL6_IrModule ir;
    xl6ParseResultInit(&parse);xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/extern_owned_async_callable_results.xl",&parse));
    assert(xl6StdlibExpandModule(parse.root,&parse.diags));
    assert(xl6SemCheckModule(parse.root,&parse.diags,"async_callable_results"));
    assert(xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags));
    assert(xl6IrLowerModule(&ir,&parse.diags));assert(xl6IrVerifyModule(&ir,&parse.diags));
    size_t workers=0;
    for(size_t m=0;m<ir.asyncMachineCount;++m) {
        XL6_IrAsyncMachine* machine=&ir.asyncMachines[m];
        const XL6_IrFunction* function=&ir.functions[machine->function];
        if(!function->debugName || !strstr(function->debugName,"__xl_c_owned_async_"))continue;
        // This fixture's producers have no await. Specialization must not
        // introduce a second Future, suspension, or call to the old worker.
        assert(machine->suspensionCount==0);
        for(size_t i=0;i<function->instructionCount;++i) {
            const XL6_IrInstruction* instruction=&ir.instructions[function->firstInstruction+i];
            const XL6_IrCallTarget* target=xl6IrCallTargetGet(&ir,instruction->targetSymbol);
            if(instruction->kind==XL6_IR_INSTR_CALL && target)
                assert(target->kind!=XL6_IR_CALL_TARGET_INTERNAL_ASYNC);
        }
        ++machine->suspensionCount;
        reject(&ir,"an adapted worker cannot invent a Future forwarding suspension",0);
        --machine->suspensionCount;
        assert(xl6IrVerifyModule(&ir,&parse.diags));++workers;
    }
    assert(workers>=10 && rejected==workers);
    puts("async callable results: direct worker publication, no forwarding Future, exact state plans authenticated");
    xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);return 0;
}
