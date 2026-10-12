#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned loops,nullable,projections,mutations;
static void refused(XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));XL6_ArtifactRuntimePlan plan={0};
    assert(!xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    for(unsigned header=0;header<2;++header) {
        XL6_Output out;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_FUTURE_ITERATION",NULL) : xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(!bytes);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc>=2);
    for(int file=1;file<argc;++file) {
        XL6_CompileUnit unit;xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[file],1));
        XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));
        for(size_t i=0;i<ir->nodeCount;++i) {
            XL6_IrNode* n=&ir->nodes[i];
            if(n->loopItemAbi!=XL6_IR_LOOP_ITEM_FUTURE_COPY && n->loopItemAbi!=XL6_IR_LOOP_ITEM_NULLABLE_FUTURE_COPY)continue;
            ++loops;nullable+=n->loopItemAbi==XL6_IR_LOOP_ITEM_NULLABLE_FUTURE_COPY;
            assert(n->loopItemLifetimeAbi==XL6_IR_LOOP_TARGET_LIFETIME_OWNED_ITERATION_RELEASE && n->loopErrorAbi==XL6_IR_ERROR_ABI_MAY_RAISE);
            XL6_IrNode saved=*n;
#define BAD_NODE(F,V) do {n->F=(V);refused(ir);*n=saved;} while(0)
            BAD_NODE(loopItemAbi,XL6_IR_LOOP_ITEM_XVALUE_RETAIN);
            BAD_NODE(loopItemAbi,saved.loopItemAbi==XL6_IR_LOOP_ITEM_FUTURE_COPY ? XL6_IR_LOOP_ITEM_NULLABLE_FUTURE_COPY : XL6_IR_LOOP_ITEM_FUTURE_COPY);
            BAD_NODE(loopItemLifetimeAbi,XL6_IR_LOOP_TARGET_LIFETIME_TRIVIAL_REPLACE);
            BAD_NODE(loopItemLifetimeAbi,XL6_IR_LOOP_TARGET_LIFETIME_NONE);
            BAD_NODE(loopErrorAbi,XL6_IR_ERROR_ABI_NO_RAISE);
            BAD_NODE(loopResourceAbi,XL6_IR_LOOP_RESOURCE_NONE);
#undef BAD_NODE
        }
        assert(xl6IrLowerModule(ir,&unit.parse.diags) && xl6IrVerifyModule(ir,NULL));
        for(size_t i=0;i<ir->instructionCount;++i) {
            XL6_IrInstruction* ins=&ir->instructions[i];
            if(ins->runtimeOp!=XL6_IR_RUNTIME_XVALUE_PROJECT_FUTURE_COPY && ins->runtimeOp!=XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_FUTURE_COPY)continue;
            ++projections;XL6_IrInstruction saved=*ins;
            assert(ins->payloadType==xl6IrFutureProjectionPayloadType(ir,ins->type,ins->runtimeOp));
            assert(xl6IrInstructionOperandOwnership(ir,ins,0)==XL6_IR_OPERAND_OWNERSHIP_OBSERVE);
#define BAD(F,V) do {ins->F=(V);refused(ir);*ins=saved;} while(0)
            BAD(payloadType,XL6_IR_TYPE_INVALID);BAD(payloadType,ins->type);BAD(type,XL6_IR_TYPE_INVALID);
            BAD(operandCount,0);BAD(calleeType,ins->type);BAD(runtimeIdentityType,ins->type);
            BAD(runtimeOp,saved.runtimeOp==XL6_IR_RUNTIME_XVALUE_PROJECT_FUTURE_COPY ? XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_FUTURE_COPY : XL6_IR_RUNTIME_XVALUE_PROJECT_FUTURE_COPY);
#undef BAD
        }
        assert(xl6IrVerifyModule(ir,NULL));xl6CompileUnitFree(&unit);
    }
    assert(loops>=12 && nullable>=3 && projections>=loops);
    printf("future iteration loops=%u nullable=%u projections=%u mutations=%u\n",loops,nullable,projections,mutations);
    return 0;
}
