#define main original_owner_main
#include "xlir_owner_consumption_contract.c"
#undef main
static void checkGenericResultModule(const char* path,size_t minimum,int scalar)
{
    XL6_ParseResult parse;XL6_IrModule ir;
    xl6ParseResultInit(&parse);xl6IrModuleInit(&ir);
    assert(xl6ParseFile(path,&parse));
    assert(xl6StdlibExpandModule(parse.root,&parse.diags));
    assert(xl6SemCheckModule(parse.root,&parse.diags,"generic_callable_results"));
    assert(xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags));
    assert(xl6IrLowerModule(&ir,&parse.diags));assert(xl6IrVerifyModule(&ir,&parse.diags));
    XL6_IrTypeId boolean=0;size_t checked=0;
    for(size_t t=0;t<ir.types.count;++t)
        if(ir.types.items[t].cAbi==XL6_IR_C_ABI_BOOL)boolean=(XL6_IrTypeId)t+1;
    assert(boolean);
    for(size_t i=0;i<ir.instructionCount;++i)
        assert(ir.instructions[i].runtimeOp!=XL6_IR_RUNTIME_XVALUE_CLOSURE_CAST_CHECKED);
    for(size_t c=0;c<ir.closureTargetCount;++c) {
        XL6_IrClosureTarget* closure=&ir.closureTargets[c];
        if(closure->kind!=XL6_IR_CLOSURE_TARGET_BOUND_METHOD_ADAPTER)continue;
        XL6_IrCallTarget* target=NULL;
        for(size_t t=0;t<ir.callTargetCount;++t)
            if(ir.callTargets[t].symbol==closure->adaptedTargetSymbol)target=&ir.callTargets[t];
        if(!target || target->parameterCount<2)continue;
        const XL6_IrType* input=xl6IrTypeGet(&ir.types,target->parameterTypes[1]);
        if(scalar ? input->cAbi!=XL6_IR_C_ABI_C_INT : input->kind!=XL6_IR_TYPE_FUNCTION)continue;
        int generic=0;
        for(size_t s=0;s<ir.genericSpecializationCount;++s)
            generic|=ir.genericSpecializations[s].symbol==target->symbol;
        if(!generic)continue;
        XL6_IrTypeId saved=target->parameterTypes[1];target->parameterTypes[1]=boolean;
        reject(&ir,"closed generic factory cannot change its inferred C input ABI",0);
        target->parameterTypes[1]=saved;
        XL6_SymbolId selected=closure->adaptedTargetSymbol;closure->adaptedTargetSymbol=0;
        reject(&ir,"generic result conversion must authenticate its selected method",0);
        closure->adaptedTargetSymbol=selected;
        assert(xl6IrVerifyModule(&ir,&parse.diags));++checked;
    }
    assert(checked>=minimum && rejected==checked*2);
    xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);
}
int main(void)
{
    checkGenericResultModule("test/cases/extern_owned_generic_callable_results.xl",7,1);
    rejected=0;
    checkGenericResultModule("test/cases/extern_owned_inferred_result_variables.xl",4,0);
    puts("generic callable results: closed input ABI and selected generic target authenticated");
    return 0;
}
