#define main original_owner_main
#include "xlir_owner_consumption_contract.c"
#undef main
int main(void)
{
    XL6_ParseResult parse;XL6_IrModule ir;
    xl6ParseResultInit(&parse);xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/extern_owned_error_safe_index.xl",&parse));
    assert(xl6StdlibExpandModule(parse.root,&parse.diags));
    assert(xl6SemCheckModule(parse.root,&parse.diags,"error_projection_contract"));
    assert(xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags));
    assert(xl6IrLowerModule(&ir,&parse.diags));assert(xl6IrVerifyModule(&ir,&parse.diags));
    XL6_IrInstruction* projection=NULL;
    for (size_t i=0;i<ir.instructionCount;++i)
        if (ir.instructions[i].runtimeOp==XL6_IR_RUNTIME_XVALUE_PROJECT_ERROR_REF &&
            ir.instructions[i].runtimeIdentityType!=ir.instructions[i].type) {projection=&ir.instructions[i];break;}
    assert(projection);
    XL6_IrTypeId identity=projection->runtimeIdentityType,payload=projection->payloadType;
    projection->runtimeIdentityType=projection->type;
    reject(&ir,"safe Error index confused result identity with source box",0);
    projection->runtimeIdentityType=identity;
    projection->payloadType=identity;
    reject(&ir,"safe Error index lost its nullable result payload",0);
    projection->payloadType=payload;
    projection->runtimeIdentityType=XL6_IR_TYPE_INVALID;
    reject(&ir,"safe Error index lost its source identity",0);
    projection->runtimeIdentityType=identity;
    assert(rejected==3 && xl6IrVerifyModule(&ir,&parse.diags));
    xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);
    puts("error projection contracts: 3 mutations rejected before C/header emission");return 0;
}
