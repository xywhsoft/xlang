#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static size_t refusals;
static void refuse(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module,NULL));
    for(int header=0;header<2;++header) {
        XL6_Output out; XL6_DiagList diags; size_t size=SIZE_MAX;
        xl6DiagInit(&diags); assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,module,"BAD_CHANNEL_CANCEL",&diags) :
            xl6IrCgenEmitC(&out,module,&diags)));
        char* text=xl6OutputTakeMemoryText(&out,&size); assert(!size);
        xrtFree(text); xl6OutputDiscard(&out); xl6DiagFree(&diags);
    }
    ++refusals;
}
static XL6_IrInstruction* definition(XL6_IrModule* module,XL6_IrValueId id)
{
    for(size_t i=0;i<module->instructionCount;++i) if(module->instructions[i].result==id) return &module->instructions[i];
    return NULL;
}
static void privateUseRefused(XL6_IrModule* module,const XL6_IrInstruction* getter)
{
    /* Independently test the loan component, without letting a new unused
     * instruction's unrelated block/SSA bookkeeping cause the refusal. */
    XL6_IrModule probe=*module;
    probe.instructions=malloc((module->instructionCount+1)*sizeof(*probe.instructions));
    probe.operands=malloc((module->operandCount+1)*sizeof(*probe.operands));
    assert(probe.instructions && probe.operands);
    memcpy(probe.instructions,module->instructions,module->instructionCount*sizeof(*probe.instructions));
    memcpy(probe.operands,module->operands,module->operandCount*sizeof(*probe.operands));
    probe.instructions[probe.instructionCount++]=(XL6_IrInstruction){.kind=XL6_IR_INSTR_MOVE,
        .type=getter->type,.origin=getter->origin,.firstOperand=probe.operandCount,.operandCount=1};
    probe.operands[probe.operandCount++]=getter->result;
    assert(!xl6IrChannelCancellationProjectionCompatible(&probe,getter));
    free(probe.instructions); free(probe.operands);
    assert(xl6IrChannelCancellationProjectionCompatible(module,getter));
}
int main(int argc,char** argv)
{
    assert(argc==2 || argc==3); XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1)); XL6_IrModule* module=&unit.ir;
    size_t typed=0,dependent=0,physical=0; unsigned sends=0,receives=0;
    for(size_t i=0;i<module->intrinsicCallCount;++i) {
        XL6_IrIntrinsicCall* call=&module->intrinsicCalls[i];
        const XL6_IrChannelSendSpec* send=xl6IrChannelSendSpecResource(call->runtimeResourceOp);
        const XL6_IrChannelReceiveSpec* receive=xl6IrChannelReceiveSpecResource(call->runtimeResourceOp);
        if(!((send && send->cancelled) || (receive && receive->cancelled))) continue;
        XL6_IrIntrinsicCall saved=*call;
        XL6_IrIntrinsicOperand* token=module->intrinsicOperands+call->firstOperand+call->operandCount-1;
        assert(token->abi==XL6_IR_INTRINSIC_OPERAND_BORROW);
        const XL6_IrClassField* field=xl6IrChannelCancellationField(module,token->type); assert(field);
        assert(xl6IrChannelCancellationOperand(call->runtimeResourceOp,call->operandCount-1));
        assert(!xl6IrChannelCancellationOperand(call->runtimeResourceOp,0));
        assert(!xl6IrChannelCancellationField(module,call->receiverType));
        if(call->admission==XL6_IR_INTRINSIC_ADMISSION_GENERIC_DEPENDENT) ++dependent;
        else { assert(call->admission==XL6_IR_INTRINSIC_ADMISSION_VERIFIED); ++typed; }
#define BAD(Field,Value) do { call->Field=(Value); refuse(module); *call=saved; } while(0)
        BAD(operandCount,call->operandCount-1); BAD(errorAbi,XL6_IR_ERROR_ABI_NO_RAISE);
        BAD(payloadType,0); BAD(runtimeResourceKeyType,call->payloadType);
#undef BAD
        XL6_IrIntrinsicOperand savedToken=*token;
        token->abi=XL6_IR_INTRINSIC_OPERAND_VALUE; refuse(module); *token=savedToken;
        token->type=call->receiverType; refuse(module); *token=savedToken;
    }
    assert(typed && dependent && xl6IrVerifyModule(module,NULL));
    assert(xl6IrLowerModule(module,&unit.parse.diags));
    for(size_t i=0;i<module->instructionCount;++i) {
        XL6_IrInstruction* ins=&module->instructions[i];
        const XL6_IrChannelSendSpec* send=xl6IrChannelSendSpecRuntime(ins->runtimeOp);
        const XL6_IrChannelReceiveSpec* receive=xl6IrChannelReceiveSpecRuntime(ins->runtimeOp);
        if(ins->kind!=XL6_IR_INSTR_RUNTIME_CALL || !((send && send->cancelled) || (receive && receive->cancelled))) continue;
        assert(xl6IrChannelInstructionCompatible(module,ins)); ++physical;
        if(send) sends|=1u<<send->waitMode; else receives|=1u<<receive->waitMode;
        for(size_t j=0;j<ins->operandCount;++j)
            assert(xl6IrInstructionOperandOwnership(module,ins,j)==XL6_IR_OPERAND_OWNERSHIP_OBSERVE);
        XL6_IrValueId* raw=module->operands+ins->firstOperand+ins->operandCount-1;
        XL6_IrInstruction* getter=definition(module,*raw); assert(getter);
        assert(xl6IrChannelCancellationProjectionCompatible(module,getter));
        privateUseRefused(module,getter);
        XL6_IrInstruction saved=*ins,savedGetter=*getter;
#define BAD(Field,Value) do { ins->Field=(Value); refuse(module); *ins=saved; } while(0)
        BAD(operandCount,ins->operandCount-1); BAD(payloadType,0); BAD(runtimeIdentityType,ins->payloadType);
        BAD(runtimeResourcePayloadAbi,XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID);
        BAD(origin,0);
#undef BAD
        XL6_IrValueId savedRaw=*raw; *raw=module->operands[ins->firstOperand]; refuse(module); *raw=savedRaw;
#define BAD_GET(Field,Value) do { getter->Field=(Value); \
    assert(!xl6IrChannelCancellationProjectionCompatible(module,getter)); refuse(module); *getter=savedGetter; } while(0)
        BAD_GET(immediateText,"\"different-field\""); BAD_GET(origin,0); BAD_GET(payloadType,ins->payloadType);
        BAD_GET(runtimeOp,XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_RAW_POINTER);
        BAD_GET(targetValue,getter->result);
        BAD_GET(runtimeResourcePayloadAbi,XL6_IR_RUNTIME_RESOURCE_PAYLOAD_STRING_COPY);
        BAD_GET(sequenceAdmissionIdentitySet,1);
