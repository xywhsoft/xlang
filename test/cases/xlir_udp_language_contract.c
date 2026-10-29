/* Exact UDP language ABI corruption. Compiles but never runs the socket fixture. */
#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t targets,refusals,enums,resources,emissionRefusals;
static bool exerciseEmission;
static void refused(XL6_IrModule* module,XL6_IrCallTarget* target)
{
    assert(!xl6IrRuntimeDirectBridgeTargetMatches(module,target));
    ++refusals;
    /* Exact matching is the shared verifier predicate for every mutation.
     * Test whole-module/C/header refusal once per target, not three complete
     * verification passes for every field permutation of the same target. */
    if(!exerciseEmission)return;
    exerciseEmission=false;assert(!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;++header){XL6_Output output;XL6_DiagList errors;size_t size=SIZE_MAX;
        xl6DiagInit(&errors);assert(xl6OutputOpenMemory(&output));
        assert(!(header?xl6IrCgenEmitHeader(&output,module,"BAD_UDP_LANGUAGE",&errors):xl6IrCgenEmitC(&output,module,&errors)));
        char* text=xl6OutputTakeMemoryText(&output,&size);assert(!size);xrtFree(text);xl6OutputDiscard(&output);xl6DiagFree(&errors);}
    ++emissionRefusals;
}
static void enumContract(XL6_IrModule* m,XL6_IrCallTarget* target,XL6_IrTypeId id,XL6_IrTypeId integer)
{
    XL6_IrEnumLayout* layout=(XL6_IrEnumLayout*)xl6IrEnumLayoutGet(m,id);if(!layout)return;
    XL6_IrType* type=(XL6_IrType*)xl6IrTypeGet(&m->types,id);XL6_IrType saved=*type;++enums;
    type->nominalSymbol=0;refused(m,target);*type=saved;
    type->integerTarget=XL6_IR_INTEGER_TARGET_SIGNED_32;refused(m,target);*type=saved;
    size_t count=layout->variantCount;--layout->variantCount;refused(m,target);layout->variantCount=count;
    XL6_IrEnumVariant* member=&m->enumVariants[layout->firstVariant];char* name=member->name;
    member->name="notARegisteredUdpMember";refused(m,target);member->name=name;
    XL6_IrTypeId* slot=target->returnType==id?&target->returnType:NULL;
    for(size_t p=0;!slot&&p<target->parameterCount;++p)if(target->parameterTypes[p]==id)slot=&target->parameterTypes[p];
    assert(slot);*slot=integer;refused(m,target);*slot=id;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));XL6_IrModule* m=&unit.ir;assert(xl6IrVerifyModule(m,NULL));
    XL6_IrTypeId integer=0,unsignedInteger=0;
    for(size_t i=0;i<m->types.count;++i){const XL6_IrType* type=&m->types.items[i];
        if(type->kind==XL6_IR_TYPE_BUILTIN&&type->cAbi==XL6_IR_C_ABI_I64&&type->integerTarget==XL6_IR_INTEGER_TARGET_SIGNED_64)integer=(XL6_IrTypeId)i+1;
        if(type->kind==XL6_IR_TYPE_BUILTIN&&type->cAbi==XL6_IR_C_ABI_U64&&type->integerTarget==XL6_IR_INTEGER_TARGET_UNSIGNED_64)unsignedInteger=(XL6_IrTypeId)i+1;}
    assert(integer&&unsignedInteger);
    for(size_t i=0;i<m->callTargetCount;++i){XL6_IrCallTarget* t=&m->callTargets[i];
        if(!t->cName||strncmp(t->cName,"xl6Udp",6)||!strncmp(t->cName,"xl6UdpConfig",12))continue;
        XL6_IrCallTarget saved=*t;++targets;exerciseEmission=true;assert(xl6IrRuntimeDirectBridgeTargetMatches(m,t));
        #define BAD(Field,Value) do{t->Field=(Value);refused(m,t);*t=saved;}while(0)
        BAD(errorAbi,saved.errorAbi==XL6_IR_ERROR_ABI_NO_RAISE?XL6_IR_ERROR_ABI_MAY_RAISE:XL6_IR_ERROR_ABI_NO_RAISE);
        BAD(effectAbi,saved.effectAbi==XL6_IR_CALL_EFFECT_PURE?XL6_IR_CALL_EFFECT_CONSERVATIVE:XL6_IR_CALL_EFFECT_PURE);
        BAD(executionBoundaryAbi,saved.executionBoundaryAbi==XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING?XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK:XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING);
        BAD(nativeServicesRequired,saved.nativeServicesRequired?0:XL6_IR_CALL_NATIVE_SERVICE_EXECUTION_SOURCE_V1);
        if(saved.errorPhysicalAbi!=XL6_IR_CALL_ERROR_PHYSICAL_NONE)BAD(errorPhysicalAbi,XL6_IR_CALL_ERROR_PHYSICAL_NONE);
        if(saved.resultPhysicalAbi!=XL6_IR_CALL_RESULT_PHYSICAL_DIRECT){++resources;BAD(resultPhysicalAbi,XL6_IR_CALL_RESULT_PHYSICAL_DIRECT);BAD(resultPhysicalFieldDeclaration,XL6_IR_NODE_INVALID);BAD(resultPhysicalDestroySymbol,0);}
        enumContract(m,t,t->returnType,integer);
        for(size_t p=0;p<t->parameterCount;++p){XL6_IrTypeId old=t->parameterTypes[p];enumContract(m,t,old,integer);
            const XL6_IrType* type=xl6IrTypeGet(&m->types,old);
            if(type&&type->kind==XL6_IR_TYPE_CLASS){
                XL6_IrCallParameterPhysicalAbi abi=t->parameterPhysicalAbis[p];t->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refused(m,t);t->parameterPhysicalAbis[p]=abi;
                t->parameterTypes[p]=integer;refused(m,t);t->parameterTypes[p]=old;
            }else if(type&&type->kind==XL6_IR_TYPE_BUILTIN){t->parameterTypes[p]=old==integer?unsignedInteger:integer;refused(m,t);t->parameterTypes[p]=old;}}
        assert(xl6IrVerifyModule(m,NULL));
        #undef BAD
    }
    assert(targets>=90&&resources>=15&&enums>=10&&refusals>500&&emissionRefusals==targets);
    xl6CompileUnitFree(&unit);printf("UDP language XLIR: %zu targets, %zu enum positions, %zu owned results, %zu corrupt contracts refused, %zu whole-module/C/header refusals\n",targets,enums,resources,refusals,emissionRefusals);return 0;
}


