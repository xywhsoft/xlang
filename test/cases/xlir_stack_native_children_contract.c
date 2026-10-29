#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));XL6_ArtifactRuntimePlan plan={0};
    assert(!xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    for(unsigned header=0;header<2;++header){XL6_Output out;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header?xl6IrCgenEmitHeader(&out,ir,"BAD_STACK_CHILD",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(!bytes);xrtFree(text);xl6OutputDiscard(&out);}
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    XL6_IrModule* ir=&unit.ir;assert(xl6IrLowerModule(ir,&unit.parse.diags) && xl6IrVerifyModule(ir,NULL));
    unsigned arrays=0,dicts=0;
    for(size_t i=0;i<ir->types.count;++i){
        XL6_IrType* type=&ir->types.items[i];
        if(type->kind!=XL6_IR_TYPE_GENERIC_INSTANCE || type->runtimeResourceAbi!=XL6_IR_RUNTIME_RESOURCE_STACK)continue;
        XL6_IrSequenceLayout* layout=&type->sequenceLayout;XL6_IrSequenceLayout saved=*layout;
        bool array=layout->abi==XL6_IR_SEQUENCE_LAYOUT_SEQUENCE_REFERENCE_V1 && layout->childRole==XL6_IR_SEQUENCE_ROLE_ARRAY;
        bool dict=layout->abi==XL6_IR_SEQUENCE_LAYOUT_DICT_REFERENCE_V1;if(!array && !dict)continue;
        assert(xl6IrStackConstructionContract(ir,(XL6_IrTypeId)(i+1),NULL));
        if(array)++arrays;else++dicts;
#define BAD(F,V) do{layout->F=(V);refused(ir);*layout=saved;}while(0)
        BAD(abi,XL6_IR_SEQUENCE_LAYOUT_PENDING_DOMAIN);BAD(elementType,XL6_IR_TYPE_INVALID);
        BAD(baseElementType,XL6_IR_TYPE_INVALID);BAD(slotAbi,XL6_IR_NATIVE_SLOT_NONE);
        BAD(transferAbi,XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID);
        if(array){BAD(childRole,XL6_IR_SEQUENCE_ROLE_STACK);BAD(childElementType,XL6_IR_TYPE_INVALID);}
#undef BAD
    }
    assert(arrays>=2 && dicts>=2 && xl6IrVerifyModule(ir,NULL));xl6CompileUnitFree(&unit);
    printf("stack array domains=%u dict domains=%u mutations=%u\n",arrays,dicts,mutations);return 0;
}
