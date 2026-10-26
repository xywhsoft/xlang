/* New scalar admission must not weaken exact parameter/capture contracts. */
#define main original_owner_main
#include "xlir_owner_consumption_contract.c"
#undef main

int main(void)
{
    XL6_ParseResult parse; XL6_IrModule ir;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/extern_owned_task_scalars.xl", &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "task_c_scalars"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    assert(xl6IrLowerModule(&ir, &parse.diags));
    assert(xl6IrVerifyModule(&ir, &parse.diags));
    XL6_IrInstruction* task = NULL;
    for (size_t i=0; i<ir.instructionCount; ++i) {
        XL6_IrInstruction* item=&ir.instructions[i];
        const XL6_IrType* type=xl6IrTypeGet(&ir.types,item->calleeType);
        if (item->runtimeOp == XL6_IR_RUNTIME_TASK_SPAWN_CLOSURE && type &&
            type->paramCount == 1 &&
            xl6IrTypeGet(&ir.types,type->params[0])->cAbi == XL6_IR_C_ABI_C_INT) {
            task=item; break;
        }
    }
    assert(task && task->callbackBoundary && task->callbackBoundary<=ir.callbackBoundaryCount);
    XL6_IrType* signature=&ir.types.items[task->calleeType-1];
    assert(signature==xl6IrTypeGet(&ir.types,task->calleeType));
    XL6_IrCallbackBoundary* boundary=&ir.callbackBoundaries[task->callbackBoundary-1];
    assert(boundary->captureCount==2);
    XL6_IrCallbackCapture* capture=&ir.callbackCaptures[boundary->firstCapture+1];
    XL6_IrParameterAbi abi=signature->parameterAbis[0];
    signature->parameterAbis[0]=XL6_IR_PARAMETER_ABI_BORROW;
    reject(&ir,"C scalar parameter acquired a borrowed pointer ABI",0);
    signature->parameterAbis[0]=abi;
    XL6_IrParameterMode mode=signature->parameterModes[0];
    signature->parameterModes[0]=XL6_IR_PARAMETER_MODE_BYREF;
    signature->parameterAbis[0]=XL6_IR_PARAMETER_ABI_INOUT;
    reject(&ir,"task acquired caller byref scalar storage",0);
    signature->parameterModes[0]=XL6_IR_PARAMETER_MODE_OUT;
    signature->parameterAbis[0]=XL6_IR_PARAMETER_ABI_OUT;
    reject(&ir,"task acquired caller out scalar storage",0);
    signature->parameterModes[0]=mode;
    signature->parameterAbis[0]=abi;
    XL6_IrTypeId type=capture->type;
    capture->type=task->payloadType;
    reject(&ir,"C scalar frame field changed to its string result type",0);
    capture->type=type;
    XL6_IrAsyncFrameTransferAbi transfer=capture->transferAbi;
    capture->transferAbi=XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_XVALUE;
    reject(&ir,"C scalar frame acquisition became boxed retention",0);
    capture->transferAbi=transfer;
    XL6_IrAsyncFrameDestroyAbi destroy=capture->destroyAbi;
    capture->destroyAbi=XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_XVALUE;
    reject(&ir,"C scalar frame acquired a pointer destructor",0);
    capture->destroyAbi=destroy;
    assert(rejected==6 && xl6IrVerifyModule(&ir,&parse.diags));
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    puts("task C scalar ABI: 6 mutations rejected before C/header emission");
    return 0;
}
