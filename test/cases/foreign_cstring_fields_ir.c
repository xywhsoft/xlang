#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* module,const char* label)
{
    if(xl6IrVerifyModule(module,NULL)){
        fprintf(stderr,"unexpected valid CString field IR: %s\n",label);assert(0);
    }
    for(unsigned header=0;header<2;++header){
        XL6_Output out;size_t size=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,module,"BAD_FIELD",NULL) : xl6IrCgenEmitC(&out,module,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&size);assert(text && !size);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));unsigned fields=0;
    for(size_t i=0;i<module->callTargetCount;++i){
        XL6_IrCallTarget* target=&module->callTargets[i];
        if(target->resultCAbi!=XL6_IR_CALL_RESULT_C_ABI_CONST_CHAR_POINTER)continue;
        XL6_IrNode* node=&module->nodes[target->declaration-1];
        if(!(node->flags&XL6_AST_FLAG_FOREIGN_FIELD))continue;
        ++fields;XL6_IrCallTarget saved=*target;XL6_IrNode source=*node;
        assert(node->sourceKind==XL6_AST_GET && target->foreignOperationAbi==XL6_IR_FOREIGN_OPERATION_FIELD_READ);
        assert(target->parameterCount==1 && target->errorAbi==XL6_IR_ERROR_ABI_MAY_RAISE);
        target->resultCAbi=XL6_IR_CALL_RESULT_C_ABI_CHAR_POINTER;refused(module,"mutable raw result");*target=saved;
        target->resultCAbi=XL6_IR_CALL_RESULT_C_ABI_TEXT_VIEW;refused(module,"counted text ABI");*target=saved;
        target->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;refused(module,"copy cannot fail");*target=saved;
        target->resultConversionAbi=XL6_IR_CALL_RESULT_CONVERSION_DIRECT;refused(module,"skip string copy");*target=saved;
        target->foreignOperationAbi=XL6_IR_FOREIGN_OPERATION_FIELD_WRITE;refused(module,"field write");*target=saved;
        target->parameterCount=0;refused(module,"missing receiver");*target=saved;
        target->returnType=0;refused(module,"missing result type");*target=saved;
        node->foreignCall.borrowedViewResult=0;refused(module,"missing snapshot fact");*node=source;
        node->foreignCall.outputIndexPlusOne=1;refused(module,"output pointer on field");*node=source;
        node->foreignCall.receiverDereference=1;refused(module,"dereference field");*node=source;
        node->sourceKind=XL6_AST_FUNC;refused(module,"function marked as field");*node=source;
        node->flags|=XL6_AST_FLAG_STATIC;refused(module,"static field");*node=source;
        node->flags|=XL6_AST_FLAG_FOREIGN_OWNED_STRING;refused(module,"borrowed field transfers ownership");*node=source;
        node->flags&=~XL6_AST_FLAG_FOREIGN_FIELD;refused(module,"field operation loses declaration fact");*node=source;
    }
    assert(fields==6 && xl6IrVerifyModule(module,NULL));
    printf("CString fields: %u getters, %u rejected mutations, %u empty outputs\n",fields,mutations,mutations*2);
    xl6CompileUnitFree(&unit);return 0;
}
