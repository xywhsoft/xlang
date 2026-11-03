#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;++header){XL6_Output out;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header?xl6IrCgenEmitHeader(&out,module,"BAD_INSTANCE_FACTORY",NULL):xl6IrCgenEmitC(&out,module,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(text && !bytes);xrtFree(text);xl6OutputDiscard(&out);}
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));unsigned factories=0;
    for(size_t i=0;i<module->callTargetCount;++i){XL6_IrCallTarget* target=&module->callTargets[i];
        XL6_IrNode* node=target->declaration ? &module->nodes[target->declaration-1]:NULL;
        if(!node || node->sourceKind!=XL6_AST_NATIVE_EXTERN_FUNC || !node->foreignCall.outputIndexPlusOne)continue;
        ++factories;XL6_IrCallTarget saved=*target;XL6_ForeignCallFacts facts=node->foreignCall;uint32_t flags=node->flags;
        node->foreignCall.outputIndexPlusOne=4;refused(module);node->foreignCall=facts;
        node->foreignCall.resultOwnerIndexPlusOne=2;refused(module);node->foreignCall=facts;
        node->foreignCall.rollbackSymbol=target->symbol;refused(module);node->foreignCall=facts;
        node->foreignCall.rollbackSymbol=0;refused(module);node->foreignCall=facts;
        node->flags|=XL6_AST_FLAG_STATIC;refused(module);node->flags=flags;
        node->flags|=XL6_AST_FLAG_FOREIGN_PURE;refused(module);node->flags=flags;
        XL6_IrCallParameterPhysicalAbi receiver=target->parameterPhysicalAbis[0];
        target->parameterPhysicalAbis[0]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refused(module);target->parameterPhysicalAbis[0]=receiver;
        target->resultConversionAbi=XL6_IR_CALL_RESULT_CONVERSION_DIRECT;refused(module);*target=saved;
    }
    assert(factories && xl6IrVerifyModule(module,NULL));
    printf("instance factories: targets=%u mutations=%u zero-output=%u\n",factories,mutations,mutations*2);
    xl6CompileUnitFree(&unit);return 0;
}
