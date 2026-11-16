#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>

static unsigned mutations;
static void refused(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module,NULL));
    for(int header=0;header<2;++header){
        XL6_Output out={0};size_t bytes=0;assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,module,"INTEGER_MIN_TEST",NULL):xl6IrCgenEmitC(&out,module,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(text && !bytes);
        xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}

int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* m=&unit.ir;assert(xl6IrVerifyModule(m,NULL));unsigned targets=0;
    for(size_t i=0;i<m->callTargetCount;++i){
        XL6_IrCallTarget* t=&m->callTargets[i];
        if(t->resultConversionAbi!=XL6_IR_CALL_RESULT_CONVERSION_INTEGER_MIN)continue;
        ++targets;XL6_IrCallTarget saved=*t;XL6_IrNode* n=&m->nodes[t->declaration-1];
        t->nativeSuccessCode^=1;refused(m);*t=saved;
        t->resultConversionAbi=XL6_IR_CALL_RESULT_CONVERSION_DIRECT;refused(m);*t=saved;
        t->resultCAbi=XL6_IR_CALL_RESULT_C_ABI_INT_STATUS;refused(m);*t=saved;
        t->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;refused(m);*t=saved;
        t->producerErrorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_XRT_THREAD_ERROR;refused(m);*t=saved;
        XL6_ForeignCallFacts facts=n->foreignCall;
        n->foreignCall.borrowedViewResult=XL6_FOREIGN_VIEW_RESULT_NONE;refused(m);n->foreignCall=facts;
        n->foreignCall.successCode^=1;refused(m);n->foreignCall=facts;
        uint32_t flags=n->flags;n->flags|=XL6_AST_FLAG_FOREIGN_FIELD;refused(m);n->flags=flags;
        XL6_IrType* result=&m->types.items[t->returnType-1];XL6_IrCAbi abi=result->cAbi;
        result->cAbi=XL6_IR_C_ABI_C_UINT;refused(m);result->cAbi=abi;
    }
    assert(targets>=6 && xl6IrVerifyModule(m,NULL));
    printf("integer minimum: %u targets, %u rejected mutations, %u empty outputs\n",targets,mutations,mutations*2);
    xl6CompileUnitFree(&unit);return 0;
}
