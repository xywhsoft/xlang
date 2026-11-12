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
    assert(argc==2 && sizeof(XL6_ForeignCallFacts)==88 && sizeof(XL6_ForeignOwnedBytesContract)==24);
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* m=&unit.ir;assert(xl6IrVerifyModule(m,NULL));
    size_t count=m->foreignOwnedBytesCount,capacity=m->foreignOwnedBytesCapacity;assert(count==5);
    XL6_ForeignOwnedBytesContract* rows=m->foreignOwnedBytes;
    m->foreignOwnedBytesCount=capacity+1;refused(m);m->foreignOwnedBytesCount=count;
    m->foreignOwnedBytesCapacity=count-1;refused(m);m->foreignOwnedBytesCapacity=capacity;
    m->foreignOwnedBytes=NULL;refused(m);m->foreignOwnedBytes=rows;
    m->foreignOwnedBytesCount=0;refused(m);m->foreignOwnedBytesCount=count;
    for(size_t i=0;i<count;++i){
        XL6_ForeignOwnedBytesContract saved=rows[i];XL6_IrNode* n=&m->nodes[saved.declaration-1];
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
        if(i>=3){rows[i].pointerAbi=XL6_FOREIGN_OWNED_BYTES_CHAR;refused(m);rows[i]=saved;}
        n->foreignCall.valueOutput=0;refused(m);n->foreignCall=facts;
        n->foreignCall.valueOutput|=XL6_FOREIGN_VALUE_OUTPUT_VIEW_CONVERTED;refused(m);n->foreignCall=facts;
        n->foreignCall.outputIndexPlusOne=0;refused(m);n->foreignCall=facts;
        n->flags|=XL6_AST_FLAG_FOREIGN_PURE;refused(m);n->flags=flags;
        n->flags&=~XL6_AST_FLAG_FOREIGN_NO_RAISE;refused(m);n->flags=flags;
    }
    XL6_ForeignOwnedBytesContract saved=rows[1];rows[1]=rows[0];refused(m);rows[1]=saved;
    unsigned targets=0;
    for(size_t i=0;i<m->callTargetCount;++i){
        XL6_IrCallTarget* t=&m->callTargets[i];
        if(!xl6IrForeignOwnedBytesGet(m,t))continue;
        ++targets;XL6_IrCallTarget original=*t;
        t->returnType=0;refused(m);*t=original;
        t->resultCAbi=XL6_IR_CALL_RESULT_C_ABI_INT_STATUS;refused(m);*t=original;
        t->resultConversionAbi=XL6_IR_CALL_RESULT_CONVERSION_DIRECT;refused(m);*t=original;
        t->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;refused(m);*t=original;
        t->nativeSuccessCode^=1;refused(m);*t=original;
    }
    assert(targets==5);
    assert(xl6IrVerifyModule(m,NULL));
    printf("owned bytes sealed IR: %u mutations, %u empty C/header rejections\n",mutations,mutations*2);
    xl6CompileUnitFree(&unit);return 0;
}
