#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;++header){
        XL6_Output out;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,module,"REJECT_RETURN_OUTPUT",NULL):xl6IrCgenEmitC(&out,module,NULL)));
        char* data=xl6OutputTakeMemoryText(&out,&bytes);assert(data && !bytes);
        xrtFree(data);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));
    unsigned targets=0,rows=0;
    for(size_t i=0;i<module->callTargetCount;++i){
        XL6_IrCallTarget* target=&module->callTargets[i];
        XL6_IrNode* node=target->declaration ? &module->nodes[target->declaration-1]:NULL;
        if(!node || !xl6ForeignValueOutputHasReturn(node->foreignCall.valueOutput))continue;
        ++targets;XL6_IrCallTarget saved=*target;XL6_ForeignCallFacts facts=node->foreignCall;
        target->outputResultAbi=XL6_IR_CALL_OUTPUT_RESULT_INT_STATUS;refused(module);*target=saved;
        target->outputResultAbi=XL6_IR_CALL_OUTPUT_RESULT_VOID;refused(module);*target=saved;
        target->nativeSuccessCode=target->nativeSuccessCode==0 ? 1:0;refused(module);*target=saved;
        target->resultConversionAbi=XL6_IR_CALL_RESULT_CONVERSION_DIRECT;refused(module);*target=saved;
        target->returnType=0;refused(module);*target=saved;
        node->foreignCall.borrowedViewResult=facts.borrowedViewResult==XL6_FOREIGN_VIEW_RESULT_INTEGER_MIN ?
            XL6_FOREIGN_VIEW_RESULT_NONE:XL6_FOREIGN_VIEW_RESULT_INTEGER_MIN;
        refused(module);node->foreignCall=facts;
        node->foreignCall.valueOutput=XL6_FOREIGN_VALUE_OUTPUT_MULTI_STATUS;refused(module);node->foreignCall=facts;
        node->foreignCall.valueOutput|=32;refused(module);node->foreignCall=facts;
        node->foreignCall.outputIndexPlusOne=0;refused(module);node->foreignCall=facts;
    }
    for(size_t i=0;i<module->foreignOutputCount;++i){
        XL6_ForeignOutputContract* row=&module->foreignOutputs[i];
        const XL6_IrNode* node=&module->nodes[row->declaration-1];
        if(!xl6ForeignValueOutputHasReturn(node->foreignCall.valueOutput))continue;
        ++rows;XL6_ForeignOutputContract saved=*row;
        row->physicalIndex=row->ordinal ? UINT32_MAX:0;refused(module);*row=saved;
        row->physicalIndex=UINT32_MAX-1u;refused(module);*row=saved;
        row->ordinal=row->ordinal ? 0:1;refused(module);*row=saved;
    }
    assert(targets>=7 && rows>=16 && xl6IrVerifyModule(module,NULL));
    printf("FOREIGN_RETURN_OUTPUT_IR_OK targets=%u rows=%u mutations=%u zeroOutput=%u\n",
        targets,rows,mutations,mutations*2);
    xl6CompileUnitFree(&unit);return 0;
}
