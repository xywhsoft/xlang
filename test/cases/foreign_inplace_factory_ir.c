#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;++header){
        XL6_Output out;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,module,"BAD_STORAGE",NULL) : xl6IrCgenEmitC(&out,module,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(text && !bytes);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));unsigned factories=0;
    /* AOT plans must classify core allocation as XRT implementation, never as
     * an unknown resident component. JIT alone cannot exercise this boundary. */
    XL6_ArtifactRuntimePlan artifact;
    assert(xl6ArtifactRuntimePlanBuild(module,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&artifact,&unit.parse.diags));
    bool memory=false;
    for(size_t i=0;i<artifact.nativeModuleCount;++i)
        if(!strcmp(artifact.nativeModules[i],"XRT_MODULE_MEMORY"))memory=true;
    assert(memory);
    for(size_t i=0;i<module->foreignStorageCount;++i){
        XL6_ForeignStorageContract* row=&module->foreignStorage[i];XL6_ForeignStorageContract saved=*row;
        XL6_IrNode* node=&module->nodes[row->declaration-1];XL6_ForeignCallFacts facts=node->foreignCall;
        const XL6_IrCallTarget* target=xl6IrCallTargetGet(module,node->symbol);assert(target && xl6IrForeignStorageFactoryMatches(module,target));++factories;
        row->declaration=0;refused(module);*row=saved;
        row->adoptSymbol=0;refused(module);*row=saved;
        row->adoptSymbol=facts.rollbackSymbol;refused(module);*row=saved;
        row->adoptSymbol=node->symbol;refused(module);*row=saved;
        row->recordCName[0]='-';refused(module);*row=saved;
        node->foreignCall.inplaceOutput=0;refused(module);node->foreignCall=facts;
        node->foreignCall.inplaceOutput=2;refused(module);node->foreignCall=facts;
        node->foreignCall.valueOutput=1;refused(module);node->foreignCall=facts;
        node->foreignCall.callbackSpanMappings=1;refused(module);node->foreignCall=facts;
        XL6_IrCallTarget* adopter=(XL6_IrCallTarget*)xl6IrCallTargetGet(module,row->adoptSymbol);XL6_IrCallTarget original=*adopter;
        XL6_IrNode* decl=&module->nodes[adopter->declaration-1];uint32 flags=decl->flags;
        decl->flags|=XL6_AST_FLAG_FOREIGN_PURE;refused(module);decl->flags=flags;
        decl->flags&=~XL6_AST_FLAG_FOREIGN_NO_RAISE;refused(module);decl->flags=flags;
        adopter->returnType=target->returnType;refused(module);*adopter=original;
        adopter->resultPhysicalAbi=XL6_IR_CALL_RESULT_PHYSICAL_NATIVE_CLASS_FIELD_BORROW;refused(module);*adopter=original;
        XL6_IrParameterAbi parameter=adopter->parameterAbis[0];adopter->parameterAbis[0]=XL6_IR_PARAMETER_ABI_OWNED;refused(module);adopter->parameterAbis[0]=parameter;
        XL6_IrCallParameterPhysicalAbi physical=adopter->parameterPhysicalAbis[0];adopter->parameterPhysicalAbis[0]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refused(module);adopter->parameterPhysicalAbis[0]=physical;
    }
    size_t count=module->foreignStorageCount;assert(count>=2);--module->foreignStorageCount;refused(module);module->foreignStorageCount=count;
    assert(sizeof(XL6_ForeignCallFacts)==88 && xl6IrVerifyModule(module,NULL));
    printf("in-place native factories: factories=%u mutations=%u zero-output=%u\n",factories,mutations,mutations*2);
    xl6CompileUnitFree(&unit);return 0;
}
