#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;++header) {
        XL6_Output out;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,module,"BAD_EXPANSION",NULL) : xl6IrCgenEmitC(&out,module,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(text && !bytes);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* m=&unit.ir;assert(xl6IrVerifyModule(m,NULL));
    assert(m->foreignParameterCount>=6);
    for(size_t i=0;i<m->foreignParameterCount;++i) {
        XL6_ForeignParameterContract* row=&m->foreignParameters[i];XL6_ForeignParameterContract save=*row;
        row->declaration=0;refused(m);*row=save;
        row->parameter=63;refused(m);*row=save;
        row->kind=0;refused(m);*row=save;
        if(row->kind==XL6_FOREIGN_PARAMETER_SPAN) {
            row->countAbi=0;refused(m);*row=save;
            row->elementCName[0]='!';refused(m);*row=save;
            row->header[0]='"';refused(m);*row=save;
            row->readOnly=!row->readOnly;refused(m);*row=save;
        } else {
            row->callbackTypeCName[0]='!';refused(m);*row=save;
            row->callbackOwnerIndexPlusOne=64;refused(m);*row=save;
            row->callbackOwnerCName[0]='!';refused(m);*row=save;
        }
    }
    unsigned prepared=0;
    for(size_t i=0;i<m->callTargetCount;++i) {
        XL6_IrCallTarget* target=&m->callTargets[i];XL6_IrCallTarget save=*target;
        for(size_t p=0;p<target->parameterCount;++p) {
            XL6_IrCallParameterPhysicalAbi abi=target->parameterPhysicalAbis[p];
            if(abi!=XL6_IR_CALL_PARAMETER_PHYSICAL_FOREIGN_SPAN_BORROW &&
                abi!=XL6_IR_CALL_PARAMETER_PHYSICAL_RETAINED_C_CALLBACK &&
                abi!=XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_HANDLE_ACCESS_BORROW)continue;
            ++prepared;
            target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refused(m);target->parameterPhysicalAbis[p]=abi;
            target->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;refused(m);*target=save;
            target->errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_NONE;refused(m);*target=save;
        }
    }
    assert(prepared && xl6IrVerifyModule(m,NULL));
    printf("expanded parameters: rows=%zu prepared=%u mutations=%u zero-output=%u\n",m->foreignParameterCount,prepared,mutations,mutations*2);
    xl6CompileUnitFree(&unit);return 0;
}
