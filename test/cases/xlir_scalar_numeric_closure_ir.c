#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule *m)
{
    assert(!xl6IrVerifyModule(m,NULL));
    for(unsigned header=0;header<2;++header) {
        XL6_Output out;size_t count=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,m,"BAD_SCALAR",NULL):xl6IrCgenEmitC(&out,m,NULL)));
        char *text=xl6OutputTakeMemoryText(&out,&count);assert(text && !count);
        xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char **argv)
{
    assert(argc==2);
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule *m=&unit.ir;assert(xl6IrVerifyModule(m,NULL));
    unsigned f32=0,native=0,arrays=0,compound=0;
    for(size_t i=0;i<m->instructionCount;++i) {
        XL6_IrInstruction *ins=&m->instructions[i];
        const XL6_IrType *type=xl6IrTypeGet(&m->types,ins->type);
        if(ins->kind==XL6_IR_INSTR_BINARY && type &&
           (type->cAbi==XL6_IR_C_ABI_F32 || type->cAbi==XL6_IR_C_ABI_C_FLOAT ||
            type->cAbi==XL6_IR_C_ABI_C_DOUBLE)) {
            if(type->cAbi==XL6_IR_C_ABI_F32)++f32;else ++native;
            XL6_IrOpcode op=ins->opcode;ins->opcode=XL6_IR_OPCODE_BIT_AND;refused(m);ins->opcode=op;
        }
        if(ins->kind==XL6_IR_INSTR_C_ARRAY_SET) {
            assert(ins->operandCount==4);++arrays;
            XL6_IrValueId *proof=&m->operands[ins->firstOperand+3],saved=*proof;
            *proof=m->operands[ins->firstOperand+1];refused(m);*proof=saved;
        }
    }
    for(size_t i=0;i<m->nodeCount;++i) {
        XL6_IrNode *n=&m->nodes[i];
        if(n->kind==XL6_IR_NODE_ASSIGN && n->opcode!=XL6_IR_OPCODE_INVALID &&
           n->indexWriteErrorAbi==XL6_IR_ERROR_ABI_MAY_RAISE) {
            XL6_IrErrorAbi saved=n->indexWriteErrorAbi;n->indexWriteErrorAbi=XL6_IR_ERROR_ABI_NO_RAISE;
            refused(m);n->indexWriteErrorAbi=saved;++compound;
        }
    }
    assert(f32>=4 && native>=8 && arrays>=4 && compound>=3);
    assert(xl6IrVerifyModule(m,NULL));
    printf("scalar numeric sealed IR: %u mutations, %u empty C/header rejections\n",mutations,mutations*2);
    xl6CompileUnitFree(&unit);return 0;
}
