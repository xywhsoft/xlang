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
        assert(!(header ? xl6IrCgenEmitHeader(&out,module,"BAD_VOID_FACTORY",NULL) : xl6IrCgenEmitC(&out,module,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(text && !bytes);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));unsigned factories=0;
    for(size_t i=0;i<module->foreignStorageCount;++i){
        const XL6_ForeignStorageContract* row=&module->foreignStorage[i];
        XL6_IrNode* node=&module->nodes[row->declaration-1];XL6_ForeignCallFacts facts=node->foreignCall;
        XL6_IrCallTarget* target=(XL6_IrCallTarget*)xl6IrCallTargetGet(module,node->symbol);
        assert(target && facts.inplaceOutput==XL6_FOREIGN_INPLACE_VOID && target->outputResultAbi==XL6_IR_CALL_OUTPUT_RESULT_VOID);
        ++factories;
        target->outputResultAbi=XL6_IR_CALL_OUTPUT_RESULT_NONE;refused(module);
        target->outputResultAbi=XL6_IR_CALL_OUTPUT_RESULT_INT_STATUS;refused(module);
        target->outputResultAbi=99;refused(module);target->outputResultAbi=XL6_IR_CALL_OUTPUT_RESULT_VOID;
        node->foreignCall.inplaceOutput=XL6_FOREIGN_INPLACE_STATUS;refused(module);node->foreignCall=facts;
        node->foreignCall.inplaceOutput=XL6_FOREIGN_INPLACE_NONE;refused(module);node->foreignCall=facts;
        node->foreignCall.inplaceOutput=99;refused(module);node->foreignCall=facts;
        node->foreignCall.successCode=1;refused(module);node->foreignCall=facts;
        node->foreignCall.outputIndexPlusOne=0;refused(module);node->foreignCall=facts;
        node->foreignCall.valueOutput=1;refused(module);node->foreignCall=facts;
        uint32 flags=node->flags;node->flags|=XL6_AST_FLAG_FOREIGN_PURE;refused(module);node->flags=flags;
        node->flags&=~XL6_AST_FLAG_FOREIGN_NO_RAISE;refused(module);node->flags=flags;
    }
    assert(factories==3 && sizeof(XL6_ForeignCallFacts)==88 && xl6IrVerifyModule(module,NULL));
    printf("void factory: factories=%u mutations=%u zero-output=%u\n",factories,mutations,mutations*2);
    xl6CompileUnitFree(&unit);return 0;
}
