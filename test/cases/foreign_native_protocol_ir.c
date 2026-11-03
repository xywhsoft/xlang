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
        assert(!(header?xl6IrCgenEmitHeader(&out,module,"BAD_NATIVE_PROTOCOL",NULL):xl6IrCgenEmitC(&out,module,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(text && !bytes);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));unsigned native=0,objects=0,sets=0;
    for(size_t i=0;i<module->protocolDispatchImplCount;++i){
        XL6_IrProtocolDispatchImpl* row=&module->protocolDispatchImpls[i];XL6_IrProtocolDispatchImpl saved=*row;
        if(!xl6IrNativeValuePlanForType(xl6IrTypeGet(&module->types,row->ownerType))){++objects;continue;}
        ++native;
        row->ownerDeclaration=0;refused(module);*row=saved;
        row->ownerType=module->protocolDispatchSlots[row->slot].protocolType;refused(module);*row=saved;
        XL6_IrCallTarget* target=(XL6_IrCallTarget*)xl6IrCallTargetGet(module,row->implementationSymbol);
        XL6_IrCallParameterPhysicalAbi physical=target->parameterPhysicalAbis[0];
        target->parameterPhysicalAbis[0]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refused(module);target->parameterPhysicalAbis[0]=physical;
    }
    for(size_t i=0;i<module->nominalIdentitySetCount;++i){
        XL6_IrNominalIdentitySet* set=&module->nominalIdentitySets[i];
        if(!xl6IrProtocolClassImplements(module,set->targetType,module->protocolDispatchImpls[0].ownerType))continue;
        ++sets;XL6_IrNominalIdentityKind kind=set->kind;assert(kind==XL6_IR_NOMINAL_IDENTITY_EXACT_TYPE_ID && set->identityCount);
        set->kind=XL6_IR_NOMINAL_IDENTITY_PROTOCOL_RUNTIME_TAG;refused(module);set->kind=kind;
        uint64 identity=module->nominalIdentities[set->firstIdentity];module->nominalIdentities[set->firstIdentity]^=UINT64_C(1);refused(module);module->nominalIdentities[set->firstIdentity]=identity;
    }
    size_t count=module->protocolDispatchImplCount;assert(count);--module->protocolDispatchImplCount;refused(module);module->protocolDispatchImplCount=count;
    assert(native>=3 && objects>=2 && sets && xl6IrVerifyModule(module,NULL));
    printf("native protocol: native rows=%u object rows=%u identity sets=%u mutations=%u zero-output=%u\n",native,objects,sets,mutations,mutations*2);
    xl6CompileUnitFree(&unit);return 0;
}
