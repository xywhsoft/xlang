/* Closed Ed25519 modes/value contracts before module/C/header output. Offline only. */
#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const char* const entries[]={
    "xl6CryptoEd25519CreateV1",
    "xl6CryptoEd25519DestroyV1",
    "xl6CryptoEd25519PublicV1",
    "xl6CryptoEd25519ExportSeedV1",
    "xl6CryptoEd25519GenerateV1",
    "xl6CryptoEd25519KeySignTextV1",
    "xl6CryptoEd25519KeySignContextTextV1",
    "xl6CryptoEd25519SignTextV1",
    "xl6CryptoEd25519VerifyTextV1",
    "xl6CryptoEd25519VerifyContextTextV1",
    "xl6CryptoEd25519KeySignBytesV1",
    "xl6CryptoEd25519KeySignContextBytesV1",
    "xl6CryptoEd25519SignBytesV1",
    "xl6CryptoEd25519VerifyBytesV1",
    "xl6CryptoEd25519VerifyContextBytesV1",
    "xl6CryptoEd25519KeySignPrehashV1",
    "xl6CryptoEd25519KeySignBufferV1",
    "xl6CryptoEd25519KeySignContextBufferV1",
    "xl6CryptoEd25519SignBufferV1",
    "xl6CryptoEd25519PublicFromSeedV1",
    "xl6CryptoEd25519VerifyPrehashV1",
    "xl6CryptoEd25519VerifyBufferV1",
    "xl6CryptoEd25519VerifyContextBufferV1"
};
static size_t refusals,targets,handles,resources;
static void refused(XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    if(target)assert(!xl6IrRuntimeDirectBridgeTargetMatches(module,target));
    assert(!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;++header) {
        XL6_Output output;XL6_DiagList diagnostics;size_t size=SIZE_MAX;
        xl6DiagInit(&diagnostics);assert(xl6OutputOpenMemory(&output));
        assert(!(header?xl6IrCgenEmitHeader(&output,module,"BAD_ED25519",&diagnostics):xl6IrCgenEmitC(&output,module,&diagnostics)));
        char* text=xl6OutputTakeMemoryText(&output,&size);assert(!size);
        xrtFree(text);xl6OutputDiscard(&output);xl6DiagFree(&diagnostics);
    }
    ++refusals;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));
    XL6_IrTypeId integer=0;
    for(size_t i=0;i<module->types.count;++i) {
        const XL6_IrType* type=&module->types.items[i];
        if(type->kind==XL6_IR_TYPE_BUILTIN&&type->cAbi==XL6_IR_C_ABI_I64&&type->integerTarget==XL6_IR_INTEGER_TARGET_SIGNED_64)integer=(XL6_IrTypeId)i+1;
    }
    assert(integer);bool seen[sizeof(entries)/sizeof(*entries)]={0};
    for(size_t i=0;i<module->callTargetCount;++i) {
        XL6_IrCallTarget* target=&module->callTargets[i];size_t index=0;
        while(index<sizeof(entries)/sizeof(*entries)&&(!target->cName||strcmp(entries[index],target->cName)))++index;
        if(index==sizeof(entries)/sizeof(*entries))continue;
        seen[index]=true;++targets;assert(xl6IrRuntimeDirectBridgeTargetMatches(module,target));
        XL6_IrCallTarget saved=*target;
        #define BAD(Field,Value) do{target->Field=(Value);refused(module,target);*target=saved;}while(0)
        BAD(errorAbi,saved.errorAbi==XL6_IR_ERROR_ABI_NO_RAISE?XL6_IR_ERROR_ABI_MAY_RAISE:XL6_IR_ERROR_ABI_NO_RAISE);
        BAD(effectAbi,saved.effectAbi==XL6_IR_CALL_EFFECT_PURE?XL6_IR_CALL_EFFECT_CONSERVATIVE:XL6_IR_CALL_EFFECT_PURE);
        BAD(executionBoundaryAbi,saved.executionBoundaryAbi==XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING?XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK:XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING);
        BAD(nativeServicesRequired,XL6_IR_CALL_NATIVE_SERVICE_EXECUTION_SOURCE_V1);
        BAD(nativeContextAbi,XL6_IR_CALL_NATIVE_CONTEXT_CLASS_RESULT_V1);
        BAD(returnType,integer);
        if(saved.errorPhysicalAbi!=XL6_IR_CALL_ERROR_PHYSICAL_NONE)BAD(errorPhysicalAbi,XL6_IR_CALL_ERROR_PHYSICAL_NONE);
        if(saved.nativeHandlePolicyCName) {
            ++handles;target->nativeHandlePolicyCName="notAnEd25519Policy";refused(module,NULL);*target=saved;
        }
        if(saved.resultPhysicalAbi!=XL6_IR_CALL_RESULT_PHYSICAL_DIRECT) {
            ++resources;BAD(resultPhysicalAbi,XL6_IR_CALL_RESULT_PHYSICAL_DIRECT);
            BAD(resultPhysicalFieldDeclaration,XL6_IR_NODE_INVALID);BAD(resultPhysicalDestroySymbol,0);
        }
        for(size_t p=0;p<target->parameterCount;++p) {
            XL6_IrTypeId original=target->parameterTypes[p];
            if(original!=integer){target->parameterTypes[p]=integer;refused(module,target);target->parameterTypes[p]=original;}
            const XL6_IrType* type=xl6IrTypeGet(&module->types,original);
            if(type&&type->kind==XL6_IR_TYPE_CLASS) {
                XL6_IrCallParameterPhysicalAbi physical=target->parameterPhysicalAbis[p];
                target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refused(module,target);target->parameterPhysicalAbis[p]=physical;
                XL6_IrNodeId field=target->parameterPhysicalFieldDeclarations[p];
                target->parameterPhysicalFieldDeclarations[p]=XL6_IR_NODE_INVALID;refused(module,target);target->parameterPhysicalFieldDeclarations[p]=field;
            }
        }
        assert(xl6IrVerifyModule(module,NULL));
        #undef BAD
    }
    for(size_t i=0;i<sizeof(entries)/sizeof(*entries);++i){if(!seen[i])fprintf(stderr,"missing Ed25519 target: %s\n",entries[i]);assert(seen[i]);}
    assert(targets>=23&&handles>=1&&resources>=2&&refusals>180);
    printf("Ed25519 XLIR: %zu direct targets, %zu bad contracts refused before complete module/C/header output\n",targets,refusals);
    xl6CompileUnitFree(&unit);return 0;
}
