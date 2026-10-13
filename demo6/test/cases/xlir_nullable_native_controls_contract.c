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
        assert(!(header?xl6IrCgenEmitHeader(&out,ir,"BAD_NATIVE_CONTROLS",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(!bytes);xrtFree(text);xl6OutputDiscard(&out);}
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    XL6_IrModule* ir=&unit.ir;assert(xl6IrLowerModule(ir,&unit.parse.diags) && xl6IrVerifyModule(ir,NULL));
    unsigned casts[3]={0},defaults=0,projections=0;
    for(size_t i=0;i<ir->nodeCount;++i){XL6_IrNode* node=&ir->nodes[i];
        if(node->castAbi<XL6_IR_CAST_NATIVE_VALUE_TO_NULLABLE || node->castAbi>XL6_IR_CAST_XVALUE_TO_NULLABLE_NATIVE_VALUE_TRY)continue;
        ++casts[node->castAbi-XL6_IR_CAST_NATIVE_VALUE_TO_NULLABLE];XL6_IrNode saved=*node;
        node->castAbi=XL6_IR_CAST_IDENTITY;refused(ir);*node=saved;
        node->castErrorAbi=XL6_IR_ERROR_ABI_INVALID;refused(ir);*node=saved;
    }
    for(size_t i=0;i<ir->valueInitializerCount;++i){XL6_IrValueInitializer* init=&ir->valueInitializers[i];
        if(init->abi!=XL6_IR_INITIALIZER_TAGGED_NULL || !xl6IrNullableNativeValuePlan(&ir->types,xl6IrTypeGet(&ir->types,init->type)))continue;
        ++defaults;XL6_IrValueInitializer saved=*init;
        init->abi=XL6_IR_INITIALIZER_SCALAR_ZERO;refused(ir);*init=saved;
    }
    for(size_t i=0;i<ir->instructionCount;++i){XL6_IrInstruction* ins=&ir->instructions[i];
        if(ins->runtimeOp!=XL6_IR_RUNTIME_NATIVE_VALUE_PROJECT_TRY)continue;
        ++projections;XL6_IrInstruction saved=*ins;
        assert(xl6IrRuntimeOperandOwnership(ins->runtimeOp,0)==XL6_IR_OPERAND_OWNERSHIP_OBSERVE);
        ins->runtimeOp=XL6_IR_RUNTIME_NATIVE_VALUE_PROJECT_CHECKED;refused(ir);*ins=saved;
        ins->payloadType=0;refused(ir);*ins=saved;
        ins->castAbi=XL6_IR_CAST_XVALUE_TO_NATIVE_VALUE_CHECKED;refused(ir);*ins=saved;
    }
    assert(casts[0] && casts[1] && casts[2] && defaults && projections && xl6IrVerifyModule(ir,NULL));
    for(unsigned header=0;header<2;++header){XL6_Output out;size_t bytes=0;assert(xl6OutputOpenMemory(&out));
        assert(header?xl6IrCgenEmitHeader(&out,ir,"NATIVE_CONTROLS",NULL):xl6IrCgenEmitC(&out,ir,NULL));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(bytes);
        if(!header)assert(strstr(text,"xl6NullableSlotMapKeyTryUnboxV1"));
        xrtFree(text);xl6OutputDiscard(&out);}
    xl6CompileUnitFree(&unit);
    printf("native nullable lifts=%u unwraps=%u tries=%u defaults=%u projections=%u mutations=%u\n",
        casts[0],casts[1],casts[2],defaults,projections,mutations);return 0;
}
