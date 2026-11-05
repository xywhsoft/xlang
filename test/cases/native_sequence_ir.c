#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>

static unsigned mutations;
static void refused(XL6_IrModule* m,const char* name)
{
    if(xl6IrVerifyModule(m,NULL)){fprintf(stderr,"accepted damaged NativeSequence IR: %s\n",name);assert(0);}
    for(unsigned header=0;header<2;++header){XL6_Output out;size_t count=SIZE_MAX;
        assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,m,"BAD_NATIVE_SEQUENCE",NULL):xl6IrCgenEmitC(&out,m,NULL)));
        char* value=xl6OutputTakeMemoryText(&out,&count);assert(value && !count);xrtFree(value);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));XL6_IrModule* m=&unit.ir;
    assert(xl6IrVerifyModule(m,NULL) && m->foreignNativeSequenceCount>=2);
    for(size_t n=0;n<m->foreignNativeSequenceCount;++n){
        XL6_ForeignNativeSequenceContract* row=&m->foreignNativeSequences[n],old=*row;
        row->declaration=0;refused(m,"missing declaration");*row=old;
        row->dataIndex=63;refused(m,"data physical slot");*row=old;
        row->countIndex=row->dataIndex;refused(m,"overlapping outputs");*row=old;
        row->ownerParameter=63;refused(m,"owning input");*row=old;
        row->sequenceType=0;refused(m,"sequence type");*row=old;
        row->elementType=0;refused(m,"POD element");*row=old;
        row->ownerType=0;refused(m,"independent owner type");*row=old;
        row->retainSymbol=0;refused(m,"Retain helper");*row=old;
        row->rollbackSymbol=0;refused(m,"release helper");*row=old;
        row->retainSuccess=0;refused(m,"Retain success ABI");*row=old;
        row->countAbi=0;refused(m,"count ABI");*row=old;
        ++row->successCode;refused(m,"success code");*row=old;
        strcpy(row->dataCName,"const void*");refused(m,"element C ABI");*row=old;
        memset(row->dataCName,'x',sizeof(row->dataCName));refused(m,"unterminated element ABI");*row=old;
    }
    size_t instructions=0,cursors=0;
    for(size_t n=0;n<m->instructionCount;++n){XL6_IrInstruction* i=&m->instructions[n],old=*i;
        if(i->kind!=XL6_IR_INSTR_RUNTIME_CALL || i->runtimeOp<XL6_IR_RUNTIME_NATIVE_SEQUENCE_CREATE || i->runtimeOp>XL6_IR_RUNTIME_NATIVE_SEQUENCE_ORDINAL)continue;
        ++instructions;
        i->type=0;refused(m,"runtime result type");*i=old;
        i->payloadType=old.type;refused(m,"unexpected payload");*i=old;
        i->runtimeIdentityType=old.type;refused(m,"unexpected identity");*i=old;
        i->targetSymbol=1;refused(m,"unexpected target");*i=old;
        ++i->operandCount;refused(m,"operand arity");*i=old;
        if(old.runtimeOp==XL6_IR_RUNTIME_NATIVE_SEQUENCE_READ_BEGIN)++cursors;
        if(old.runtimeOp==XL6_IR_RUNTIME_NATIVE_SEQUENCE_ITEM || old.runtimeOp==XL6_IR_RUNTIME_NATIVE_SEQUENCE_ORDINAL){
            XL6_IrInstruction* guard=NULL;
            for(size_t q=0;q<m->instructionCount;++q)if(m->instructions[q].runtimeOp==XL6_IR_RUNTIME_NATIVE_SEQUENCE_ADVANCE &&
                m->instructions[q].operandCount==1 && m->operands[m->instructions[q].firstOperand]==m->operands[old.firstOperand]){guard=&m->instructions[q];break;}
            assert(guard);XL6_IrInstruction saved=*guard;guard->runtimeOp=XL6_IR_RUNTIME_NONE;
            refused(m,"lost successful advance guard");*guard=saved;
        }
    }
    assert(instructions>=8 && cursors>=2);
    size_t count=m->foreignNativeSequenceCount;m->foreignNativeSequenceCount=m->foreignNativeSequenceCapacity+1;
    refused(m,"row storage overflow");m->foreignNativeSequenceCount=count;
    assert(xl6IrVerifyModule(m,NULL));xl6CompileUnitFree(&unit);
    printf("NativeSequence sealed producer/read IR: %u mutations, %zu operations, %zu read scopes\n",mutations,instructions,cursors);
    return 0;
}
