#include "xlir_continuation_owner_test.h"
static bool metadata(XL6_IrIntrinsicKind kind)
{
    return kind==XL6_IR_INTRINSIC_FUTURE_SET_DEBUG_NAME || kind==XL6_IR_INTRINSIC_FUTURE_DEBUG_NAME ||
        kind==XL6_IR_INTRINSIC_FUTURE_CREATE_TIME_MS || kind==XL6_IR_INTRINSIC_FUTURE_COMPLETE_TIME_MS ||
        kind==XL6_IR_INTRINSIC_FUTURE_PENDING_CONTINUATIONS;
}
int main(void)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; char* text;
    size_t typed=0,lowered=0;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if(!(xl6ParseFile("test/cases/stdlib_future_metadata_only.xl",&parse) &&
        xl6StdlibExpandModule(parse.root,&parse.diags) &&
        xl6SemCheckModule(parse.root,&parse.diags,"metadata_contract") &&
        xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags))) {
        xl6DiagPrintText(&parse.diags,"metadata typed"); return 1;
    }
    verify(&ir);
    for(size_t i=0;i<ir.intrinsicCallCount;++i) {
        XL6_IrIntrinsicCall* call=&ir.intrinsicCalls[i]; XL6_IrIntrinsicCall saved=*call;
        if(!metadata(call->kind)) continue;
        ++typed; assert(call->errorAbi==XL6_IR_ERROR_ABI_MAY_RAISE);
        call->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE; reject(&ir,NULL); *call=saved;
        call->admission=XL6_IR_INTRINSIC_ADMISSION_INVALID; reject(&ir,NULL); *call=saved;
        XL6_IrIntrinsicOperand* receiver=&ir.intrinsicOperands[call->firstOperand];
        XL6_IrIntrinsicOperand borrow=*receiver;
        receiver->abi=XL6_IR_INTRINSIC_OPERAND_VALUE; reject(&ir,NULL); *receiver=borrow;
    }
    if(!xl6IrLowerModule(&ir,&parse.diags)) {
        xl6DiagPrintText(&parse.diags,"metadata lower"); return 1;
    }
    verify(&ir);
    for(size_t i=0;i<ir.instructionCount;++i) {
        XL6_IrInstruction* call=&ir.instructions[i]; XL6_IrInstruction saved=*call;
        if(call->kind!=XL6_IR_INSTR_RUNTIME_CALL ||
            xl6IrRuntimeFutureMetadataErrorPhysicalAbi(call->runtimeOp)==XL6_IR_CALL_ERROR_PHYSICAL_INVALID) continue;
        ++lowered;
        assert(xl6IrCallInstructionErrorAbi(&ir,call)==XL6_IR_ERROR_ABI_MAY_RAISE &&
            xl6IrCallInstructionErrorPhysicalAbi(&ir,call)==
                (call->runtimeOp==XL6_IR_RUNTIME_FUTURE_DEBUG_NAME_COPY ?
                    XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT : XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE));
        assert(i+1<ir.instructionCount && ir.instructions[i+1].kind==XL6_IR_INSTR_ERROR_TEST);
        XL6_IrInstruction* test=&ir.instructions[i+1]; XL6_IrInstruction testSaved=*test;
        test->kind=XL6_IR_INSTR_DISCARD; reject(&ir,NULL); *test=testSaved;
        test->falseTarget=test->trueTarget; reject(&ir,NULL); *test=testSaved;
        call->operandCount=0; reject(&ir,NULL); *call=saved;
        call->targetSymbol=1; reject(&ir,NULL); *call=saved;
    }
    assert(typed>=5 && lowered==typed && mutations==typed*7); verify(&ir);
    assert(xl6OutputOpenMemory(&out) && xl6IrCgenEmitC(&out,&ir,&parse.diags));
    text=xl6OutputTakeMemoryText(&out,NULL); assert(text);
    const char* names[]={"xl6FutureDebugNameSetV2(","xl6FutureDebugNameCopyV2(","xl6FutureFirstObservedMsV2(",
        "xl6FutureFirstTerminalObservedMsV2(","xl6FuturePendingWatchesV2("};
    for(size_t i=0;i<sizeof(names)/sizeof(*names);++i) assert(strstr(text,names[i]));
    assert(!strstr(text,"xl6FutureName(") && !strstr(text,"xlir_future_set_debug_name(")); xrtFree(text);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    printf("Future metadata IR: %zu fallible operations, %zu typed/lowered/C/header rejected mutations, direct byte-length ABI\n",typed,mutations);
    return 0;
}
