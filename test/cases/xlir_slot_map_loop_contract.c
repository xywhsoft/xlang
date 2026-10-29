#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations,calls,keys,lanes;
static void refused(const XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    for(unsigned header=0;header<2;++header) {
        XL6_Output out; size_t size=SIZE_MAX; assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"REJECTED_SLOT_MAP_CURSOR",NULL) : xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&size); assert(!size); xrtFree(text); xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2); XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* ir=&unit.ir; assert(xl6IrVerifyModule(ir,NULL));
    for(size_t i=0;i<ir->instructionCount;++i) {
        XL6_IrInstruction* ins=&ir->instructions[i];
        if(ins->kind!=XL6_IR_INSTR_RUNTIME_CALL || !xl6IrSlotMapIteratorOpIs(ins->runtimeOp)) continue;
        ++calls; assert(xl6IrSlotMapIteratorInstructionCompatible(ir,ins)); XL6_IrInstruction saved=*ins;
        assert(xl6IrInstructionOperandOwnership(ir,ins,0)==XL6_IR_OPERAND_OWNERSHIP_OBSERVE);
        assert(xl6IrCallInstructionErrorAbi(ir,ins)==XL6_IR_ERROR_ABI_MAY_RAISE);
        bool copy=ins->runtimeOp==XL6_IR_RUNTIME_SLOT_MAP_ITERATOR_KEY_COPY;
        if(copy) ++keys; else lanes|=1u<<(ins->runtimeOp-XL6_IR_RUNTIME_SLOT_MAP_ITERATOR_PAIRS);
        assert(xl6IrCallInstructionErrorPhysicalAbi(ir,ins)==(copy ?
            XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT : XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT));
#define BAD(Field,Value) do { ins->Field=(Value); refused(ir); *ins=saved; } while(0)
        BAD(payloadType,saved.type); BAD(type,0); BAD(runtimeResourcePayloadAbi,XL6_IR_RUNTIME_RESOURCE_PAYLOAD_NATIVE_VALUE_COPY);
        BAD(opcode,XL6_IR_OPCODE_ADD); BAD(runtimeIdentityType,saved.type); BAD(runtimeResourceKeyType,saved.type);
        BAD(operandCount,2); BAD(targetValue,1); BAD(targetSymbol,1); BAD(immediateSize,1);
        BAD(sequenceAdmissionIdentitySet,1); BAD(collectionAbi,XL6_IR_COLLECTION_DICT);
        BAD(runtimeResourceKeyPayloadAbi,XL6_IR_RUNTIME_RESOURCE_PAYLOAD_TRIVIAL_COPY);
        if(!copy) {
            BAD(runtimeOp,saved.runtimeOp==XL6_IR_RUNTIME_SLOT_MAP_ITERATOR_COUNT ?
                XL6_IR_RUNTIME_SLOT_MAP_ITERATOR_PAIRS : XL6_IR_RUNTIME_SLOT_MAP_ITERATOR_COUNT);
            BAD(origin,0);
        }
#undef BAD
        XL6_IrInstruction outcome=ir->instructions[i+1]; assert(outcome.kind==XL6_IR_INSTR_ERROR_TEST);
        ir->instructions[i+1].kind=XL6_IR_INSTR_VALUE; refused(ir); ir->instructions[i+1]=outcome;
        assert(xl6IrVerifyModule(ir,NULL));
    }
    assert(calls && keys && lanes==15);
    printf("slot-map cursor calls=%u keys=%u lanes=%u mutations=%u\n",calls,keys,lanes,mutations);
    xl6CompileUnitFree(&unit); return 0;
}
