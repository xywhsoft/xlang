#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned comparisons,emptyOperands,queries,awaits,mutations,emissions;
static void queryRefused(XL6_IrModule* module,XL6_IrNodeId expression)
{
    XL6_IrNullOperandAbi abi=(XL6_IrNullOperandAbi)77;
    assert(!xl6IrNullOperandPlanBuild(module,expression,&abi) && abi==(XL6_IrNullOperandAbi)77);
    ++queries;
}
static void refused(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;++header) {
        XL6_Output output;size_t size=SIZE_MAX;assert(xl6OutputOpenMemory(&output));
        assert(!(header ? xl6IrCgenEmitHeader(&output,module,"BAD_NULL_COMPARISON_H",NULL) : xl6IrCgenEmitC(&output,module,NULL)));
        char* body=xl6OutputTakeMemoryText(&output,&size);assert(body && !size);
        xrtFree(body);xl6OutputDiscard(&output);++emissions;
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==4);
    for(int file=1;file<argc;++file) {
        XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
        assert(xl6CompileUnitLoadFile(&unit,argv[file],1));XL6_IrModule* module=&unit.ir;
        for(size_t n=0;n<module->nodeCount;++n) {
            XL6_IrNode* node=&module->nodes[n];XL6_IrNodeId id=(XL6_IrNodeId)(n+1);
            if(node->kind==XL6_IR_NODE_BINARY && (node->opcode==XL6_IR_OPCODE_EQ || node->opcode==XL6_IR_OPCODE_NE)) {
                XL6_IrNullComparisonPlan plan={0};assert(xl6IrNullComparisonPlanBuild(module,id,&plan));
                if(plan.operands[0] || plan.operands[1]) {
                    ++comparisons;XL6_IrNullComparisonPlan saved=plan;
                    size_t count=node->childCount;node->childCount=1;
                    assert(!xl6IrNullComparisonPlanBuild(module,id,&plan) && !memcmp(&plan,&saved,sizeof(plan)));
                    node->childCount=count;++queries;
                }
            }
            XL6_IrNullOperandAbi abi;
            if(node->kind!=XL6_IR_NODE_AWAIT || !xl6IrTypeIsZeroPayload(module,node->storageType)) continue;
            assert(xl6IrNullOperandPlanBuild(module,id,&abi) && abi==XL6_IR_NULL_OPERAND_EMPTY_AWAIT);++emptyOperands;
            XL6_IrNode saved=*node;node->kind=XL6_IR_NODE_CALL;queryRefused(module,id);*node=saved;
            node->firstChild=module->childCount;queryRefused(module,id);*node=saved;
            node->childCount=0;queryRefused(module,id);*node=saved;
            XL6_IrNode* input=&module->nodes[module->children[node->firstChild]-1];
            XL6_IrType* future=&module->types.items[input->type-1];
            XL6_IrTypeId payload=future->params[0];future->params[0]=input->type;
            queryRefused(module,id);future->params[0]=payload;
        }
        assert(xl6IrVerifyModule(module,NULL) && xl6IrLowerModule(module,&unit.parse.diags));
        for(size_t i=0;i<module->instructionCount;++i) {
            XL6_IrInstruction* instruction=&module->instructions[i];
            if(instruction->kind!=XL6_IR_INSTR_AWAIT || instruction->asyncResultAbi!=XL6_IR_ASYNC_RESULT_VOID) continue;
            ++awaits;XL6_IrInstruction saved=*instruction;
            assert(instruction->type==XL6_IR_TYPE_INVALID && instruction->result &&
                instruction->futureOutcomeAbi==XL6_IR_FUTURE_OUTCOME_XRT_ERROR_KIND &&
                instruction->awaitOutcomeAbi==XL6_IR_AWAIT_OUTCOME_STATUS_DIAGNOSTIC_NULL_STATE_ERROR);
            for(size_t u=0;u<module->instructionCount;++u) {
                const XL6_IrInstruction* use=&module->instructions[u];
                for(size_t p=0;p<use->operandCount;++p)
                    if(module->operands[use->firstOperand+p]==instruction->result) assert(use->kind==XL6_IR_INSTR_ERROR_TEST);
            }
            instruction->asyncResultAbi=XL6_IR_ASYNC_RESULT_COPY_I64;refused(module);*instruction=saved;
            instruction->futureOutcomeAbi=0;refused(module);*instruction=saved;
            instruction->awaitOutcomeAbi=0;refused(module);*instruction=saved;
        }
        assert(xl6IrVerifyModule(module,NULL));xl6CompileUnitFree(&unit);
    }
    assert(comparisons>=8 && emptyOperands>=12 && awaits==emptyOperands && queries==comparisons+emptyOperands*4);
    assert(mutations==awaits*3 && emissions==mutations*2);
    printf("null comparison proof: comparisons=%u empty-operands=%u queries=%u awaits=%u mutations=%u zero-output=%u\n",
        comparisons,emptyOperands,queries,awaits,mutations,emissions);
    return 0;
}
