#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t mutations;
static const XL6_IrInstruction* definition(const XL6_IrModule* ir, XL6_IrValueId value)
{
    size_t i;
    for (i = 0; i < ir->instructionCount; ++i)
        if (ir->instructions[i].result == value) return &ir->instructions[i];
    return NULL;
}
static XL6_IrBlockId instructionBlock(const XL6_IrModule* ir, size_t index)
{
    size_t i;
    for (i = 0; i < ir->blockCount; ++i)
        if (index >= ir->blocks[i].firstInstruction &&
            index - ir->blocks[i].firstInstruction < ir->blocks[i].instructionCount)
            return (XL6_IrBlockId)(i + 1u);
    return 0;
}
static int reachable(const XL6_IrModule* ir, XL6_IrBlockId start, XL6_IrBlockId target)
{
    unsigned char* visited = (unsigned char*)xrtCalloc(ir->blockCount, 1);
    XL6_IrBlockId* stack = (XL6_IrBlockId*)xrtMalloc(ir->blockCount * sizeof(*stack));
    size_t count = 0; int found = 0;
    assert(visited && stack && start && start <= ir->blockCount);
    visited[start - 1u] = 1; stack[count++] = start;
    while (count && !found) {
        XL6_IrBlockId id = stack[--count];
        const XL6_IrBlock* block = &ir->blocks[id - 1u];
        const XL6_IrInstruction* last;
        XL6_IrBlockId edges[2]; size_t edge;
        if (id == target) { found = 1; break; }
        if (block->instructionCount == 0) continue;
        last = &ir->instructions[block->firstInstruction + block->instructionCount - 1u];
        edges[0] = last->trueTarget; edges[1] = last->falseTarget;
        for (edge = 0; edge < 2; ++edge) {
            XL6_IrBlockId next = edges[edge];
            if (next && next <= ir->blockCount && !visited[next - 1u]) {
                visited[next - 1u] = 1; stack[count++] = next;
            }
        }
    }
    xrtFree(stack); xrtFree(visited); return found;
}
static void reject(const XL6_IrModule* ir)
{
    XL6_DiagList diags = {0}; XL6_Output out; char* text; size_t i; int found = 0;
    assert(!xl6IrVerifyValueLifetimes(ir, &diags));
    xl6DiagFree(&diags);
    assert(!xl6IrVerifyModule(ir, &diags) && diags.count != 0);
    for (i = 0; i < diags.count; ++i)
        if (strstr(diags.items[i].message, "XLIR value lifetime:") != NULL) found = 1;
    if (!found) for (i = 0; i < diags.count; ++i)
        fprintf(stderr, "conditional mutation %llu: %s\n", (unsigned long long)mutations, diags.items[i].message);
    assert(found);
    assert(xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(text == NULL || *text == '\0'); xrtFree(text);
    assert(xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitHeader(&out, ir, "BAD_CONDITIONAL", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(text == NULL || *text == '\0'); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
static void emitPositiveFixture(const char* source, const char* generated)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile(source, &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "conditional_index_positive"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    assert(xl6IrLowerModule(&ir, &parse.diags) && xl6IrVerifyModule(&ir, &parse.diags));
    assert(xl6OutputOpenFile(&out, generated));
    assert(xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
}
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    size_t i, j, conditionalCount = 0; XL6_SymbolId pending = 0;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/xlir_index_conditional_assign.xl", &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "conditional_index"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    assert(xl6IrLowerModule(&ir, &parse.diags) && xl6IrVerifyModule(&ir, &parse.diags));
    for (i = 0; i < ir.instructionCount; ++i) {
        const XL6_IrInstruction* test = &ir.instructions[i];
        const XL6_IrNode* origin = xl6IrNodeGet(&ir, test->origin);
        const XL6_IrInstruction *read, *receiver, *acquisition = NULL;
        XL6_IrInstruction *branch = NULL, *write = NULL, *release = NULL;
        size_t writeIndex = 0, currentUses = 0;
        if (test->kind != XL6_IR_INSTR_NULL_TEST || origin == NULL ||
            origin->kind != XL6_IR_NODE_ASSIGN || origin->text == NULL || strcmp(origin->text, "?=") != 0)
            continue;
        assert(test->operandCount == 1);
        read = definition(&ir, ir.operands[test->firstOperand]);
        assert(read && read->kind == XL6_IR_INSTR_RUNTIME_CALL && read->operandCount >= 1);
        assert(xl6IrValueOwnershipGet(&ir, read->result) == XL6_IR_OWNERSHIP_BORROW);
        receiver = definition(&ir, ir.operands[read->firstOperand]);
        assert(receiver && receiver->kind == XL6_IR_INSTR_LOAD);
        assert(xl6IrStorageGet(&ir, receiver->targetSymbol)->kind == XL6_IR_STORAGE_TEMPORARY);
        for (j = 0; j < ir.instructionCount; ++j) {
            XL6_IrInstruction* x = &ir.instructions[j]; size_t operand;
            for (operand = 0; operand < x->operandCount; ++operand)
                if (ir.operands[x->firstOperand + operand] == read->result) ++currentUses;
            if (x->kind == XL6_IR_INSTR_STORE && x->targetSymbol == receiver->targetSymbol) acquisition = x;
            if (x->origin != test->origin) continue;
            if (x->kind == XL6_IR_INSTR_BRANCH && x->operandCount == 1 &&
                ir.operands[x->firstOperand] == test->result) { assert(branch == NULL); branch = x; }
            if (x->kind == XL6_IR_INSTR_RUNTIME_CALL && x->operandCount >= 2 &&
                xl6IrRuntimeOperandOwnership(x->runtimeOp, x->operandCount - 1) == XL6_IR_OPERAND_OWNERSHIP_CONSUME &&
                ir.operands[x->firstOperand] == receiver->result) { assert(write == NULL); write = x; writeIndex = j; }
        }
        assert(branch && write && acquisition && currentUses == 1);
        assert(reachable(&ir, branch->trueTarget, instructionBlock(&ir, writeIndex)));
        assert(!reachable(&ir, branch->falseTarget, instructionBlock(&ir, writeIndex)));
        if (read->operandCount == 2) assert(ir.operands[read->firstOperand + 1] == ir.operands[write->firstOperand + 1]);
        for (j = 0; j < ir.instructionCount; ++j) {
            XL6_IrInstruction* x = &ir.instructions[j];
            if (x->kind == XL6_IR_INSTR_RELEASE && x->targetSymbol == receiver->targetSymbol &&
                instructionBlock(&ir, j) == branch->falseTarget) { assert(release == NULL); release = x; }
        }
        assert(release);
        /* The old owned SSA was transferred into address storage before the
         * read/RHS. It cannot replace the protected LOAD at the commit. */
        {
            XL6_IrValueId saved = ir.operands[write->firstOperand];
            ir.operands[write->firstOperand] = ir.operands[acquisition->firstOperand]; reject(&ir);
            ir.operands[write->firstOperand] = saved;
        }
        /* Missing merge cleanup must independently fail storage activation,
         * even though the failure routes still clean up the address. */
        {
            XL6_IrInstruction saved = *release;
            release->kind = XL6_IR_INSTR_DISCARD; release->targetSymbol = 0;
            release->type = test->type; release->firstOperand = branch->firstOperand;
            release->operandCount = 1;
            reject(&ir); *release = saved;
        }
        ++conditionalCount;
    }
    assert(conditionalCount == 15 && mutations == 30 && xl6IrVerifyModule(&ir, &parse.diags));
    assert(argc == 4 && xl6OutputOpenFile(&out, argv[1]));
    assert(xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]));
    assert(xl6IrCgenEmitHeader(&out, &ir, "XLIR_CONDITIONAL_INDEX", &parse.diags)); xl6OutputClose(&out);
    for (i = 0; i < ir.storageCount; ++i) {
        const XL6_IrStorage* storage = &ir.storages[i];
        const XL6_IrType* type = xl6IrTypeGet(&ir.types, storage->type);
        if (storage->kind == XL6_IR_STORAGE_GLOBAL && type && type->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR) {
            assert(pending == 0); pending = storage->symbol;
        }
    }
    assert(pending && xl6OutputOpenFile(&out, argv[3]));
    xl6OutputPrintf(&out, "#define CONDITIONAL_PENDING s%u\n", (unsigned int)pending); xl6OutputClose(&out);
    printf("conditional index: %llu lazy address transactions, %llu ownership/merge mutations rejected before C/header output\n",
        (unsigned long long)conditionalCount, (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    emitPositiveFixture("test/cases/xlir_nullable_index_assign_if_null.xl",
                        "build/test/xlir_nullable_index_assign_if_null_generated.c");
    emitPositiveFixture("test/cases/xlir_nullable_index_assign_if_null_may_raise.xl",
                        "build/test/xlir_nullable_index_assign_if_null_may_raise_generated.c");
    return 0;
}
