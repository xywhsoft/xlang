#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    for(unsigned header=0;header<2;++header){
        XL6_Output out;size_t size=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header?xl6IrCgenEmitHeader(&out,ir,"REJECTED_POINT_SEQUENCE",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&size);assert(!size);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    if(!xl6CompileUnitLoadFile(&unit,argv[1],1))goto failed;
    XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));
    if(!xl6IrLowerModule(ir,&unit.parse.diags))goto failed;
    assert(xl6IrVerifyModule(ir,NULL));unsigned schemas=0,loops=0,projections=0,comparisons=0;
    for(size_t i=0;i<ir->types.count;++i){
        XL6_IrSequenceLayout* plan=&ir->types.items[i].sequenceLayout;
        if(plan->slotAbi!=XL6_IR_NATIVE_SLOT_RAW_POINTER)continue;
        assert(plan->abi==XL6_IR_SEQUENCE_LAYOUT_C_SCALAR_V1 &&
            plan->transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_TRIVIAL_COPY &&
            plan->codecAbi==XL6_IR_SEQUENCE_CODEC_DIRECT && plan->admissionAbi==XL6_VALUE_ADMIT_NONE);
        XL6_IrSequenceLayout saved=*plan;++schemas;
#define BAD(Field,Value) do{plan->Field=(Value);refused(ir);*plan=saved;}while(0)
        BAD(slotAbi,XL6_IR_NATIVE_SLOT_U64);
        BAD(transferAbi,XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN);
        BAD(codecAbi,XL6_IR_SEQUENCE_CODEC_CLOSURE_VALUE_V1);
        BAD(admissionAbi,XL6_VALUE_ADMIT_ANY);
        BAD(baseElementType,XL6_IR_TYPE_INVALID);
#undef BAD
    }
    for(size_t i=0;i<ir->nodeCount;++i){
        XL6_IrNode* node=&ir->nodes[i];if(node->loopItemAbi!=XL6_IR_LOOP_ITEM_RAW_POINTER_COPY)continue;
        assert(node->loopItemLifetimeAbi==XL6_IR_LOOP_TARGET_LIFETIME_TRIVIAL_REPLACE);++loops;
        XL6_IrNode saved=*node;node->loopItemAbi=XL6_IR_LOOP_ITEM_U64;refused(ir);*node=saved;
        node->loopItemAbi=(XL6_IrLoopItemAbi)(XL6_IR_LOOP_ITEM_RAW_POINTER_COPY+1);refused(ir);*node=saved;
        node->loopItemLifetimeAbi=XL6_IR_LOOP_TARGET_LIFETIME_OWNED_ITERATION_RELEASE;refused(ir);*node=saved;
    }
    for(size_t i=0;i<ir->instructionCount;++i){
        XL6_IrInstruction* ins=&ir->instructions[i];
        if(ins->kind==XL6_IR_INSTR_BINARY &&
            (ins->opcode==XL6_IR_OPCODE_EQ || ins->opcode==XL6_IR_OPCODE_NE) && ins->operandCount==2){
            const XL6_IrInstruction* left=NULL;const XL6_IrInstruction* right=NULL;
            for(size_t j=0;j<i;++j){
                const XL6_IrInstruction* v=&ir->instructions[j];
                if(v->result==ir->operands[ins->firstOperand])left=v;
                if(v->result==ir->operands[ins->firstOperand+1])right=v;
            }
            if(left && right && left->type!=right->type &&
                xl6IrNativeSlotAbiForType(&ir->types,xl6IrTypeGet(&ir->types,left->type))==XL6_IR_NATIVE_SLOT_RAW_POINTER &&
                xl6IrNativeSlotAbiForType(&ir->types,xl6IrTypeGet(&ir->types,right->type))==XL6_IR_NATIVE_SLOT_RAW_POINTER){
                ++comparisons;XL6_IrOpcode saved=ins->opcode;ins->opcode=XL6_IR_OPCODE_LT;refused(ir);ins->opcode=saved;
            }
        }
        if(ins->runtimeOp==XL6_IR_RUNTIME_XVALUE_TYPED_COLLECTION_METHOD &&
            (ins->typedCollectionOp==XL6_IR_TYPED_COLLECTION_POP || ins->typedCollectionOp==XL6_IR_TYPED_COLLECTION_TAKE)){
            assert(xl6IrCallInstructionErrorPhysicalAbi(ir,ins)==XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT);
        }
        if(ins->runtimeOp!=XL6_IR_RUNTIME_XVALUE_PROJECT_RAW_POINTER_BORROW)continue;
        ++projections;XL6_IrTypeId saved=ins->payloadType;
        ins->payloadType=XL6_IR_TYPE_INVALID;refused(ir);ins->payloadType=saved;
    }
    assert(schemas>=3&&loops>=3&&projections>=3&&comparisons>=5);
    printf("point Sequence: %u exact schemas, %u trivial cursor targets, %u exact pointer projections, %u pointer equalities, %u rejected C/header mutations\n",schemas,loops,projections,comparisons,mutations);
    xl6CompileUnitFree(&unit);return 0;
failed:
    for(size_t i=0;i<unit.parse.diags.count;++i)fprintf(stderr,"%d:%d %s\n",unit.parse.diags.items[i].line,unit.parse.diags.items[i].column,unit.parse.diags.items[i].message);
    xl6CompileUnitFree(&unit);return 2;
}
