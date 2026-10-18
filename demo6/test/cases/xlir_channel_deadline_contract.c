/* Observe the genuine compiler product; never compile compiler source here. */
#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
_Static_assert(XL6_IR_RUNTIME_RESOURCE_OP_CHANNEL_SEND_TIMEOUT==
    XL6_IR_RUNTIME_RESOURCE_OP_CHANNEL_RECEIVE_FOR+1,"stable source op identities");
_Static_assert(XL6_IR_RUNTIME_CHANNEL_SEND_TIMEOUT==
    XL6_IR_RUNTIME_XVALUE_CAST_NULLABLE_BYTES+1,"stable physical op identities");
static size_t refusals;
static void refuse(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module,NULL));
    for (int header=0;header<2;++header) {
        XL6_Output output; XL6_DiagList diags; size_t size=SIZE_MAX;
        xl6DiagInit(&diags); assert(xl6OutputOpenMemory(&output));
        assert(!(header ? xl6IrCgenEmitHeader(&output,module,"BAD_DEADLINE",&diags) :
            xl6IrCgenEmitC(&output,module,&diags)));
        char* text=xl6OutputTakeMemoryText(&output,&size); assert(!size);
        xrtFree(text); xl6OutputDiscard(&output); xl6DiagFree(&diags);
    }
    ++refusals;
}
int main(int argc,char** argv)
{
    assert(argc==2 || (argc==3 && !strcmp(argv[2],"--minimal")));
    bool minimal=argc==3; XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    XL6_IrModule* module=&unit.ir; size_t typed=0,dependent=0,physical=0,send=0,receive=0;
    for (size_t i=0;i<module->intrinsicCallCount;++i) {
        XL6_IrIntrinsicCall* call=&module->intrinsicCalls[i];
        const XL6_IrChannelSendSpec* s=xl6IrChannelSendSpecResource(call->runtimeResourceOp);
        const XL6_IrChannelReceiveSpec* r=xl6IrChannelReceiveSpecResource(call->runtimeResourceOp);
        if ((!s || s->waitMode!=3) && (!r || r->waitMode!=3)) continue;
        XL6_IrIntrinsicCall saved=*call;
        XL6_IrIntrinsicOperand* time=module->intrinsicOperands+call->firstOperand+(s ? 2 : 1);
        const XL6_IrType* type=xl6IrTypeGet(&module->types,time->type);
        assert(type && type->cAbi==XL6_IR_C_ABI_U64 && type->integerTarget==XL6_IR_INTEGER_TARGET_UNSIGNED_64);
        assert(time->abi==XL6_IR_INTRINSIC_OPERAND_VALUE);
        assert(call->errorAbi==XL6_IR_ERROR_ABI_MAY_RAISE);
        if (call->admission==XL6_IR_INTRINSIC_ADMISSION_GENERIC_DEPENDENT) ++dependent;
        else { assert(call->admission==XL6_IR_INTRINSIC_ADMISSION_VERIFIED); ++typed; }
#define BAD(Field,Value) do { call->Field=(Value); refuse(module); *call=saved; } while(0)
        BAD(errorAbi,XL6_IR_ERROR_ABI_NO_RAISE); BAD(operandCount,0); BAD(payloadType,0);
        BAD(runtimeResourceOp,s ? XL6_IR_RUNTIME_RESOURCE_OP_CHANNEL_SEND_TIMEOUT :
            XL6_IR_RUNTIME_RESOURCE_OP_CHANNEL_RECEIVE_FOR);
#undef BAD
        XL6_IrIntrinsicOperand savedTime=*time;
        time->abi=XL6_IR_INTRINSIC_OPERAND_BORROW; refuse(module); *time=savedTime;
    }
    assert(typed && (minimal || dependent) && xl6IrVerifyModule(module,NULL));
    assert(xl6IrLowerModule(module,&unit.parse.diags));
    for (size_t i=0;i<module->instructionCount;++i) {
        XL6_IrInstruction* ins=&module->instructions[i];
        const XL6_IrChannelSendSpec* s=xl6IrChannelSendSpecRuntime(ins->runtimeOp);
        const XL6_IrChannelReceiveSpec* r=xl6IrChannelReceiveSpecRuntime(ins->runtimeOp);
        if (ins->kind!=XL6_IR_INSTR_RUNTIME_CALL || ((!s || s->waitMode!=3) && (!r || r->waitMode!=3))) continue;
        assert(xl6IrChannelInstructionCompatible(module,ins)); ++physical;
        if (s) ++send; else ++receive;
        for (size_t j=0;j<ins->operandCount;++j)
            assert(xl6IrInstructionOperandOwnership(module,ins,j)==XL6_IR_OPERAND_OWNERSHIP_OBSERVE);
        XL6_IrInstruction saved=*ins;
#define BAD(Field,Value) do { ins->Field=(Value); refuse(module); *ins=saved; } while(0)
        BAD(runtimeOp,s ? XL6_IR_RUNTIME_CHANNEL_SEND_TIMEOUT : XL6_IR_RUNTIME_CHANNEL_RECEIVE_FOR);
        BAD(operandCount,0); BAD(payloadType,0); BAD(runtimeIdentityType,ins->payloadType);
        BAD(runtimeResourcePayloadAbi,XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID);
#undef BAD
        XL6_IrValueId* time=module->operands+ins->firstOperand+(s ? 2 : 1);
        XL6_IrValueId savedTime=*time; *time=module->operands[ins->firstOperand];
        refuse(module); *time=savedTime;
    }
    assert(physical && send && receive && xl6IrVerifyModule(module,NULL));
    XL6_Output generated; XL6_DiagList diags; size_t size=0;
    xl6DiagInit(&diags); assert(xl6OutputOpenMemory(&generated));
    assert(xl6IrCgenEmitC(&generated,module,&diags));
    char* text=xl6OutputTakeMemoryText(&generated,&size); assert(text && size);
    assert(strstr(text,"#include <__xl_runtime/xl6_channel_wait_abi.h>"));
    assert(strstr(text,"bool channel_ok = xl6ChannelSendWaitV2("));
    assert(strstr(text,"bool channel_ok = xl6ChannelReceiveWaitV2("));
    xrtFree(text); xl6OutputDiscard(&generated); xl6DiagFree(&diags);
    xl6CompileUnitFree(&unit);
    printf("Channel deadline: %zu typed, %zu dependent, %zu physical (%zu send/%zu receive), %zu IR refusals\n",
        typed,dependent,physical,send,receive,refusals);
    return 0;
}
