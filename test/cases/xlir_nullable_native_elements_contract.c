#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned mutations;
static void refused(XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));XL6_ArtifactRuntimePlan plan={0};
    assert(!xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    for(unsigned header=0;header<2;++header){XL6_Output out;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header?xl6IrCgenEmitHeader(&out,ir,"BAD_OPTIONAL_ELEMENTS",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(!bytes);xrtFree(text);xl6OutputDiscard(&out);}
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    XL6_IrModule* ir=&unit.ir;assert(xl6IrLowerModule(ir,&unit.parse.diags) && xl6IrVerifyModule(ir,NULL));
    unsigned sequences=0;
    for(size_t i=0;i<ir->types.count;++i){XL6_IrType* t=&ir->types.items[i];
        if(t->sequenceLayout.abi!=XL6_IR_SEQUENCE_LAYOUT_NATIVE_VALUE_V1 || !t->sequenceLayout.nullable)continue;
        ++sequences;XL6_IrSequenceLayout saved=t->sequenceLayout;
        const XL6_IrNativeValuePlan* p=xl6IrNullableNativeValuePlan(&ir->types,xl6IrTypeGet(&ir->types,saved.elementType));
        assert(p && p->sequenceSymbol && p->converterExpr && !strcmp(p->profileSymbol,"xl6NullableSlotMapKeyCollectionProfileV1Get"));
        assert(saved.slotAbi==XL6_IR_NATIVE_SLOT_NULLABLE_SLOT_MAP_KEY_V1 && saved.transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_NATIVE_VALUE_COPY && saved.defaultAbi==XL6_IR_SEQUENCE_DEFAULT_ABSENT);
        t->sequenceLayout.nullable=false;refused(ir);t->sequenceLayout=saved;
        t->sequenceLayout.slotAbi=XL6_IR_NATIVE_SLOT_SLOT_MAP_KEY_V1;refused(ir);t->sequenceLayout=saved;
        t->sequenceLayout.transferAbi=XL6_IR_RUNTIME_RESOURCE_PAYLOAD_TRIVIAL_COPY;refused(ir);t->sequenceLayout=saved;
        t->sequenceLayout.defaultAbi=XL6_IR_SEQUENCE_DEFAULT_NONE;refused(ir);t->sequenceLayout=saved;
        t->sequenceLayout.elementType=saved.baseElementType;refused(ir);t->sequenceLayout=saved;
    }
    assert(sequences>=2 && xl6IrVerifyModule(ir,NULL));
    XL6_ArtifactRuntimePlan plan={0};assert(xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_NULLABLE_SLOT_MAP_KEY_COLLECTION));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_NATIVE_VALUE_POLICY));
    printf("nullable native element sequences=%u mutations=%u\n",sequences,mutations);xl6CompileUnitFree(&unit);return 0;
}
