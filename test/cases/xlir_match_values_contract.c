#define main original_owner_main
#include "xlir_owner_consumption_contract.c"
#undef main
int main(void)
{
    XL6_ParseResult parse;XL6_IrModule ir;
    xl6ParseResultInit(&parse);xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/extern_owned_match_values.xl",&parse));
    assert(xl6StdlibExpandModule(parse.root,&parse.diags));
    assert(xl6SemCheckModule(parse.root,&parse.diags,"match_values"));
    assert(xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags));
    assert(xl6IrLowerModule(&ir,&parse.diags));assert(xl6IrVerifyModule(&ir,&parse.diags));
    size_t checked=0;
    for(size_t f=0;f<ir.functionCount;++f){
        const XL6_IrFunction* function=&ir.functions[f];
        if(!function->debugName || strcmp(function->debugName,"reads"))continue;
        for(size_t i=function->firstInstruction;i<function->firstInstruction+function->instructionCount;++i){
            const XL6_IrInstruction* instruction=&ir.instructions[i];
            assert(instruction->runtimeOp!=XL6_IR_RUNTIME_CSTRING_COPY &&
                instruction->runtimeOp!=XL6_IR_RUNTIME_CBYTES_COPY);
        }
        ++checked;
    }
    assert(checked==1);
    puts("match values: both arm orders retain owner payloads without copies");
    xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);return 0;
}
