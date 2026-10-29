/* Concrete generic tasks must retain the same closed ABI as ordinary tasks. */
#define main original_owner_main
#include "xlir_owner_consumption_contract.c"
#undef main

int main(void)
{
    XL6_ParseResult parse; XL6_IrModule ir;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/extern_owned_task_generic.xl",&parse));
    assert(xl6StdlibExpandModule(parse.root,&parse.diags));
    assert(xl6SemCheckModule(parse.root,&parse.diags,"task_composition"));
    assert(xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags));
    assert(xl6IrLowerModule(&ir,&parse.diags));
    assert(xl6IrVerifyModule(&ir,&parse.diags));
    XL6_IrIntrinsicCall* spawn=NULL;
    XL6_IrInstruction* all=NULL;
    for(size_t i=0;i<ir.intrinsicCallCount;++i) {
        XL6_IrIntrinsicCall* item=&ir.intrinsicCalls[i];
        if(item->kind==XL6_IR_INTRINSIC_TASK_SPAWN &&
           item->admission==XL6_IR_INTRINSIC_ADMISSION_VERIFIED &&
           item->payloadAbi==XL6_IR_ASYNC_RESULT_COPY_C_SCALAR) { spawn=item;break; }
    }
    for(size_t i=0;i<ir.instructionCount;++i) {
        XL6_IrInstruction* item=&ir.instructions[i];
        if(item->runtimeOp==XL6_IR_RUNTIME_FUTURE_ALL_XVALUES &&
           item->asyncResultAbi==XL6_IR_ASYNC_RESULT_COPY_C_SCALAR) { all=item;break; }
    }
    assert(spawn && all && xl6IrTaskIntrinsicCompatible(&ir,spawn));
    XL6_IrAsyncResultAbi payload=spawn->payloadAbi;
    spawn->payloadAbi=XL6_IR_ASYNC_RESULT_COPY_I64;
    assert(!xl6IrTaskIntrinsicCompatible(&ir,spawn));
    reject(&ir,"specialized native task acquired an i64 payload",0);
    spawn->payloadAbi=payload;
    XL6_IrIntrinsicOperand* argument=&ir.intrinsicOperands[spawn->firstOperand+1];
    XL6_IrIntrinsicOperandAbi abi=argument->abi;
    argument->abi=XL6_IR_INTRINSIC_OPERAND_BORROW;
    assert(!xl6IrTaskIntrinsicCompatible(&ir,spawn));
    reject(&ir,"specialized native capture acquired borrowed storage",0);
    argument->abi=abi;
    XL6_IrIntrinsicResultAbi result=spawn->resultAbi;
    spawn->resultAbi=XL6_IR_INTRINSIC_RESULT_VALUE;
    assert(!xl6IrTaskIntrinsicCompatible(&ir,spawn));
    reject(&ir,"specialized task lost Future ownership",0);
    spawn->resultAbi=result;
    XL6_IrErrorAbi error=spawn->errorAbi;
    spawn->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;
    assert(!xl6IrTaskIntrinsicCompatible(&ir,spawn));
    reject(&ir,"specialized task lost submission failure",0);
    spawn->errorAbi=error;
    payload=all->asyncResultAbi;
    all->asyncResultAbi=XL6_IR_ASYNC_RESULT_COPY_I64;
    reject(&ir,"task.all acquired an i64 boxer for native payload",0);
    all->asyncResultAbi=payload;
    XL6_IrType* type=&ir.types.items[all->payloadType-1];
    uint64_t identity=type->typeValueId;
    type->typeValueId=0;
    reject(&ir,"native task.all boxer lost type identity",0);
    type->typeValueId=identity;
    XL6_IrOwnershipAbi ownership=type->ownershipAbi;
    type->ownershipAbi=XL6_IR_OWNERSHIP_ABI_MANAGED;
    reject(&ir,"native task.all acquired managed payload destruction",0);
    type->ownershipAbi=ownership;
    XL6_IrCAbi physical=type->cAbi;
    type->cAbi=XL6_IR_C_ABI_I64;
    reject(&ir,"native task.all lost its scalar profile",0);
    type->cAbi=physical;
    assert(rejected==8 && xl6IrVerifyModule(&ir,&parse.diags));
    xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);
    xl6ParseResultInit(&parse);xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/extern_owned_task_nullable.xl",&parse));
    assert(xl6StdlibExpandModule(parse.root,&parse.diags));
    assert(xl6SemCheckModule(parse.root,&parse.diags,"task_nullable_proof"));
    assert(xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags));
    assert(xl6IrLowerModule(&ir,&parse.diags));
    assert(xl6IrVerifyModule(&ir,&parse.diags));
    XL6_IrNode* proven=NULL;
    for(size_t i=0;i<ir.intrinsicCallCount;++i) {
        XL6_IrIntrinsicCall* item=&ir.intrinsicCalls[i];
        if(item->kind!=XL6_IR_INTRINSIC_TASK_SPAWN || !item->operandCount)continue;
        XL6_IrNode* source=&ir.nodes[ir.intrinsicOperands[item->firstOperand].expression-1];
        if(source->type!=source->storageType) { proven=source;break; }
    }
    assert(proven);
    XL6_IrTypeId proof=proven->type;
    proven->type=proven->storageType;
    reject(&ir,"task nullable callable lost its materialized non-null proof",0);
    proven->type=proof;
    assert(rejected==9 && xl6IrVerifyModule(&ir,&parse.diags));
    xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);
    puts("task composition ABI: 9 mutations rejected before C/header emission");
    return 0;
}
