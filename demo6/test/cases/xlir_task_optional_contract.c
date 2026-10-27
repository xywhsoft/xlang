/* Invalid task contracts must be rejected before either C or header output. */
#define main original_owner_main
#include "xlir_owner_consumption_contract.c"
#undef main

static void loadTaskFixture(XL6_ParseResult* parse, XL6_IrModule* ir, const char* path)
{
    xl6ParseResultInit(parse); xl6IrModuleInit(ir);
    assert(xl6ParseFile(path,parse));
    assert(xl6StdlibExpandModule(parse->root,&parse->diags));
    assert(xl6SemCheckModule(parse->root,&parse->diags,"task_optional_contract"));
    assert(xl6IrBuildModuleFromAst(ir,parse->root,&parse->diags));
    assert(xl6IrLowerModule(ir,&parse->diags));
    assert(xl6IrVerifyModule(ir,&parse->diags));
}

int main(void)
{
    XL6_ParseResult parse; XL6_IrModule ir;
    loadTaskFixture(&parse,&ir,"test/cases/extern_owned_task_optional_scalars.xl");
    XL6_IrInstruction* all=NULL; XL6_IrIntrinsicCall* spawn=NULL;
    for(size_t i=0;i<ir.instructionCount;++i)
        if(ir.instructions[i].runtimeOp==XL6_IR_RUNTIME_FUTURE_ALL_XVALUES &&
           ir.instructions[i].asyncResultAbi==XL6_IR_ASYNC_RESULT_COPY_TAGGED) { all=&ir.instructions[i];break; }
    for(size_t i=0;i<ir.intrinsicCallCount;++i)
        if(ir.intrinsicCalls[i].kind==XL6_IR_INTRINSIC_TASK_SPAWN &&
           ir.intrinsicCalls[i].payloadAbi==XL6_IR_ASYNC_RESULT_COPY_TAGGED) { spawn=&ir.intrinsicCalls[i];break; }
    assert(all && spawn);
    XL6_IrAsyncResultAbi abi=all->asyncResultAbi;
    all->asyncResultAbi=XL6_IR_ASYNC_RESULT_COPY_C_SCALAR;
    reject(&ir,"optional aggregate lost its presence carrier",0);
    all->asyncResultAbi=abi;
    XL6_IrTypeId payload=all->payloadType;
    all->payloadType=ir.types.items[payload-1].params[0];
    reject(&ir,"optional aggregate acquired its non-null element type",0);
    all->payloadType=payload;
    XL6_IrNullAbi nullAbi=ir.types.items[payload-1].nullAbi;
    ir.types.items[payload-1].nullAbi=XL6_IR_NULL_ABI_NONE;
    reject(&ir,"optional native payload lost its null ABI",0);
    ir.types.items[payload-1].nullAbi=nullAbi;
    XL6_IrIntrinsicOperand* argument=&ir.intrinsicOperands[spawn->firstOperand+1];
    XL6_IrTypeId input=argument->type;
    argument->type=ir.types.items[input-1].params[0];
    reject(&ir,"nullable capture lost its source representation",0);
    argument->type=input;
    assert(xl6IrVerifyModule(&ir,&parse.diags));
    xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);

    loadTaskFixture(&parse,&ir,"test/cases/extern_owned_task_continuations.xl");
    XL6_IrIntrinsicCall* then=NULL;
    for(size_t i=0;i<ir.intrinsicCallCount;++i)
        if(ir.intrinsicCalls[i].kind==XL6_IR_INTRINSIC_FUTURE_THEN &&
           ir.intrinsicCalls[i].sourcePayloadType!=ir.intrinsicCalls[i].payloadType &&
           ir.intrinsicCalls[i].admission==XL6_IR_INTRINSIC_ADMISSION_VERIFIED) { then=&ir.intrinsicCalls[i];break; }
    assert(then);
    payload=then->sourcePayloadType;then->sourcePayloadType=then->payloadType;
    reject(&ir,"generic continuation acquired its output as its source",0);
    then->sourcePayloadType=payload;
    abi=then->sourcePayloadAbi;then->sourcePayloadAbi=XL6_IR_ASYNC_RESULT_OWNED_STRING;
    reject(&ir,"generic continuation lost the source scalar ABI",0);
    then->sourcePayloadAbi=abi;
    abi=then->payloadAbi;then->payloadAbi=XL6_IR_ASYNC_RESULT_COPY_C_SCALAR;
    reject(&ir,"generic continuation lost the result string owner",0);
    then->payloadAbi=abi;
    XL6_IrErrorAbi failure=then->errorAbi;then->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;
    reject(&ir,"generic continuation lost its checked submission",0);
    then->errorAbi=failure;
    assert(xl6IrVerifyModule(&ir,&parse.diags));
    xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);

    loadTaskFixture(&parse,&ir,"test/cases/extern_owned_task_generic_callable.xl");
    XL6_IrNode* reference=NULL;
    for(size_t i=0;i<ir.nodeCount;++i)
        if(ir.nodes[i].sourceKind==XL6_AST_GENERIC_INSTANT &&
           ir.nodes[i].namedCallableAbi==XL6_IR_NAMED_CALLABLE_CAPTURE_FREE_ADAPTER) { reference=&ir.nodes[i];break; }
    assert(reference);
    XL6_SymbolId target=reference->namedCallableTargetSymbol;
    reference->namedCallableTargetSymbol=0;
    reject(&ir,"generic reference lost its concrete target",0);
    reference->namedCallableTargetSymbol=target;
    payload=reference->namedCallableSignatureType;
    reference->namedCallableSignatureType=XL6_IR_TYPE_INVALID;
    reject(&ir,"generic reference lost its closed signature",0);
    reference->namedCallableSignatureType=payload;
    assert(rejected==10 && xl6IrVerifyModule(&ir,&parse.diags));
    xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);
    puts("optional task contracts: 10 mutations rejected before C/header emission");
    return 0;
}
