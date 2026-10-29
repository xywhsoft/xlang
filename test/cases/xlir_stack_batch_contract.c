/* Actual closed IR must reject corrupted batch schemas before C/header output. */
#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void rejected(const XL6_IrModule* module)
{
    int valid=xl6IrVerifyModule(module,NULL);
    if (valid) fprintf(stderr,"accepted corruption after %u rejected mutations\n",mutations);
    assert(!valid);
    for (unsigned header=0;header<2;++header) {
        XL6_Output output; XL6_DiagList diagnostics; size_t size=SIZE_MAX;
        xl6DiagInit(&diagnostics); assert(xl6OutputOpenMemory(&output));
        assert(!(header ? xl6IrCgenEmitHeader(&output,module,"BATCH_REJECTED",&diagnostics)
            : xl6IrCgenEmitC(&output,module,&diagnostics)));
        char* text=xl6OutputTakeMemoryText(&output,&size);
        assert(size==0); xrtFree(text); xl6OutputDiscard(&output); xl6DiagFree(&diagnostics);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2); XL6_ParseResult parse; XL6_IrModule module;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&module);
    assert(xl6ParseFile(argv[1],&parse) && xl6StdlibExpandModule(parse.root,&parse.diags) &&
        xl6SemCheckModule(parse.root,&parse.diags,"batch_contract") &&
        xl6IrBuildModuleFromAst(&module,parse.root,&parse.diags) && xl6IrLowerModule(&module,&parse.diags));
    assert(xl6IrVerifyModule(&module,NULL)); unsigned checked=0, mask=0;
    for (size_t index=0;index<module.instructionCount;++index) {
        XL6_IrInstruction* call=&module.instructions[index];
        if (call->kind!=XL6_IR_INSTR_RUNTIME_CALL || call->runtimeOp<XL6_IR_RUNTIME_STACK_PUSH_BATCH ||
            call->runtimeOp>XL6_IR_RUNTIME_STACK_PEEK_BATCH_AT) continue;
        assert(xl6IrStackBatchInstructionCompatible(&module,call));
        XL6_IrInstruction saved=*call;
#define CORRUPT(field,value) do { call->field=(value); rejected(&module); *call=saved; } while(0)
        CORRUPT(payloadType,XL6_IR_TYPE_INVALID);
        CORRUPT(runtimeResourcePayloadAbi,XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID);
        CORRUPT(operandCount,1);
        CORRUPT(type,XL6_IR_TYPE_INVALID);
        CORRUPT(result,XL6_IR_VALUE_INVALID);
        CORRUPT(opcode,XL6_IR_OPCODE_ADD);
        CORRUPT(immediateText,"not-a-runtime-operand");
        CORRUPT(sequenceAdmissionIdentitySet,true);
        CORRUPT(runtimeResourceKeyType,saved.payloadType);
#undef CORRUPT
        XL6_IrIntrinsicCall* intrinsic=(XL6_IrIntrinsicCall*)xl6IrIntrinsicCallGet(&module,call->origin);
        assert(intrinsic); XL6_IrIntrinsicCall original=*intrinsic;
        intrinsic->resultAbi=original.resultAbi==XL6_IR_INTRINSIC_RESULT_OWNED ?
            XL6_IR_INTRINSIC_RESULT_VALUE : XL6_IR_INTRINSIC_RESULT_OWNED;
        rejected(&module); *intrinsic=original;
        intrinsic->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE; rejected(&module); *intrinsic=original;
        XL6_IrIntrinsicOperand* argument=&module.intrinsicOperands[intrinsic->firstOperand];
        XL6_IrIntrinsicOperandAbi abi=argument->abi;
        argument->abi=XL6_IR_INTRINSIC_OPERAND_VALUE; rejected(&module); argument->abi=abi;
        assert(index+1<module.instructionCount);
        XL6_IrInstruction* test=&module.instructions[index+1];
        assert(test->kind==XL6_IR_INSTR_ERROR_TEST && test->operandCount==1);
        XL6_IrValueId outcome=module.operands[test->firstOperand];
        module.operands[test->firstOperand]=XL6_IR_VALUE_INVALID; rejected(&module);
        module.operands[test->firstOperand]=outcome;
        XL6_IrBlockId success=test->trueTarget;
        test->trueTarget=test->falseTarget; rejected(&module); test->trueTarget=success;
        XL6_IrType* owner=NULL;
        for (size_t i=0;i<module.instructionCount;++i)
            if (module.instructions[i].result==module.operands[call->firstOperand])
                owner=&module.types.items[module.instructions[i].type-1];
        assert(owner); XL6_IrSequenceLayout layout=owner->sequenceLayout;
        owner->sequenceLayout.elementType=XL6_IR_TYPE_INVALID; rejected(&module); owner->sequenceLayout=layout;
        owner->sequenceLayout.transferAbi=XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID;
        rejected(&module); owner->sequenceLayout=layout;
        uint64_t identity=owner->typeValueId;
        owner->typeValueId=0; rejected(&module); owner->typeValueId=identity;
        assert(xl6IrVerifyModule(&module,NULL));
        mask|=1u<<(call->runtimeOp-XL6_IR_RUNTIME_STACK_PUSH_BATCH); ++checked;
    }
    assert(checked && mask==15);
    printf("Stack batch IR: %u calls, %u mutations refused before C/header output\n",checked,mutations);
    xl6IrModuleFree(&module); xl6ParseResultFree(&parse); return 0;
}
