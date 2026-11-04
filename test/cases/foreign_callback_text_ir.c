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
        assert(!(header ? xl6IrCgenEmitHeader(&out,module,"BAD_CALLBACK_TEXT",NULL) : xl6IrCgenEmitC(&out,module,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(text && !bytes);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));unsigned callbacks=0;
    for(size_t i=0;i<module->foreignParameterCount;++i){
        XL6_ForeignParameterContract* row=&module->foreignParameters[i];
        if(row->callbackResultKind!=XL6_FOREIGN_CALLBACK_RESULT_OWNED_TEXT)continue;
        XL6_ForeignParameterContract saved=*row;XL6_IrNode* node=&module->nodes[row->declaration-1];
        const XL6_IrCallTarget* target=xl6IrCallTargetGet(module,node->symbol);assert(target);++callbacks;
        const XL6_IrType* signature=xl6IrForeignCallbackSignature(module,target,row->parameter);
        assert(signature && xl6IrForeignCallbackParameterMatches(module,target,row->parameter));
        row->callbackResultKind=XL6_FOREIGN_CALLBACK_RESULT_TRIVIAL;refused(module);*row=saved;
        row->callbackResultKind=99;refused(module);*row=saved;
        row->callbackFailureCode=-1;refused(module);*row=saved;
        row->countAbi=XL6_FOREIGN_SPAN_COUNT_INT;refused(module);*row=saved;
        row->readOnly=true;refused(module);*row=saved;
        row->callbackOwnerIndexPlusOne=0;refused(module);*row=saved;
        row->callbackOwnerCName[0]='-';refused(module);*row=saved;
        row->callbackTypeCName[0]='-';refused(module);*row=saved;
        XL6_IrType* mutable=(XL6_IrType*)signature;XL6_IrType original=*mutable;
        mutable->returnCount=0;refused(module);*mutable=original;
        mutable->callableResultType=target->parameterTypes[0];refused(module);*mutable=original;
        XL6_IrType* result=(XL6_IrType*)xl6IrTypeGet(&module->types,signature->callableResultType);XL6_IrType resultOriginal=*result;
        result->cAbi=XL6_IR_C_ABI_RUNTIME_XVALUE_PTR;refused(module);*result=resultOriginal;
    }
    assert(callbacks==2 && sizeof(XL6_ForeignCallFacts)==88 && xl6IrVerifyModule(module,NULL));
    printf("owned callback text: callbacks=%u mutations=%u zero-output=%u\n",callbacks,mutations,mutations*2);
    xl6CompileUnitFree(&unit);return 0;
}
