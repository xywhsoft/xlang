#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* module,const char* label)
{
    if(xl6IrVerifyModule(module,NULL)){fprintf(stderr,"unexpected NULL/cstr IR: %s\n",label);assert(0);}
    for(unsigned header=0;header<2;++header){
        XL6_Output out;size_t size=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header?xl6IrCgenEmitHeader(&out,module,"BAD_NULL_CSTR",NULL):xl6IrCgenEmitC(&out,module,NULL)));
        char* bytes=xl6OutputTakeMemoryText(&out,&size);assert(bytes && !size);xrtFree(bytes);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));assert(module->foreignFixedInputCount==3);
    for(size_t i=0;i<module->foreignFixedInputCount;++i){
        XL6_ForeignFixedInputContract* row=&module->foreignFixedInputs[i];XL6_ForeignFixedInputContract saved=*row;
        strcpy(row->pointerCName,"char*");refused(module,"mutable pointer");*row=saved;
        strcpy(row->pointerCName,"const unsigned char*");refused(module,"other character type");*row=saved;
        strcpy(row->pointerCName,"const void*");refused(module,"untyped pointer");*row=saved;
        row->physicalIndex=63;refused(module,"changed physical slot");*row=saved;
        row->kind=XL6_FOREIGN_FIXED_MODULE_CODE_OWNER;strcpy(row->pointerCName,"const XL6_CodeOwner*");refused(module,"module capability is not NULL");*row=saved;
    }
    for(size_t i=0;i<module->callTargetCount;++i){
        XL6_IrCallTarget* target=&module->callTargets[i];if(target->kind!=XL6_IR_CALL_TARGET_EXTERN_C)continue;
        for(size_t p=0;p<target->parameterCount;++p){
            if(target->parameterPhysicalAbis[p]!=XL6_IR_CALL_PARAMETER_PHYSICAL_TERMINATED_STRING_BORROW)continue;
            XL6_IrCallParameterPhysicalAbi physical=target->parameterPhysicalAbis[p];
            target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_TEXT_VIEW_BORROW;refused(module,"xstrview is not const char pointer");target->parameterPhysicalAbis[p]=physical;
            XL6_IrParameterAbi abi=target->parameterAbis[p];target->parameterAbis[p]=XL6_IR_PARAMETER_ABI_VALUE;refused(module,"lost managed borrow");target->parameterAbis[p]=abi;
            XL6_IrNode* node=&module->nodes[target->declaration-1];uint64 bits=node->foreignCall.terminatedStrings;
            node->foreignCall.terminatedStrings=0;refused(module,"lost explicit C-string contract");node->foreignCall.terminatedStrings=bits;
            XL6_IrCallParameterConstraintAbi constraint=target->parameterConstraintAbis[p];
            target->parameterConstraintAbis[p]=(XL6_IrCallParameterConstraintAbi)UINT32_MAX;refused(module,"unknown input constraint");target->parameterConstraintAbis[p]=constraint;
        }
    }
    assert(xl6IrVerifyModule(module,NULL));printf("NULL CString: %u malformed contracts rejected before C/header output\n",mutations);
    xl6CompileUnitFree(&unit);return 0;
}
