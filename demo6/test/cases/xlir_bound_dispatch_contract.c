#define main original_owner_main
#include "xlir_owner_consumption_contract.c"
#undef main
int main(void)
{
    XL6_ParseResult parse; XL6_IrModule ir;
    xl6ParseResultInit(&parse);xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/extern_owned_bound_base_dispatch.xl",&parse));
    assert(xl6StdlibExpandModule(parse.root,&parse.diags));
    assert(xl6SemCheckModule(parse.root,&parse.diags,"bound_dispatch"));
    assert(xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags));
    assert(xl6IrLowerModule(&ir,&parse.diags));assert(xl6IrVerifyModule(&ir,&parse.diags));
    size_t direct=0,virtuals=0;
    for(size_t c=0;c<ir.closureTargetCount;++c){
        XL6_IrClosureTarget* closure=&ir.closureTargets[c];
        if(closure->kind!=XL6_IR_CLOSURE_TARGET_BOUND_METHOD_ADAPTER)continue;
        const XL6_IrFunction* function=NULL;
        for(size_t f=0;f<ir.functionCount;++f)
            if(ir.functions[f].symbol==closure->targetSymbol)function=&ir.functions[f];
        assert(function);
        uint32 slot=closure->boundMethodDispatchSlot;
        closure->boundMethodDispatchSlot=slot?0:1;
        reject(&ir,"bound dispatch plan no longer matches source receiver",0);
        closure->boundMethodDispatchSlot=slot;
        for(size_t i=function->firstInstruction;i<function->firstInstruction+function->instructionCount;++i){
            XL6_IrInstruction* call=&ir.instructions[i];
            if(call->origin!=closure->declaration ||
               (call->kind!=XL6_IR_INSTR_CALL && call->kind!=XL6_IR_INSTR_VIRTUAL_CALL))continue;
            if(call->kind==XL6_IR_INSTR_CALL && call->targetSymbol!=closure->adaptedTargetSymbol)continue;
            XL6_IrInstruction original=*call;
            if(slot){++virtuals;call->kind=XL6_IR_INSTR_CALL;call->dispatchSlot=0;call->targetSymbol=closure->adaptedTargetSymbol;}
            else{++direct;call->kind=XL6_IR_INSTR_VIRTUAL_CALL;call->dispatchSlot=1;call->targetSymbol=0;}
            reject(&ir,"adapter call no longer follows the authenticated dispatch plan",0);
            *call=original;
        }
        assert(xl6IrVerifyModule(&ir,&parse.diags));
    }
    assert(direct && virtuals && rejected>=4);
    printf("bound dispatch: %llu static, %llu virtual, %llu rejected mutations\n",
        (unsigned long long)direct,(unsigned long long)virtuals,(unsigned long long)rejected);
    xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);return 0;
}
