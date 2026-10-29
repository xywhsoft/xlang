#define main original_owner_main
#include "xlir_owner_consumption_contract.c"
#undef main
int main(void)
{
    XL6_ParseResult parse;XL6_IrModule ir;
    xl6ParseResultInit(&parse);xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/extern_owned_bound_nested_generics.xl",&parse));
    assert(xl6StdlibExpandModule(parse.root,&parse.diags));
    assert(xl6SemCheckModule(parse.root,&parse.diags,"bound_generic_owners"));
    assert(xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags));
    assert(xl6IrLowerModule(&ir,&parse.diags));assert(xl6IrVerifyModule(&ir,&parse.diags));
    XL6_IrTypeId boolean=0;
    for(size_t t=0;t<ir.types.count;++t)
        if(ir.types.items[t].kind==XL6_IR_TYPE_BUILTIN && ir.types.items[t].name &&
           !strcmp(ir.types.items[t].name,"bool"))boolean=(XL6_IrTypeId)t+1;
    assert(boolean);size_t checked=0,reversed=0;
    for(size_t s=0;s<ir.genericSpecializationCount;++s){
        XL6_IrGenericSpecialization* instance=&ir.genericSpecializations[s];
        const XL6_IrNode* source=xl6IrNodeGet(&ir,instance->sourceDeclaration);
        const XL6_IrType* owner=source ? xl6IrTypeGet(&ir.types,source->declarationOwnerType) : NULL;
        if(!owner || owner->kind!=XL6_IR_TYPE_CLASS || owner->paramCount!=1 || instance->typeArgCount<=1)continue;
        const XL6_IrNode* name=xl6IrNodeGet(&ir,ir.children[source->firstChild]);
        if(name && name->text && !strcmp(name->text,"Reverse")){
            assert(instance->typeArgCount==3);
            assert(xl6IrTypeGet(&ir.types,instance->typeArgs[0])->cAbi==XL6_IR_C_ABI_MANAGED_STRING);
            assert(xl6IrTypeGet(&ir.types,instance->typeArgs[1])->cAbi==XL6_IR_C_ABI_MANAGED_STRING);
            assert(xl6IrTypeGet(&ir.types,instance->typeArgs[2])->cAbi==XL6_IR_C_ABI_I64);++reversed;
        }
        XL6_IrTypeId first=instance->typeArgs[0];instance->typeArgs[0]=boolean;
        reject(&ir,"method key cannot substitute another concrete owner",0);instance->typeArgs[0]=first;
        size_t count=instance->typeArgCount;instance->typeArgCount=count-1;
        reject(&ir,"method key cannot omit its method vector",0);instance->typeArgCount=count;
        assert(xl6IrVerifyModule(&ir,&parse.diags));++checked;
    }
    assert(checked>=4 && reversed && rejected>=checked*2);
    puts("nested generic methods: ordered owner/method keys authenticated");
    xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);return 0;
}
