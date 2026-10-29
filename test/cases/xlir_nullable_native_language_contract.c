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
        assert(!(header?xl6IrCgenEmitHeader(&out,ir,"BAD_OPTIONAL_KEY",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(!bytes);xrtFree(text);xl6OutputDiscard(&out);}
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    XL6_IrModule* ir=&unit.ir;assert(xl6IrLowerModule(ir,&unit.parse.diags) && xl6IrVerifyModule(ir,NULL));
    unsigned types=0,views=0;
    for(size_t i=0;i<ir->types.count;++i){XL6_IrType* type=&ir->types.items[i];
        if(!xl6IrNullableNativeValuePlan(&ir->types,type))continue;
        ++types;XL6_IrType saved=*type;
#define BAD_TYPE(F,V) do{type->F=(V);refused(ir);*type=saved;}while(0)
        BAD_TYPE(cAbi,XL6_IR_C_ABI_TAGGED_NULLABLE);BAD_TYPE(ownershipAbi,XL6_IR_OWNERSHIP_ABI_TRIVIAL);
        BAD_TYPE(nullAbi,XL6_IR_NULL_ABI_POINTER_SENTINEL);BAD_TYPE(abiId,0);
        BAD_TYPE(nativeValueAbi,XL6_IR_NATIVE_VALUE_SLOT_MAP_KEY_V1);BAD_TYPE(typeValueId,0);
        BAD_TYPE(nativeSlotAbi,XL6_IR_NATIVE_SLOT_SLOT_MAP_KEY_V1);
        XL6_IrTypeId element=type->params[0];type->params[0]=0;refused(ir);type->params[0]=element;
#undef BAD_TYPE
    }
    for(size_t i=0;i<ir->instructionCount;++i){XL6_IrInstruction* ins=&ir->instructions[i];
        if(ins->kind!=XL6_IR_INSTR_NULLABLE_UNWRAP || ins->operandCount!=1)continue;
        const XL6_IrNativeValuePlan* element=xl6IrNativeValuePlanForType(xl6IrTypeGet(&ir->types,ins->type));
        if(!element || element->abi!=XL6_IR_NATIVE_VALUE_SLOT_MAP_KEY_V1)continue;
        ++views;assert(xl6IrValueOwnershipGet(ir,ins->result)==XL6_IR_OWNERSHIP_BORROW);
    }
    assert(types && views && xl6IrVerifyModule(ir,NULL));
    XL6_ArtifactRuntimePlan plan={0};assert(xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_NULLABLE_SLOT_MAP_KEY));
    for(unsigned header=0;header<2;++header){XL6_Output out;size_t bytes=0;assert(xl6OutputOpenMemory(&out));
        assert(header?xl6IrCgenEmitHeader(&out,ir,"OPTIONAL_KEY",NULL):xl6IrCgenEmitC(&out,ir,NULL));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(bytes);
        if(!header){for(size_t i=0;i<ir->instructionCount;++i){
            const XL6_IrInstruction* ins=&ir->instructions[i];
            if(ins->runtimeOp!=XL6_IR_RUNTIME_STACK_TRY_POP && ins->runtimeOp!=XL6_IR_RUNTIME_STACK_TRY_PEEK &&
                ins->runtimeOp!=XL6_IR_RUNTIME_STACK_TRY_PEEK_AT)continue;
            char field[80],presence[96];
            snprintf(field,sizeof(field),"&v%u.e1.value",(unsigned)ins->result);
            snprintf(presence,sizeof(presence),"v%u.e1.hasValue = v%u.e0",(unsigned)ins->result,(unsigned)ins->result);
            assert(strstr(text,field) && strstr(text,presence));
        }}
        xrtFree(text);xl6OutputDiscard(&out);}
    xl6CompileUnitFree(&unit);printf("optional native types=%u borrowed views=%u mutations=%u\n",types,views,mutations);return 0;
}
