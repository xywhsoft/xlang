#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    for(unsigned header=0;header<2;++header) {
        XL6_Output out;size_t count=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_PLACE",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&count);assert(text && !count);
        xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));
    unsigned paths=0,bounds=0,sets=0;
    for(size_t n=0;n<ir->nodeCount;++n) {
        XL6_IrNode* node=&ir->nodes[n];
        if(node->memberAbi!=XL6_IR_MEMBER_ABI_C_AGGREGATE_PATH_WRITE)continue;
        ++paths;XL6_IrMemberAbi abi=node->memberAbi;
        node->memberAbi=XL6_IR_MEMBER_ABI_UNION_FIELD;refused(ir);node->memberAbi=abi;
        size_t field=node->unionField;node->unionField=SIZE_MAX;refused(ir);node->unionField=field;
    }
    for(size_t i=0;i<ir->instructionCount;++i) {
        XL6_IrInstruction* instruction=&ir->instructions[i];
        if(instruction->kind==XL6_IR_INSTR_C_ARRAY_SET) {
            ++sets;XL6_IrValueId* operands=ir->operands+instruction->firstOperand;
            XL6_IrValueId proof=operands[3];operands[3]=0;refused(ir);operands[3]=proof;
            XL6_IrValueId key=operands[1];operands[1]=proof;refused(ir);operands[1]=key;
        }
        if(instruction->kind!=XL6_IR_INSTR_C_ARRAY_BOUNDS)continue;
        ++bounds;
        for(size_t b=0;b<ir->blockCount;++b) {
            XL6_IrBlock* block=&ir->blocks[b];if(!block->instructionCount)continue;
            XL6_IrInstruction* branch=&ir->instructions[block->firstInstruction+block->instructionCount-1];
            if(branch->kind!=XL6_IR_INSTR_BRANCH || branch->operandCount!=1 ||
               ir->operands[branch->firstOperand]!=instruction->result)continue;
            XL6_IrBlockId target=branch->falseTarget;branch->falseTarget=branch->trueTarget;
            refused(ir);branch->falseTarget=target;
        }
    }
    assert(paths>=5 && sets>=5 && bounds>=5 && mutations>=30 && xl6IrVerifyModule(ir,NULL));
    printf("aggregate places: %u paths, %u bounds, %u sets; %u unsafe mutations rejected before C/header output\n",
        paths,bounds,sets,mutations);
    xl6CompileUnitFree(&unit);return 0;
}
