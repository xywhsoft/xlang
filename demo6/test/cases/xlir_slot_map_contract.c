#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(const XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    for(unsigned header=0;header<2;++header) {
        XL6_Output out; size_t size=SIZE_MAX; assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"REJECTED_SLOT_MAP",NULL) : xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&size); assert(!size); xrtFree(text); xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2); XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* ir=&unit.ir; assert(xl6IrVerifyModule(ir,NULL)); unsigned calls=0;
    for(size_t i=0;i<ir->instructionCount;++i) {
        XL6_IrInstruction* ins=&ir->instructions[i];
        if(ins->kind!=XL6_IR_INSTR_RUNTIME_CALL || !xl6IrSlotMapNativeSymbol(ins->runtimeOp)) continue;
        ++calls; assert(xl6IrSlotMapInstructionCompatible(ir,ins)); XL6_IrInstruction saved=*ins;
        for(size_t p=0;p<ins->operandCount;++p)
            assert(xl6IrInstructionOperandOwnership(ir,ins,p)==XL6_IR_OPERAND_OWNERSHIP_OBSERVE);
        assert(xl6IrCallInstructionErrorAbi(ir,ins)==XL6_IR_ERROR_ABI_MAY_RAISE);
        assert(xl6IrCallInstructionErrorPhysicalAbi(ir,ins)==xl6IrSlotMapErrorPhysicalAbi(ins->runtimeOp));
#define BAD(Field,Value) do { ins->Field=(Value); refused(ir); *ins=saved; } while(0)
        BAD(payloadType,0); BAD(type,0); BAD(runtimeResourcePayloadAbi,XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID);
        BAD(opcode,XL6_IR_OPCODE_ADD); BAD(runtimeIdentityType,saved.type); BAD(runtimeResourceKeyType,saved.type);
        BAD(operandCount,saved.operandCount+1); BAD(targetValue,1);
#undef BAD
        XL6_IrInstruction outcome=ir->instructions[i+1]; assert(outcome.kind==XL6_IR_INSTR_ERROR_TEST);
        ir->instructions[i+1].kind=XL6_IR_INSTR_VALUE; refused(ir); ir->instructions[i+1]=outcome;
    }
    for(size_t i=0;i<ir->types.count;++i) {
        XL6_IrType* t=&ir->types.items[i];
        if(t->runtimeResourceAbi!=XL6_IR_RUNTIME_RESOURCE_SLOT_MAP || t->kind!=XL6_IR_TYPE_GENERIC_INSTANCE) continue;
        XL6_IrType saved=*t;
        t->collectionValueSchemaType=0; refused(ir); *t=saved;
        t->runtimeResourceAbi=XL6_IR_RUNTIME_RESOURCE_STACK; refused(ir); *t=saved;
        t->ownershipAbi=XL6_IR_OWNERSHIP_ABI_TRIVIAL; refused(ir); *t=saved;
    }
    assert(calls && xl6IrVerifyModule(ir,NULL));
    printf("slot-map calls=%u mutations=%u\n",calls,mutations); xl6CompileUnitFree(&unit); return 0;
}
