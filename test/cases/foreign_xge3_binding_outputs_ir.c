#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned mutations;
static void refused(XL6_IrModule* ir,const char* label)
{
    if(xl6IrVerifyModule(ir,NULL)){fprintf(stderr,"unexpected valid IR: %s\n",label);assert(0);}
    for(unsigned header=0;header<2;++header){
        XL6_Output out;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_NATIVE_OUTPUTS",NULL) : xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(text && bytes==0);
        xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));assert(ir->foreignOutputCount>=10);
    size_t count=ir->foreignOutputCount,capacity=ir->foreignOutputCapacity;
    XL6_ForeignOutputContract* rows=ir->foreignOutputs;
    ir->foreignOutputCount=capacity+1;refused(ir,"row count beyond capacity");ir->foreignOutputCount=count;
    ir->foreignOutputCapacity=count-1;refused(ir,"row capacity");ir->foreignOutputCapacity=capacity;
    ir->foreignOutputs=NULL;refused(ir,"missing row storage");ir->foreignOutputs=rows;
    unsigned targets=0;
    for(size_t t=0;t<ir->callTargetCount;++t){
        XL6_IrCallTarget* target=&ir->callTargets[t];
        if(!target->cName || (strcmp(target->cName,"xuiWidgetGetGridMetrics") && strcmp(target->cName,"xgeMouseGet") && strcmp(target->cName,"binding_span_outputs")))continue;
        XL6_IrCallTarget saved=*target;XL6_IrNode* node=&ir->nodes[target->declaration-1];XL6_IrNode nodeSaved=*node;
        assert(node->foreignCall.valueOutput==XL6_FOREIGN_VALUE_OUTPUT_MULTI_STATUS || node->foreignCall.valueOutput==XL6_FOREIGN_VALUE_OUTPUT_MULTI_VOID);
        ++targets;
#define BAD_TARGET(F,V) do{target->F=(V);refused(ir,#F);*target=saved;}while(0)
        BAD_TARGET(outputResultAbi,XL6_IR_CALL_OUTPUT_RESULT_NONE);
        BAD_TARGET(returnType,0);BAD_TARGET(resultConversionAbi,XL6_IR_CALL_RESULT_CONVERSION_DIRECT);
        BAD_TARGET(resultPhysicalAbi,XL6_IR_CALL_RESULT_PHYSICAL_INVALID);
#undef BAD_TARGET
        node->foreignCall.valueOutput=255;refused(ir,"value output kind");*node=nodeSaved;
        node->foreignCall.outputIndexPlusOne+=1;refused(ir,"first output slot");*node=nodeSaved;
        node->foreignCall.successCode=33;refused(ir,"success status");*node=nodeSaved;
        target->nativeSuccessCode=33;refused(ir,"physical success status");*target=saved;
        if(saved.outputResultAbi==XL6_IR_CALL_OUTPUT_RESULT_INT_STATUS){
            node->foreignCall.successCode=33;target->nativeSuccessCode=33;
            assert(xl6IrVerifyModule(ir,NULL));*node=nodeSaved;*target=saved;
        }
        size_t first=SIZE_MAX,second=SIZE_MAX;
        for(size_t r=0;r<count;++r)if(rows[r].declaration==target->declaration){if(rows[r].ordinal==0)first=r;if(rows[r].ordinal==1)second=r;}
        assert(first!=SIZE_MAX && second!=SIZE_MAX);XL6_ForeignOutputContract rowSaved=rows[first];
        rows[first].declaration=0;refused(ir,"row declaration");rows[first]=rowSaved;
        rows[first].ordinal=64;refused(ir,"row ordinal bound");rows[first]=rowSaved;
        rows[first].physicalIndex=64;refused(ir,"row slot bound");rows[first]=rowSaved;
        rows[first].ordinal=rows[second].ordinal;refused(ir,"duplicate row ordinal");rows[first]=rowSaved;
        rows[first].physicalIndex=rows[second].physicalIndex;refused(ir,"duplicate row slot");rows[first]=rowSaved;
        rows[first].ordinal=7;refused(ir,"missing sequential ordinal");rows[first]=rowSaved;
        if(!strcmp(target->cName,"binding_span_outputs")){
            rows[first].physicalIndex=1;node->foreignCall.outputIndexPlusOne=2;
            refused(ir,"output splits native span pair");rows[first]=rowSaved;*node=nodeSaved;
        }
        assert(xl6IrVerifyModule(ir,NULL));
    }
    assert(targets==3);printf("native outputs: %u target families, %u mutations rejected before C/header output\n",targets,mutations);
    xl6CompileUnitFree(&unit);return 0;
}
