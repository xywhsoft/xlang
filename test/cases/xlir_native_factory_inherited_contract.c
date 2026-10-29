#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static size_t refusals;
static void reject(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module,NULL));
    for(int header=0;header!=2;++header) {
        XL6_Output output;XL6_DiagList diagnostics;size_t size=SIZE_MAX;
        xl6DiagInit(&diagnostics);assert(xl6OutputOpenMemory(&output));
        assert(!(header ? xl6IrCgenEmitHeader(&output,module,"BAD_NATIVE_INHERITANCE",&diagnostics) :
            xl6IrCgenEmitC(&output,module,&diagnostics)));
        char* text=xl6OutputTakeMemoryText(&output,&size);
        assert(size==0);xrtFree(text);xl6OutputDiscard(&output);xl6DiagFree(&diagnostics);
    }
    ++refusals;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;size_t factories=0,borrows=0;
    assert(xl6IrVerifyModule(module,NULL));
    for(size_t i=0;i<module->callTargetCount;++i) {
        XL6_IrCallTarget* target=&module->callTargets[i];
        XL6_IrCallTarget saved=*target;
        const XL6_IrClassField* field=xl6IrCallTargetNativeClassResultField(module,target);
        if(field && field->ownerType!=target->returnType) {
            assert(field==xl6IrManagedNativeConstructorField(module,target->returnType));
            assert(xl6IrTypeRequiresExplicitNativeInitialization(module,target->returnType));
            assert(xl6IrRuntimeDirectBridgeTargetMatches(module,target));
            target->resultPhysicalFieldDeclaration=XL6_IR_NODE_INVALID;reject(module);*target=saved;
            target->resultPhysicalDestroySymbol=0;reject(module);*target=saved;
            target->resultPhysicalAbi=XL6_IR_CALL_RESULT_PHYSICAL_DIRECT;reject(module);*target=saved;
            XL6_IrClassField* mutableField=&module->classFields[field-module->classFields];
            XL6_IrTypeId owner=mutableField->ownerType;
            mutableField->ownerType=target->returnType;
            assert(!xl6IrManagedNativeConstructorField(module,target->returnType));reject(module);
            mutableField->ownerType=owner;
            for(size_t l=0;l<module->classLayoutCount;++l) {
                XL6_IrClassLayout* layout=&module->classLayouts[l];
                if(layout->type!=target->returnType)continue;
                XL6_IrTypeId base=layout->baseType;
                layout->baseType=layout->type;
                assert(!xl6IrManagedNativeConstructorField(module,target->returnType));reject(module);
                layout->baseType=base;
            }
            ++factories;
        }
        for(size_t p=0;p<target->parameterCount;++p) {
            field=xl6IrCallTargetNativeClassPhysicalField(module,target,p);
            if(!field || field->ownerType==target->parameterTypes[p])continue;
            XL6_IrNodeId declaration=target->parameterPhysicalFieldDeclarations[p];
            target->parameterPhysicalFieldDeclarations[p]=XL6_IR_NODE_INVALID;reject(module);
            target->parameterPhysicalFieldDeclarations[p]=declaration;
            XL6_IrCallParameterPhysicalAbi abi=target->parameterPhysicalAbis[p];
            target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;reject(module);
            target->parameterPhysicalAbis[p]=abi;
            ++borrows;
        }
    }
    assert(factories>=3 && borrows>=1 && refusals>=17);
    assert(xl6IrVerifyModule(module,NULL));xl6CompileUnitFree(&unit);
    printf("inherited native contracts: %zu factories, %zu borrows, %zu refusals\n",factories,borrows,refusals);
    return 0;
}
