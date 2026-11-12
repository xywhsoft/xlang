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
        assert(!(header ? xl6IrCgenEmitHeader(&out,module,"BAD_CSTRING",NULL) : xl6IrCgenEmitC(&out,module,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(text && !bytes);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));unsigned results=0;
    for(size_t i=0;i<module->callTargetCount;++i){
        XL6_IrCallTarget* target=&module->callTargets[i];
        if(target->resultCAbi!=XL6_IR_CALL_RESULT_C_ABI_CONST_CHAR_POINTER)continue;
        ++results;XL6_IrNode* node=&module->nodes[target->declaration-1];
        XL6_IrCallTarget saved=*target;XL6_ForeignCallFacts facts=node->foreignCall;unsigned flags=node->flags;
        assert(target->errorAbi==XL6_IR_ERROR_ABI_MAY_RAISE);
        target->resultCAbi=XL6_IR_CALL_RESULT_C_ABI_CHAR_POINTER;refused(module);*target=saved;
        target->resultCAbi=XL6_IR_CALL_RESULT_C_ABI_TEXT_VIEW;refused(module);*target=saved;
        target->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;refused(module);*target=saved;
        target->resultConversionAbi=XL6_IR_CALL_RESULT_CONVERSION_DIRECT;refused(module);*target=saved;
        target->returnType=0;refused(module);*target=saved;
        node->foreignCall.borrowedViewResult=0;refused(module);node->foreignCall=facts;
        node->foreignCall.borrowedViewResult=XL6_FOREIGN_VIEW_RESULT_TEXT;refused(module);node->foreignCall=facts;
        node->foreignCall.outputIndexPlusOne=1;refused(module);node->foreignCall=facts;
        node->foreignCall.receiverDereference=1;refused(module);node->foreignCall=facts;
        node->flags|=XL6_AST_FLAG_FOREIGN_OWNED_STRING;refused(module);node->flags=flags;
        node->flags|=XL6_AST_FLAG_FOREIGN_FIELD;refused(module);node->flags=flags;
    }
    assert(results>=5 && xl6IrVerifyModule(module,NULL));
    printf("cstring: results=%u mutations=%u zero-output=%u\n",results,mutations,mutations*2);
    xl6CompileUnitFree(&unit);return 0;
}