#undef BAD_GET
        XL6_IrInstruction* object=definition(module,module->operands[getter->firstOperand]); assert(object);
        const XL6_IrClassField* field=xl6IrChannelCancellationField(module,object->type); assert(field);
        XL6_IrClassField* mutableField=&module->classFields[field-module->classFields];
        const char* policy=mutableField->nativeHandlePolicyCName;
        mutableField->nativeHandlePolicyCName="xl6CancellationSourceHandlePolicyV1Get";
        refuse(module); mutableField->nativeHandlePolicyCName=policy;
        assert(xl6IrChannelInstructionCompatible(module,ins));
    }
    assert(physical && sends==14 && receives==14 && xl6IrVerifyModule(module,NULL));
    XL6_Output out; XL6_DiagList diags; size_t size=0;
    xl6DiagInit(&diags); assert(xl6OutputOpenMemory(&out)); assert(xl6IrCgenEmitC(&out,module,&diags));
    char* text=xl6OutputTakeMemoryText(&out,&size); assert(size && text);
    assert(strstr(text," != NULL && xl6ChannelSendCancelV2("));
    assert(strstr(text," != NULL && xl6ChannelReceiveCancelV2("));
    assert(strstr(text,"#include <__xl_runtime/xl6_channel_wait_abi.h>"));
    xrtFree(text); xl6OutputDiscard(&out); xl6DiagFree(&diags); xl6CompileUnitFree(&unit);
    size_t flows=0;
    if(argc==3) {
        xl6CompileUnitInit(&unit); assert(xl6CompileUnitLoadFile(&unit,argv[2],1)); module=&unit.ir;
        for(size_t i=0;i<module->intrinsicCallCount;++i) {
            XL6_IrIntrinsicCall* call=&module->intrinsicCalls[i];
            if(call->kind!=XL6_IR_INTRINSIC_RUNTIME_RESOURCE) continue;
            for(size_t j=0;j<call->operandCount;++j) {
                XL6_IrIntrinsicOperand* operand=module->intrinsicOperands+call->firstOperand+j;
                const XL6_IrNode* value=xl6IrNodeGet(module,operand->expression); assert(value);
                const XL6_IrType* slot=xl6IrTypeGet(&module->types,value->storageType);
                if(!slot || slot->kind!=XL6_IR_TYPE_NULLABLE || operand->type==value->storageType) continue;
                assert(operand->type==xl6IrNodeMaterializedValueType(module,value));
                XL6_IrTypeId type=operand->type;
                operand->type=value->storageType; refuse(module); operand->type=type; ++flows;
            }
        }
        assert(flows>=5 && xl6IrVerifyModule(module,NULL));
        assert(xl6IrLowerModule(module,&unit.parse.diags) && xl6IrVerifyModule(module,NULL));
        xl6CompileUnitFree(&unit);
    }
    printf("Channel cancellation: %zu typed, %zu dependent, %zu physical, %zu refusals, %zu flow inputs; private loans sealed\n",
        typed,dependent,physical,refusals,flows); return 0;
}
