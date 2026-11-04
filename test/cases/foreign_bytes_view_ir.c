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
        assert(!(header ? xl6IrCgenEmitHeader(&out,module,"BAD_BYTES",NULL) : xl6IrCgenEmitC(&out,module,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(text && !bytes);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));unsigned inputs=0,results=0;
    for(size_t i=0;i<module->callTargetCount;++i){
        XL6_IrCallTarget* target=&module->callTargets[i];
        XL6_IrNode* node=target->declaration ? &module->nodes[target->declaration-1] : NULL;
        for(size_t p=0;p<target->parameterCount;++p){
            if(target->parameterPhysicalAbis[p]!=XL6_IR_CALL_PARAMETER_PHYSICAL_BYTES_VIEW_BORROW)continue;
            ++inputs;assert(node);XL6_ForeignCallFacts facts=node->foreignCall;
            target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refused(module);
            target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_BYTES_VIEW_BORROW;
            node->foreignCall.bytesViews=0;refused(module);node->foreignCall=facts;
            node->foreignCall.constAggregateAddresses|=facts.bytesViews;refused(module);node->foreignCall=facts;
            node->foreignCall.textViews|=facts.bytesViews;refused(module);node->foreignCall=facts;
            node->foreignCall.bytesViews|=UINT64_C(1)<<63;refused(module);node->foreignCall=facts;
            int constraint=target->parameterConstraintAbis[p];
            target->parameterConstraintAbis[p]=1;refused(module);target->parameterConstraintAbis[p]=constraint;
            XL6_IrParameterAbi abi=target->parameterAbis[p];target->parameterAbis[p]=XL6_IR_PARAMETER_ABI_OWNED;refused(module);target->parameterAbis[p]=abi;
        }
        if(target->resultCAbi!=XL6_IR_CALL_RESULT_C_ABI_BYTES_VIEW)continue;
        ++results;assert(node && target->errorAbi==XL6_IR_ERROR_ABI_MAY_RAISE);
        XL6_IrCallTarget saved=*target;XL6_ForeignCallFacts facts=node->foreignCall;
        target->resultCAbi=XL6_IR_CALL_RESULT_C_ABI_CHAR_POINTER;refused(module);*target=saved;
        target->resultCAbi=XL6_IR_CALL_RESULT_C_ABI_TEXT_VIEW;refused(module);*target=saved;
        target->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;refused(module);*target=saved;
        target->resultConversionAbi=XL6_IR_CALL_RESULT_CONVERSION_DIRECT;refused(module);*target=saved;
        node->foreignCall.borrowedViewResult=0;refused(module);node->foreignCall=facts;
        node->foreignCall.borrowedViewResult=XL6_FOREIGN_VIEW_RESULT_TEXT;refused(module);node->foreignCall=facts;
        node->foreignCall.borrowedViewResult=(XL6_ForeignBorrowedViewResult)3;refused(module);node->foreignCall=facts;
    }
    assert(inputs>=2 && results>=3 && xl6IrVerifyModule(module,NULL));
    printf("bytes view: inputs=%u results=%u mutations=%u zero-output=%u\n",inputs,results,mutations,mutations*2);
    xl6CompileUnitFree(&unit);return 0;
}
