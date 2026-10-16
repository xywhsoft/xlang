#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>

static unsigned mutations;
static void refused(const XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    for(unsigned header=0;header<2;++header) {
        XL6_Output out;size_t size=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"REJECTED_REMOVE_RANGE",NULL) : xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&size);assert(size==0);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));unsigned calls=0;
    for(size_t i=0;i<ir->instructionCount;++i) {
        XL6_IrInstruction* ins=&ir->instructions[i];
        if(ins->runtimeOp!=XL6_IR_RUNTIME_XVALUE_COLLECTION_REMOVE || ins->collectionAbi!=XL6_IR_COLLECTION_ARRAY) continue;
        ++calls;assert(ins->operandCount==3 && xl6IrCollectionMemberRuntimeInstructionCompatible(ir,ins));
        XL6_IrInstruction saved=*ins;
        ins->operandCount=2;refused(ir);*ins=saved;
        ins->collectionAbi=XL6_IR_COLLECTION_LIST;refused(ir);*ins=saved;
        ins->type=0;refused(ir);*ins=saved;
        XL6_IrValueId* count=&ir->operands[ins->firstOperand+2];XL6_IrValueId original=*count;
        *count=ir->operands[ins->firstOperand];refused(ir);*count=original;
    }
    assert(calls && xl6IrVerifyModule(ir,NULL));
    printf("array-remove calls=%u mutations=%u\n",calls,mutations);
    xl6CompileUnitFree(&unit);return 0;
}
