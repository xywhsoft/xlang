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
        assert(!(header ? xl6IrCgenEmitHeader(&out,module,"BAD_BORROWED_OUTPUT",NULL):
                         xl6IrCgenEmitC(&out,module,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&count);assert(text && !count);
        xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));unsigned outputs=0;
    for(size_t i=0;i<module->callTargetCount;++i){
        XL6_IrCallTarget* target=&module->callTargets[i];
        XL6_IrNode* node=target->declaration ? &module->nodes[target->declaration-1]:NULL;
        if(!node || !xl6ForeignBorrowedOwnerOutput(&node->foreignCall))continue;
        ++outputs;XL6_IrCallTarget saved=*target;XL6_ForeignCallFacts facts=node->foreignCall;
        uint32 flags=node->flags;
        target->returnType=0;refused(module);*target=saved;
        target->resultConversionAbi=XL6_IR_CALL_RESULT_CONVERSION_DIRECT;refused(module);*target=saved;
        target->resultConversionAbi=XL6_IR_CALL_RESULT_CONVERSION_NATIVE_OUTPUT_VALUE;refused(module);*target=saved;
        target->resultCAbi=XL6_IR_CALL_RESULT_C_ABI_STORAGE;refused(module);*target=saved;
        target->outputResultAbi=XL6_IR_CALL_OUTPUT_RESULT_NONE;refused(module);*target=saved;
        node->flags&=~XL6_AST_FLAG_FOREIGN_NO_RAISE;refused(module);node->flags=flags;
        node->flags|=XL6_AST_FLAG_FOREIGN_PURE;refused(module);node->flags=flags;
        node->foreignCall.outputIndexPlusOne=0;refused(module);node->foreignCall=facts;
        node->foreignCall.outputIndexPlusOne=65;refused(module);node->foreignCall=facts;
        node->foreignCall.valueOutput=0;refused(module);node->foreignCall=facts;
        node->foreignCall.borrowedViewResult=0;refused(module);node->foreignCall=facts;
        node->foreignCall.borrowedViewResult=XL6_FOREIGN_VIEW_RESULT_OWNED_NATIVE;refused(module);node->foreignCall=facts;
        node->foreignCall.borrowedViewResult=XL6_FOREIGN_VIEW_RESULT_SCOPE_NATIVE;refused(module);node->foreignCall=facts;
        node->foreignCall.resultOwnerIndexPlusOne=0;refused(module);node->foreignCall=facts;
        node->foreignCall.resultOwnerIndexPlusOne=65;refused(module);node->foreignCall=facts;
        node->foreignCall.rollbackSymbol=1;refused(module);node->foreignCall=facts;
        node->foreignCall.receiverDereference=1;refused(module);node->foreignCall=facts;
        node->foreignCall.inplaceOutput=1;refused(module);node->foreignCall=facts;
        if(xl6ForeignValueOutputIsVoid(facts.valueOutput)){
            node->foreignCall.successCode=1;refused(module);node->foreignCall=facts;
        }
    }
    assert(outputs>=8 && xl6IrVerifyModule(module,NULL));
    printf("Borrowed outputs: %u targets, %u mutations rejected before C/header bytes\n",outputs,mutations);
    xl6CompileUnitFree(&unit);return 0;
}
