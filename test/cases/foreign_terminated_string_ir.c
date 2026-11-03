#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* m)
{
    assert(!xl6IrVerifyModule(m,NULL));
    for(unsigned h=0;h<2;++h) {
        XL6_Output out;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(h ? xl6IrCgenEmitHeader(&out,m,"BAD_CSTR",NULL) : xl6IrCgenEmitC(&out,m,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(text && !bytes);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* m=&unit.ir;unsigned strings=0;assert(xl6IrVerifyModule(m,NULL));
    for(size_t i=0;i<m->callTargetCount;++i) {
        XL6_IrCallTarget* target=&m->callTargets[i];XL6_IrCallTarget saved=*target;
        for(size_t p=0;p<target->parameterCount;++p) {
            if(target->parameterPhysicalAbis[p]!=XL6_IR_CALL_PARAMETER_PHYSICAL_TERMINATED_STRING_BORROW)continue;
            ++strings;target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refused(m);
            target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_TERMINATED_STRING_BORROW;
            target->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;refused(m);*target=saved;
            target->errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_NONE;refused(m);*target=saved;
            XL6_IrParameterAbi abi=target->parameterAbis[p];target->parameterAbis[p]=XL6_IR_PARAMETER_ABI_VALUE;refused(m);target->parameterAbis[p]=abi;
            XL6_IrNode* node=&m->nodes[target->declaration-1];XL6_ForeignCallFacts facts=node->foreignCall;
            node->foreignCall.terminatedStrings=0;refused(m);node->foreignCall=facts;
        }
    }
    assert(strings>=2 && xl6IrVerifyModule(m,NULL));
    printf("terminated strings=%u mutations=%u zero-output=%u\n",strings,mutations,mutations*2);
    xl6CompileUnitFree(&unit);return 0;
}
