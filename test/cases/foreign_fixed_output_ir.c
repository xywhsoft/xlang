#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;++header){
        XL6_Output out;size_t count=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,module,"BAD_FIXED_OUTPUT",NULL):xl6IrCgenEmitC(&out,module,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&count);assert(text && !count);
        xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));unsigned arrays=0,outputs=0;
    for(size_t i=0;i<module->types.count;++i){
        XL6_IrType* type=&module->types.items[i];if(type->kind!=XL6_IR_TYPE_C_ARRAY)continue;
        ++arrays;XL6_IrType saved=*type;
        type->nativeArrayExtent=0;refused(module);*type=saved;
        type->nativeArrayExtent=UINT64_C(4294967296);refused(module);*type=saved;
        type->nativeArrayExtent++;refused(module);*type=saved;
        type->paramCount=0;refused(module);*type=saved;
        type->params=NULL;refused(module);*type=saved;
        type->cAbi=XL6_IR_C_ABI_RAW_POINTER;refused(module);*type=saved;
        type->ownershipAbi=XL6_IR_OWNERSHIP_ABI_MANAGED;refused(module);*type=saved;
        type->nullAbi=XL6_IR_NULL_ABI_POINTER_SENTINEL;refused(module);*type=saved;
    }
    for(size_t i=0;i<module->callTargetCount;++i){
        XL6_IrCallTarget* target=&module->callTargets[i];
        XL6_IrNode* node=target->declaration ? &module->nodes[target->declaration-1]:NULL;
        if(!node || !node->foreignCall.valueOutput)continue;
        ++outputs;XL6_IrCallTarget saved=*target;XL6_ForeignCallFacts facts=node->foreignCall;
        target->returnType=0;refused(module);*target=saved;
        target->resultConversionAbi=XL6_IR_CALL_RESULT_CONVERSION_DIRECT;refused(module);*target=saved;
        target->outputResultAbi=XL6_IR_CALL_OUTPUT_RESULT_NONE;refused(module);*target=saved;
        node->foreignCall.outputIndexPlusOne=0;refused(module);node->foreignCall=facts;
        node->foreignCall.valueOutput=0;refused(module);node->foreignCall=facts;
    }
    assert(arrays>=3 && outputs>=6 && xl6IrVerifyModule(module,NULL));
    printf("Fixed outputs: %u array types, %u outputs, %u mutations rejected before C/header bytes\n",arrays,outputs,mutations);
    xl6CompileUnitFree(&unit);return 0;
}
