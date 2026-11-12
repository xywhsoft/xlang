#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* m)
{
    assert(!xl6IrVerifyModule(m,NULL));
    for(unsigned header=0;header<2;++header){
        XL6_Output out;size_t count=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,m,"BAD_OWNED_BYTES",NULL):xl6IrCgenEmitC(&out,m,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&count);assert(text && !count);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2 && sizeof(XL6_ForeignCallFacts)==88 && sizeof(XL6_ForeignOwnedOutputContract)==28);
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* m=&unit.ir;assert(xl6IrVerifyModule(m,NULL));
    size_t count=m->foreignOwnedOutputsCount,capacity=m->foreignOwnedOutputsCapacity;assert(count==5);
    XL6_ForeignOwnedOutputContract* rows=m->foreignOwnedOutputs;
    m->foreignOwnedOutputsCount=capacity+1;refused(m);m->foreignOwnedOutputsCount=count;
    m->foreignOwnedOutputsCapacity=count-1;refused(m);m->foreignOwnedOutputsCapacity=capacity;
    m->foreignOwnedOutputs=NULL;refused(m);m->foreignOwnedOutputs=rows;
    m->foreignOwnedOutputsCount=0;refused(m);m->foreignOwnedOutputsCount=count;
    for(size_t i=0;i<count;++i){
        XL6_ForeignOwnedOutputContract saved=rows[i];XL6_IrNode* n=&m->nodes[saved.declaration-1];
        XL6_ForeignCallFacts facts=n->foreignCall;uint32 flags=n->flags;
        rows[i].declaration=0;refused(m);rows[i]=saved;
        rows[i].dataIndex=64;refused(m);rows[i]=saved;
        rows[i].sizeIndex=saved.dataIndex;refused(m);rows[i]=saved;
        rows[i].sizeIndex=64;refused(m);rows[i]=saved;
        rows[i].pointerAbi=0;refused(m);rows[i]=saved;
        rows[i].pointerAbi=99;refused(m);rows[i]=saved;
        rows[i].sizeAbi=0;refused(m);rows[i]=saved;
        rows[i].sizeAbi=99;refused(m);rows[i]=saved;
        rows[i].successCode=saved.successCode^1;refused(m);rows[i]=saved;
        rows[i].kind=0;refused(m);rows[i]=saved;
        rows[i].kind=99;refused(m);rows[i]=saved;
        rows[i].kind=saved.kind==XL6_FOREIGN_OWNED_OUTPUT_TEXT ? XL6_FOREIGN_OWNED_OUTPUT_BYTES:XL6_FOREIGN_OWNED_OUTPUT_TEXT;refused(m);rows[i]=saved;
        if(saved.kind==XL6_FOREIGN_OWNED_OUTPUT_TEXT){rows[i].pointerAbi=XL6_FOREIGN_OWNED_OUTPUT_VOID;refused(m);rows[i]=saved;}
        n->foreignCall.valueOutput=0;refused(m);n->foreignCall=facts;
        n->foreignCall.valueOutput|=XL6_FOREIGN_VALUE_OUTPUT_VIEW_CONVERTED;refused(m);n->foreignCall=facts;
        n->foreignCall.outputIndexPlusOne=0;refused(m);n->foreignCall=facts;
        n->flags|=XL6_AST_FLAG_FOREIGN_PURE;refused(m);n->flags=flags;
        n->flags&=~XL6_AST_FLAG_FOREIGN_NO_RAISE;refused(m);n->flags=flags;
    }
    XL6_ForeignOwnedOutputContract saved=rows[1];rows[1]=rows[0];refused(m);rows[1]=saved;
    unsigned targets=0;
    for(size_t i=0;i<m->callTargetCount;++i){
        XL6_IrCallTarget* t=&m->callTargets[i];
        if(!xl6IrForeignOwnedOutputGet(m,t))continue;
        ++targets;XL6_IrCallTarget original=*t;
        t->returnType=0;refused(m);*t=original;
        t->resultCAbi=XL6_IR_CALL_RESULT_C_ABI_INT_STATUS;refused(m);*t=original;
        t->resultConversionAbi=XL6_IR_CALL_RESULT_CONVERSION_DIRECT;refused(m);*t=original;
        t->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;refused(m);*t=original;
        t->nativeSuccessCode^=1;refused(m);*t=original;
    }
    assert(targets==5);
    assert(xl6IrVerifyModule(m,NULL));
    printf("owned UTF-8 sealed IR: %u mutations, %u empty C/header rejections\n",mutations,mutations*2);
    xl6CompileUnitFree(&unit);return 0;
}
