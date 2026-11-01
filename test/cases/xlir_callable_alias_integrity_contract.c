#define main original_owner_main
#include "xlir_owner_consumption_contract.c"
#undef main
static uint64_t shape(const XL6_AstNode* node)
{
    uint64_t hash=(uint64_t)node->kind+node->childCount;
    for(const char* text=node->text;text && *text;++text)hash=(hash^(unsigned char)*text)*UINT64_C(1099511628211);
    for(size_t i=0;i<node->childCount;++i)hash=(hash^shape(node->children[i]))*UINT64_C(1099511628211);
    return hash;
}
static void check_aliases(const char* path,int accepted)
{
    XL6_ParseResult parse;XL6_IrModule ir;
    xl6ParseResultInit(&parse);xl6IrModuleInit(&ir);
    assert(xl6ParseFile(path,&parse));assert(xl6StdlibExpandModule(parse.root,&parse.diags));
    uint64_t hashes[32];XL6_AstNode* aliases[32];size_t count=0;
    for(size_t i=0;i<parse.root->childCount;++i) {
        XL6_AstNode* node=parse.root->children[i];
        if(node->kind!=XL6_AST_TYPE_ALIAS)continue;
        assert(count<32);aliases[count]=node;hashes[count]=shape(node);++count;
    }
    assert(count>0);
    assert(xl6SemCheckModule(parse.root,&parse.diags,"alias_integrity")==accepted);
    for(size_t i=0;i<count;++i)assert(shape(aliases[i])==hashes[i]);
    if(accepted) {
        assert(count==9);assert(xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags));
        assert(xl6IrLowerModule(&ir,&parse.diags));assert(xl6IrVerifyModule(&ir,&parse.diags));
        size_t exact=0;
        for(size_t c=0;c<ir.closureTargetCount;++c) {
            const XL6_IrClosureTarget* closure=&ir.closureTargets[c];
            if(closure->kind!=XL6_IR_CLOSURE_TARGET_BOUND_METHOD_ADAPTER)continue;
            const XL6_IrCallTarget* target=xl6IrCallTargetGet(&ir,closure->adaptedTargetSymbol);
            const XL6_IrType* signature=xl6IrTypeGet(&ir.types,closure->signatureType);
            if(!target || !signature || target->returnType!=signature->callableResultType ||
               !xl6IrTypeIsCOwned(xl6IrTypeGet(&ir.types,target->returnType)))continue;
            const XL6_IrFunction* body=NULL;
            for(size_t f=0;f<ir.functionCount;++f)if(ir.functions[f].symbol==closure->targetSymbol)body=&ir.functions[f];
            assert(body);
            for(size_t i=body->firstInstruction;i<body->firstInstruction+body->instructionCount;++i)
                assert(ir.instructions[i].runtimeOp!=XL6_IR_RUNTIME_CSTRING_COPY &&
                       ir.instructions[i].runtimeOp!=XL6_IR_RUNTIME_CBYTES_COPY);
            ++exact;
        }
        assert(exact>=5);
    }
    xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);
}
int main(void)
{
    check_aliases("test/cases/extern_owned_callable_alias_integrity.xl",1);
    check_aliases("test/cases/bad_extern_owned_alias_channel.xl",0);
    puts("callable aliases: immutable on success and rejection, exact owner reads contain no copy");return 0;
}
