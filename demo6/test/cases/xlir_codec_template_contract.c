/* Mutates the genuine source-derived lowered IR in the actual compiler SO.
 * Every mutation must be refused before either C or header emits a byte. */
#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned mutations;
static void rejected(XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    for (unsigned header=0;header<2;header++) {
        XL6_Output output;XL6_DiagList diagnostics;size_t size=SIZE_MAX;
        xl6DiagInit(&diagnostics);assert(xl6OutputOpenMemory(&output));
        assert(!(header?xl6IrCgenEmitHeader(&output,ir,"REJECTED_NATIVE_FIELD",&diagnostics):
            xl6IrCgenEmitC(&output,ir,&diagnostics)));
        char* text=xl6OutputTakeMemoryText(&output,&size);
        assert(size==0);xrtFree(text);xl6OutputDiscard(&output);xl6DiagFree(&diagnostics);
    }
    mutations++;
}
static XL6_IrClassField* fieldByDeclaration(XL6_IrModule* ir,XL6_IrNodeId declaration)
{
    for(size_t i=0;i<ir->classFieldCount;i++)
        if(ir->classFields[i].declaration==declaration)return &ir->classFields[i];
    return NULL;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));unsigned checked=0,fields=0;
    unsigned visibilityStates=0;
    for(size_t i=0;i<ir->classFieldCount;i++) {
        XL6_IrClassField* field=&ir->classFields[i];int included=field->publicDataIncluded;
        if(visibilityStates & (1u<<included))continue;
        visibilityStates|=1u<<included;
        field->publicDataIncluded=!included;rejected(ir);field->publicDataIncluded=included;
    }
    for(size_t i=0;i<ir->persistenceProjectFieldCount && i<1;i++) {
        XL6_IrPersistenceProjectField* field=&ir->persistenceProjectFields[i];
        size_t saved=field->classField;field->classField=ir->classFieldCount;
        rejected(ir);field->classField=saved;
        for(size_t f=0;f<ir->classFieldCount;f++)
            if(!ir->classFields[f].publicDataIncluded && !strcmp(ir->classFields[f].name,"secret")) {
                field->classField=f;rejected(ir);field->classField=saved;break;
            }
    }
    for(size_t i=0;i<ir->callTargetCount;i++) {
        XL6_IrCallTarget* target=&ir->callTargets[i];
        if(!target->cName || (strncmp(target->cName,"xl6Codec",8) &&
            strncmp(target->cName,"xl6Template",11) && strncmp(target->cName,"xl6Deflat",9) &&
            strncmp(target->cName,"xl6Inflat",9) && strncmp(target->cName,"xl6Crypto",9) &&
            strncmp(target->cName,"xl6Der",6) && strncmp(target->cName,"xl6Pem",6)))continue;
        assert(xl6IrRuntimeDirectBridgeTargetMatches(ir,target));XL6_IrCallTarget saved=*target;
#define MUTATE(Field,Value) do {target->Field=(Value);rejected(ir);*target=saved;}while(0)
        MUTATE(errorAbi,saved.errorAbi==XL6_IR_ERROR_ABI_MAY_RAISE?XL6_IR_ERROR_ABI_NO_RAISE:XL6_IR_ERROR_ABI_MAY_RAISE);
        MUTATE(effectAbi,saved.effectAbi==XL6_IR_CALL_EFFECT_PURE?XL6_IR_CALL_EFFECT_CONSERVATIVE:XL6_IR_CALL_EFFECT_PURE);
        MUTATE(executionBoundaryAbi,saved.executionBoundaryAbi==XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING?
            XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK:XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING);
        MUTATE(errorPhysicalAbi,(XL6_IrCallErrorPhysicalAbi)999);
        MUTATE(parameterCount,saved.parameterCount+1);
        if(saved.resultPhysicalFieldDeclaration!=XL6_IR_NODE_INVALID) {
            MUTATE(resultPhysicalFieldDeclaration,XL6_IR_NODE_INVALID);
            MUTATE(resultPhysicalDestroySymbol,0);
        }
#undef MUTATE
        for(size_t p=0;p<target->parameterCount;p++) {
            XL6_IrNodeId selected=target->parameterPhysicalFieldDeclarations[p];
            if(selected==XL6_IR_NODE_INVALID)continue;
            XL6_IrClassField* field=fieldByDeclaration(ir,selected);assert(field && field->nativeDestroySymbol);
            target->parameterPhysicalFieldDeclarations[p]=XL6_IR_NODE_INVALID;rejected(ir);
            target->parameterPhysicalFieldDeclarations[p]=selected;
            XL6_SymbolId destructor=field->nativeDestroySymbol;
            field->nativeDestroySymbol=0;rejected(ir);field->nativeDestroySymbol=destructor;
            for(size_t f=0;f<ir->classFieldCount;f++) {
                XL6_IrClassField* other=&ir->classFields[f];
                if(other->ownerType!=field->ownerType || other==field)continue;
                target->parameterPhysicalFieldDeclarations[p]=other->declaration;rejected(ir);
                target->parameterPhysicalFieldDeclarations[p]=selected;fields++;break;
            }
        }
        assert(xl6IrRuntimeDirectBridgeTargetMatches(ir,target));checked++;
    }
    assert(checked>0 && mutations>=5*checked && xl6IrVerifyModule(ir,NULL));
    if(strstr(argv[1],"native_field_mixin"))assert(fields>0);
    printf("%u closed native targets, %u pre-emission mutation rejections, %u mixed fields\n",checked,mutations,fields);
    xl6CompileUnitFree(&unit);return 0;
}
