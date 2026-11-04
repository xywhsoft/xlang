#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned mutations;
static void refused(XL6_IrModule* ir,const char* label)
{
    if(xl6IrVerifyModule(ir,NULL)){fprintf(stderr,"unexpected valid output-size IR: %s\n",label);assert(0);}
    for(unsigned header=0;header<2;++header){
        XL6_Output out;size_t count=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_OUTPUT_SIZE",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&count);assert(text && !count);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);assert(sizeof(XL6_ForeignCallFacts)==88);
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));
    size_t count=ir->foreignOutputSizeCount,capacity=ir->foreignOutputSizeCapacity;assert(count>=6);
    XL6_ForeignOutputSizeContract* rows=ir->foreignOutputSizes;
    ir->foreignOutputSizeCount=capacity+1;refused(ir,"count/capacity");ir->foreignOutputSizeCount=count;
    ir->foreignOutputSizeCapacity=count-1;refused(ir,"capacity");ir->foreignOutputSizeCapacity=capacity;
    ir->foreignOutputSizes=NULL;refused(ir,"missing rows");ir->foreignOutputSizes=rows;
    ir->foreignOutputSizeCount=0;refused(ir,"lost presence rows");ir->foreignOutputSizeCount=count;
    XL6_ForeignOutputSizeContract first=rows[0],second=rows[1];
    rows[0].declaration=0;refused(ir,"declaration");rows[0]=first;
    rows[0].physicalIndex=63;refused(ir,"output slot");rows[0]=first;
    rows[0].ordinal=63;refused(ir,"ordinal");rows[0]=first;
    rows[0].recordType=0;refused(ir,"record type");rows[0]=first;
    rows[0].fieldDeclaration=0;refused(ir,"field declaration");rows[0]=first;
    rows[0].fieldType=0;refused(ir,"field type");rows[0]=first;
    rows[0].field[0]=0;refused(ir,"empty member");rows[0]=first;
    strcpy(rows[0].field,"size;bad");refused(ir,"member injection");rows[0]=first;
    strcpy(rows[0].field,"value");refused(ir,"signed member");rows[0]=first;
    memset(rows[0].field,'x',sizeof(rows[0].field));refused(ir,"unterminated member");rows[0]=first;
    rows[1]=rows[0];refused(ir,"duplicate output");rows[1]=second;
    XL6_IrNode* declaration=&ir->nodes[first.declaration-1];uint8 saved=declaration->foreignCall.valueOutput;
    declaration->foreignCall.valueOutput&=(uint8)~XL6_FOREIGN_VALUE_OUTPUT_SIZE_INITIALIZED;
    refused(ir,"lost presence flag");declaration->foreignCall.valueOutput=saved;
    declaration->foreignCall.valueOutput|=8u;refused(ir,"unknown low flag");declaration->foreignCall.valueOutput=saved;
    declaration->foreignCall.valueOutput|=32u;refused(ir,"unknown high flag");declaration->foreignCall.valueOutput=saved;
    declaration->foreignCall.valueOutput=XL6_FOREIGN_VALUE_OUTPUT_SIZE_INITIALIZED;
    refused(ir,"presence without kind");declaration->foreignCall.valueOutput=saved;
    for(size_t i=0;i<count;++i){
        XL6_ForeignOutputSizeContract row=rows[i];
        if(row.physicalIndex==4){rows[i].physicalIndex=3;refused(ir,"split pointer count");rows[i]=row;break;}
    }
    unsigned targets=0;
    for(size_t t=0;t<ir->callTargetCount;++t){
        XL6_IrCallTarget* target=&ir->callTargets[t];
        if(!xl6IrForeignOutputSizeGet(ir,target,0))continue;
        XL6_IrTypeId type=target->returnType;target->returnType=0;refused(ir,"target output type");target->returnType=type;++targets;
    }
    assert(targets>=2 && xl6IrVerifyModule(ir,NULL));
    printf("output-size: %u targets, %u mutations refused before C/header output; facts stay 88 bytes\n",targets,mutations);
    xl6CompileUnitFree(&unit);return 0;
}
