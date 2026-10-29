#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t refusals;
static void refuse(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module,NULL));
    for (int header=0;header<2;++header) {
        XL6_Output output; XL6_DiagList errors; size_t size=SIZE_MAX;
        xl6DiagInit(&errors); assert(xl6OutputOpenMemory(&output));
        assert(!(header ? xl6IrCgenEmitHeader(&output,module,"BAD_CHANNEL",&errors) :
            xl6IrCgenEmitC(&output,module,&errors)));
        char* text=xl6OutputTakeMemoryText(&output,&size); assert(!size);
        xrtFree(text); xl6OutputDiscard(&output); xl6DiagFree(&errors);
    }
    ++refusals;
}
int main(int argc,char** argv)
{
    assert(argc==2); XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    XL6_IrModule* module=&unit.ir; size_t typed=0,dependent=0,physical=0; unsigned modes=0;
    for (size_t i=0;i<module->intrinsicCallCount;++i) {
        XL6_IrIntrinsicCall* call=&module->intrinsicCalls[i];
        const XL6_IrChannelSendSpec* spec=xl6IrChannelSendSpecResource(call->runtimeResourceOp);
        if (!spec) continue;
        XL6_IrIntrinsicCall saved=*call;
        XL6_IrIntrinsicOperand* args=module->intrinsicOperands+call->firstOperand;
        if (call->admission==XL6_IR_INTRINSIC_ADMISSION_GENERIC_DEPENDENT) {
            XL6_IrTypeId types[3]={0};
            for(size_t j=0;j<call->operandCount;++j) types[j]=args[j].type;
            assert(xl6IrChannelSendDependentSignatureCompatible(module,call->origin,spec->source,
                call->receiverType,call->resultType,types,call->operandCount));
            assert(!xl6IrChannelSendDependentSignatureCompatible(module,0,spec->source,
                call->receiverType,call->resultType,types,call->operandCount));
            ++dependent;
        } else {
            assert(xl6IrChannelSendIntrinsicCompatible(module,call,args)); ++typed;
        }
#define BAD_CALL(Field,Value) do { call->Field=(Value); refuse(module); *call=saved; } while(0)
        BAD_CALL(payloadType,0); BAD_CALL(receiverType,0); BAD_CALL(operandCount,0);
        BAD_CALL(errorAbi,XL6_IR_ERROR_ABI_NO_RAISE); BAD_CALL(runtimeResourceKeyType,call->payloadType);
        BAD_CALL(resultAbi,XL6_IR_INTRINSIC_RESULT_OWNED);
#undef BAD_CALL
        if (call->admission==XL6_IR_INTRINSIC_ADMISSION_VERIFIED) {
            XL6_IrIntrinsicOperand arg=args[1];
            args[1].abi=XL6_IR_INTRINSIC_OPERAND_STORAGE_REF; refuse(module); args[1]=arg;
        }
    }
    assert(typed && dependent && xl6IrVerifyModule(module,NULL));
    assert(xl6IrLowerModule(module,&unit.parse.diags));
    for (size_t i=0;i<module->instructionCount;++i) {
        XL6_IrInstruction* ins=&module->instructions[i];
        const XL6_IrChannelSendSpec* spec=xl6IrChannelSendSpecRuntime(ins->runtimeOp);
        if (ins->kind!=XL6_IR_INSTR_RUNTIME_CALL || !spec) continue;
        assert(xl6IrChannelInstructionCompatible(module,ins)); ++physical; modes|=1u<<spec->waitMode;
        assert(xl6IrCallInstructionErrorPhysicalAbi(module,ins)==XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT);
        for(size_t j=0;j<ins->operandCount;++j)
            assert(xl6IrInstructionOperandOwnership(module,ins,j)==XL6_IR_OPERAND_OWNERSHIP_OBSERVE);
        XL6_IrInstruction saved=*ins;
#define BAD_INS(Field,Value) do { ins->Field=(Value); refuse(module); *ins=saved; } while(0)
        BAD_INS(payloadType,0); BAD_INS(type,ins->payloadType); BAD_INS(result,0);
        BAD_INS(operandCount,0); BAD_INS(firstOperand,SIZE_MAX);
        BAD_INS(runtimeResourcePayloadAbi,XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID);
        BAD_INS(sequenceAdmissionIdentitySet,1); BAD_INS(runtimeResourceKeyType,ins->payloadType);
        BAD_INS(runtimeIdentityType,ins->payloadType); BAD_INS(immediateText,"invalid");
        BAD_INS(immediateData,(unsigned char*)"invalid"); BAD_INS(immediateSize,1);
        BAD_INS(targetValue,1); BAD_INS(targetSymbol,1); BAD_INS(calleeType,ins->payloadType);
#undef BAD_INS
        assert(i+1<module->instructionCount && module->instructions[i+1].kind==XL6_IR_INSTR_ERROR_TEST);
        XL6_IrInstruction* test=&module->instructions[i+1]; XL6_IrInstructionKind kind=test->kind;
        test->kind=XL6_IR_INSTR_CONST; refuse(module); test->kind=kind;
    }
    assert(physical && modes==7 && xl6IrVerifyModule(module,NULL));
    xl6CompileUnitFree(&unit);
    printf("Channel send: %zu typed, %zu dependent, %zu physical, %zu IR refusals\n",typed,dependent,physical,refusals);
    return 0;
}
