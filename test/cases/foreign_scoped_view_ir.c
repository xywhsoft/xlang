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
        assert(!(header ? xl6IrCgenEmitHeader(&out,module,"BAD_SCOPE_VIEW",NULL) : xl6IrCgenEmitC(&out,module,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(text && !bytes);
        xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));
    unsigned types=0,inputs=0,results=0,callbacks=0;
    for(size_t i=0;i<module->types.count;++i){
        XL6_IrType* type=&module->types.items[i];
        if(type->foreignLifetime!=XL6_FOREIGN_LIFETIME_CALLBACK)continue;
        ++types;XL6_IrType saved=*type;
        type->foreignLifetime=XL6_FOREIGN_LIFETIME_CALLER;refused(module);*type=saved;
        type->foreignLifetime=XL6_FOREIGN_LIFETIME_OWNER;refused(module);*type=saved;
        type->foreignOwner=(XL6_IrTypeId)(i+1);refused(module);*type=saved;
        type->foreignOwner=0;refused(module);*type=saved;
        type->ownershipAbi=XL6_IR_OWNERSHIP_ABI_TRIVIAL;refused(module);*type=saved;
        type->nativeValueAbi=XL6_IR_NATIVE_VALUE_FOREIGN_VIEW_V1;refused(module);*type=saved;
        type->cAbi=XL6_IR_C_ABI_NATIVE_HANDLE;refused(module);*type=saved;
        type->foreignPolicy="managedChildPolicy";refused(module);*type=saved;
        type->foreignScope="ManagedScope";refused(module);*type=saved;
        type->foreignWatch="ManagedWatch";refused(module);*type=saved;
        type->nullAbi=XL6_IR_NULL_ABI_POINTER_SENTINEL;refused(module);*type=saved;
    }
    for(size_t i=0;i<module->callTargetCount;++i){
        XL6_IrCallTarget* target=&module->callTargets[i];
        XL6_IrNode* node=target->declaration ? &module->nodes[target->declaration-1] : NULL;
        for(size_t p=0;p<target->parameterCount;++p){
            const XL6_IrType* type=xl6IrTypeGet(&module->types,target->parameterTypes[p]);
            if(target->parameterPhysicalAbis[p]==XL6_IR_CALL_PARAMETER_PHYSICAL_RETAINED_C_CALLBACK)++callbacks;
            if(!type || type->foreignLifetime!=XL6_FOREIGN_LIFETIME_CALLBACK ||
                target->parameterPhysicalAbis[p]!=XL6_IR_CALL_PARAMETER_PHYSICAL_FOREIGN_VIEW_BORROW)continue;
            ++inputs;XL6_IrCallTarget saved=*target;
            XL6_IrCallParameterPhysicalAbi physical=target->parameterPhysicalAbis[p];
            target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refused(module);target->parameterPhysicalAbis[p]=physical;
            XL6_IrParameterAbi abi=target->parameterAbis[p];target->parameterAbis[p]=XL6_IR_PARAMETER_ABI_VALUE;refused(module);target->parameterAbis[p]=abi;
            target->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;refused(module);*target=saved;
        }
        if(!node || node->foreignCall.borrowedViewResult!=XL6_FOREIGN_VIEW_RESULT_SCOPE_NATIVE)continue;
        ++results;XL6_ForeignCallFacts saved=node->foreignCall;
        node->foreignCall.borrowedViewResult=XL6_FOREIGN_VIEW_RESULT_OWNER_NATIVE;refused(module);node->foreignCall=saved;
        node->foreignCall.resultOwnerIndexPlusOne=0;refused(module);node->foreignCall=saved;
        node->foreignCall.resultOwnerIndexPlusOne=64;refused(module);node->foreignCall=saved;
        XL6_IrCallTarget call=*target;
        target->resultConversionAbi=XL6_IR_CALL_RESULT_CONVERSION_DIRECT;refused(module);*target=call;
        target->resultCAbi=XL6_IR_CALL_RESULT_C_ABI_STORAGE;refused(module);*target=call;
    }
    assert(types>=2 && inputs>=3 && results>=2 && callbacks && xl6IrVerifyModule(module,NULL));
    printf("scope views: types=%u inputs=%u results=%u callbacks=%u mutations=%u zero-output=%u\n",
        types,inputs,results,callbacks,mutations,mutations*2);
    xl6CompileUnitFree(&unit);return 0;
}
