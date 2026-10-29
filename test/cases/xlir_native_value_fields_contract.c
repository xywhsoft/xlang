#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned fields,projections,mutations;
static void refused(XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));XL6_ArtifactRuntimePlan plan={0};
    assert(!xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    for(unsigned header=0;header<2;++header){
        XL6_Output out;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header?xl6IrCgenEmitHeader(&out,ir,"BAD_NATIVE_FIELD",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(!bytes);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc>=2);
    for(int file=1;file<argc;++file){
        XL6_CompileUnit unit;xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[file],1));
        XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));
        for(size_t i=0;i<ir->nodeCount;++i){
            XL6_IrNode* n=&ir->nodes[i];
            if(n->memberAbi!=XL6_IR_MEMBER_ABI_CLASS_FIELD_EXACT_VALUE ||
                !xl6IrNativeValuePlanForType(xl6IrTypeGet(&ir->types,n->storageType)) ||
                n->ownership!=XL6_IR_OWNERSHIP_MOVE)continue;
            ++fields;XL6_IrNode saved=*n;
            assert(n->projectionErrorAbi==XL6_IR_ERROR_ABI_MAY_RAISE);
            assert(n->ownership==XL6_IR_OWNERSHIP_MOVE);
#define BAD_NODE(F,V) do {n->F=(V);refused(ir);*n=saved;} while(0)
            BAD_NODE(memberAbi,XL6_IR_MEMBER_ABI_NONE);
            BAD_NODE(memberAbi,XL6_IR_MEMBER_ABI_CLASS_FIELD_XVALUE_BORROW);
            BAD_NODE(memberAbi,XL6_IR_MEMBER_ABI_CLASS_FIELD_RAW_POINTER_BORROW);
            BAD_NODE(projectionErrorAbi,XL6_IR_ERROR_ABI_NO_RAISE);
            BAD_NODE(ownership,XL6_IR_OWNERSHIP_BORROW);
#undef BAD_NODE
        }
        assert(xl6IrLowerModule(ir,&unit.parse.diags) && xl6IrVerifyModule(ir,NULL));
        for(size_t i=0;i<ir->instructionCount;++i){
            XL6_IrInstruction* ins=&ir->instructions[i];
            if(ins->runtimeOp!=XL6_IR_RUNTIME_NATIVE_VALUE_PROJECT_CHECKED)continue;
            const XL6_IrNode* n=xl6IrNodeGet(ir,ins->origin);
            if(!n || n->memberAbi!=XL6_IR_MEMBER_ABI_CLASS_FIELD_EXACT_VALUE)continue;
            ++projections;XL6_IrInstruction saved=*ins;
            assert(ins->payloadType==ins->type && ins->castAbi==XL6_IR_CAST_XVALUE_TO_NATIVE_VALUE_CHECKED);
            assert(xl6IrInstructionOperandOwnership(ir,ins,0)==XL6_IR_OPERAND_OWNERSHIP_OBSERVE);
#define BAD(F,V) do {ins->F=(V);refused(ir);*ins=saved;} while(0)
            BAD(payloadType,XL6_IR_TYPE_INVALID);BAD(type,XL6_IR_TYPE_INVALID);
            BAD(castAbi,XL6_IR_CAST_INVALID);BAD(operandCount,0);BAD(runtimeIdentityType,ins->type);
#undef BAD
        }
        assert(xl6IrVerifyModule(ir,NULL));xl6CompileUnitFree(&unit);
    }
    printf("native field reads=%u projections=%u mutations=%u\n",fields,projections,mutations);
    fflush(stdout);
    assert(fields>=6 && projections>=fields);
    return 0;
}
