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
        assert(!(header ? xl6IrCgenEmitHeader(&out,module,"BAD_SCOPE_VIEW",NULL) : xl6IrCgenEmitC(&out,module,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(text && !bytes);
        xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));
    unsigned outputs=0;
    for(size_t i=0;i<module->callTargetCount;++i){
        XL6_IrCallTarget* target=&module->callTargets[i];
        XL6_IrNode* node=target->declaration ? &module->nodes[target->declaration-1] : NULL;
        if(!node || !node->foreignCall.valueOutput)continue;
        ++outputs;XL6_IrCallTarget saved=*target;XL6_ForeignCallFacts facts=node->foreignCall;uint32_t flags=node->flags;
        target->resultCAbi=XL6_IR_CALL_RESULT_C_ABI_INT_STATUS;refused(module);*target=saved;
        target->resultConversionAbi=XL6_IR_CALL_RESULT_CONVERSION_DIRECT;refused(module);*target=saved;
        target->returnType=0;refused(module);*target=saved;
        target->producerErrorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_XRT_THREAD_ERROR;refused(module);*target=saved;
        target->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;refused(module);*target=saved;
        node->foreignCall.valueOutput=0;refused(module);node->foreignCall=facts;
        node->foreignCall.valueOutput=2;refused(module);node->foreignCall=facts;
        node->foreignCall.outputIndexPlusOne=0;refused(module);node->foreignCall=facts;
        node->foreignCall.outputIndexPlusOne=64;refused(module);node->foreignCall=facts;
        node->foreignCall.rollbackSymbol=target->symbol;refused(module);node->foreignCall=facts;
        node->flags&=~XL6_AST_FLAG_FOREIGN_NO_RAISE;refused(module);node->flags=flags;
        if(node->foreignCall.physicalPairs){node->foreignCall.outputIndexPlusOne=node->sourceKind==XL6_AST_NATIVE_EXTERN_FUNC ? 3u : 2u;refused(module);node->foreignCall=facts;}
    }
    assert(outputs>=5 && xl6IrVerifyModule(module,NULL));
    printf("status/output values: outputs=%u mutations=%u zero-output=%u\n",outputs,mutations,mutations*2);
    xl6CompileUnitFree(&unit);return 0;
}
