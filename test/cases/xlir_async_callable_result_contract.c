#define main original_owner_main
#include "xlir_owner_consumption_contract.c"
#undef main
static void checkAsyncFixture(const char* path,size_t minimum)
{
    XL6_ParseResult parse;XL6_IrModule ir;
    xl6ParseResultInit(&parse);xl6IrModuleInit(&ir);
    assert(xl6ParseFile(path,&parse));
    assert(xl6StdlibExpandModule(parse.root,&parse.diags));
    assert(xl6SemCheckModule(parse.root,&parse.diags,"async_callable_results"));
    assert(xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags));
    assert(xl6IrLowerModule(&ir,&parse.diags));assert(xl6IrVerifyModule(&ir,&parse.diags));
    // Global leaf async functions can use the existing ready-Future fast path,
    // with no state machine. Inspect those bodies as well as machine workers.
    for(size_t f=0;f<ir.functionCount;++f) {
        const XL6_IrFunction* function=&ir.functions[f];
        const XL6_IrNode* declaration=xl6IrNodeGet(&ir,function->declaration);
        if(!declaration || declaration->sourceKind!=XL6_AST_ASYNC_FUNC ||
           !(declaration->flags & XL6_AST_FLAG_C_OWNED_SPECIALIZATION))continue;
        for(size_t i=0;i<function->instructionCount;++i) {
            const XL6_IrInstruction* instruction=&ir.instructions[function->firstInstruction+i];
            const XL6_IrCallTarget* target=xl6IrCallTargetGet(&ir,instruction->targetSymbol);
            if(instruction->kind==XL6_IR_INSTR_CALL && target)
                assert(target->kind!=XL6_IR_CALL_TARGET_INTERNAL_ASYNC);
        }
    }
    size_t workers=0,before=rejected;
    for(size_t m=0;m<ir.asyncMachineCount;++m) {
        XL6_IrAsyncMachine* machine=&ir.asyncMachines[m];
        const XL6_IrFunction* function=&ir.functions[machine->function];
        const XL6_IrNode* declaration=xl6IrNodeGet(&ir,function->declaration);
        if(!declaration || !(declaration->flags & XL6_AST_FLAG_C_OWNED_SPECIALIZATION))continue;
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
    fprintf(stderr,"%s: %zu private workers, %zu rejected mutations\n",path,workers,rejected-before);
    assert(workers>=minimum && rejected-before==workers);
    xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);
}

int main(void)
{
    checkAsyncFixture("test/cases/extern_owned_async_callable_results.xl",10);
    checkAsyncFixture("test/cases/extern_owned_async_open_callers.xl",5);
    checkAsyncFixture("test/cases/extern_owned_async_named_results.xl",1);
    checkAsyncFixture("test/cases/extern_owned_async_deep_dispatch.xl",41);
    puts("async callable results: direct worker publication, no forwarding Future, generic and named workers, deep dispatch, exact state plans authenticated");
    return 0;
}
