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
        assert(!(header ? xl6IrCgenEmitHeader(&out,m,"BAD_C_INTEGER",NULL):xl6IrCgenEmitC(&out,m,NULL)));
        char *text=xl6OutputTakeMemoryText(&out,&count);assert(text && !count);
        xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char **argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule *m=&unit.ir;assert(xl6IrVerifyModule(m,NULL));
    unsigned profiles[21]={0},checked=0,contextual=0;
    for(size_t i=0;i<m->instructionCount;++i) {
        XL6_IrInstruction *ins=&m->instructions[i];
        if(ins->kind==XL6_IR_INSTR_RUNTIME_CALL && xl6IrArithmeticIntegerTargetIsNative(ins->integerTarget)) {
            unsigned profile=(unsigned)ins->integerTarget-XL6_IR_INTEGER_TARGET_NATIVE_BASE;
            ++checked;
            if(profiles[profile]++)continue;
            XL6_IrIntegerTarget saved=ins->integerTarget;
            ins->integerTarget=XL6_IR_INTEGER_TARGET_SIGNED_64;refused(m);ins->integerTarget=saved;
            XL6_IrRuntimeOp op=ins->runtimeOp;
            ins->runtimeOp=op==XL6_IR_RUNTIME_INTEGER_ADD_CHECKED ? XL6_IR_RUNTIME_INTEGER_SUB_CHECKED:XL6_IR_RUNTIME_INTEGER_ADD_CHECKED;
            refused(m);ins->runtimeOp=op;
            XL6_IrInstruction raw=*ins;
            ins->kind=XL6_IR_INSTR_BINARY;ins->runtimeOp=XL6_IR_RUNTIME_NONE;ins->opcode=XL6_IR_OPCODE_ADD;ins->integerTarget=0;
            refused(m);*ins=raw;
        }
        if(ins->kind==XL6_IR_INSTR_RUNTIME_CALL && ins->runtimeOp==XL6_IR_RUNTIME_NATIVE_SCALAR_CONVERT_CHECKED && ins->contextualIntegerConversionSource) {
            XL6_IrNodeId source=ins->contextualIntegerConversionSource;
            ins->contextualIntegerConversionSource=0;refused(m);ins->contextualIntegerConversionSource=source;
            ins->contextualIntegerConversionSource=ins->origin;refused(m);ins->contextualIntegerConversionSource=source;
            ++contextual;
        }
    }
    unsigned domains=0;for(unsigned p=1;p<21;++p)domains+=profiles[p]!=0;
    assert(domains==17 && checked>400 && contextual>0);
    assert(xl6IrVerifyModule(m,NULL));
    printf("checked native integer IR: %u domains, %u mutations, %u empty C/header refusals\n",domains,mutations,mutations*2);
    xl6CompileUnitFree(&unit);return 0;
}
