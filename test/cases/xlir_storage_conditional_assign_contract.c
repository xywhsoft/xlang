#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "xlir_storage_exchange_checks.h"

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
    XL6_DiagList diags = {0}; XL6_Output out; char* text;
    assert(!xl6IrVerifyModule(ir, &diags) && diags.count);
    assert(xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(text == NULL || *text == '\0'); xrtFree(text);
    assert(xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitHeader(&out, ir, "BAD_STORAGE", &diags));
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

static void checkGeneratorBoundary(void)
{
    const char* sources[] = {
        "func outer() as generator<int>\n"
        "auto inner = func() as int\nyield 1\nreturn 2\nend\nyield inner()\nend\n",
        "func outer() as generator<int>\n"
        "auto inner = func() as int\nreturn 2\nend\nreturn inner()\nend\n"
    };
    const char* expected[] = {"yield 必须位于", "generator 函数中的 return"};
    size_t i, j;
    for (i = 0; i < sizeof(sources) / sizeof(sources[0]); ++i) {
        XL6_ParseResult parse; int found = 0;
        xl6ParseResultInit(&parse);
        assert(xl6ParseSource(sources[i], "cell-generator-boundary.xl", &parse));
        assert(!xl6SemCheckModule(parse.root, &parse.diags, "cell_generator_boundary"));
        for (j = 0; j < parse.diags.count; ++j)
            if (strstr(parse.diags.items[j].message, expected[i])) found = 1;
        assert(found); xl6ParseResultFree(&parse);
    }
    puts("conditional storage: nested ordinary callable and outer generator yield/return boundaries rejected independently");
}
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    size_t i, j, transactions = 0, cells = 0, managed = 0, tagged = 0, cellFrames = 0;
    XL6_SymbolId pending = 0;
    checkGeneratorBoundary();
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/xlir_storage_conditional_assign.xl", &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "conditional_storage"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    assert(xl6IrLowerModule(&ir, &parse.diags) && xl6IrVerifyModule(&ir, &parse.diags));
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* test = &ir.instructions[i];
        const XL6_IrNode* origin = xl6IrNodeGet(&ir, test->origin);
        const XL6_IrInstruction* read;
        const XL6_IrStorage* storage;
        const XL6_IrType* type;
        XL6_IrInstruction *branch = NULL, *write = NULL, *release = NULL;
        size_t writeIndex = 0, currentUses = 0;
        if (test->kind != XL6_IR_INSTR_NULL_TEST || !origin ||
            origin->kind != XL6_IR_NODE_ASSIGN || origin->opcode != XL6_IR_OPCODE_NULL_COALESCE) continue;
        read = definition(&ir, ir.operands[test->firstOperand]);
        assert(read && (read->kind == XL6_IR_INSTR_LOAD || read->kind == XL6_IR_INSTR_CELL_LOAD));
        storage = xl6IrStorageGet(&ir, read->targetSymbol);
        type = xl6IrTypeGet(&ir.types, read->type);
        assert(storage && type && storage->type == read->type);
        for (j = 0; j < ir.instructionCount; ++j) {
            XL6_IrInstruction* x = &ir.instructions[j]; size_t operand;
            for (operand = 0; operand < x->operandCount; ++operand)
                if (ir.operands[x->firstOperand + operand] == read->result) ++currentUses;
            if (x->origin != test->origin) continue;
            if (x->kind == XL6_IR_INSTR_BRANCH && x->operandCount == 1 &&
                ir.operands[x->firstOperand] == test->result) { assert(!branch); branch = x; }
            if (x->targetSymbol != storage->symbol) continue;
            if (x->kind == XL6_IR_INSTR_STORE || x->kind == XL6_IR_INSTR_CELL_STORE ||
                x->kind == XL6_IR_INSTR_CELL_EXCHANGE) {
                assert(!write); write = x; writeIndex = j;
            }
            if (x->kind == XL6_IR_INSTR_RELEASE || x->kind == XL6_IR_INSTR_CELL_RELEASE) {
                assert(!release); release = x;
            }
        }
        assert(branch && write && currentUses == 1 && write->type == storage->type);
        assert(reachable(&ir, branch->trueTarget, instructionBlock(&ir, writeIndex)));
        assert(!reachable(&ir, branch->falseTarget, instructionBlock(&ir, writeIndex)));
        assert(!release); /* Destruction targets the detached old-owner slot. */
        checkStorageReplacement(&ir, write);
        if (storage->cellType) { assert(read->kind == XL6_IR_INSTR_CELL_LOAD); ++cells; }
        if (type->nullAbi == XL6_IR_NULL_ABI_TAGGED_VALUE) ++tagged;
        {
            XL6_IrTypeId saved = test->type;
            test->type = read->type; reject(&ir); test->type = saved;
            saved = write->type; write->type = test->type; reject(&ir); write->type = saved;
        }
        if (storage->cellType) {
            XL6_IrInstructionKind saved = write->kind;
            write->kind = XL6_IR_INSTR_STORE; reject(&ir); write->kind = saved;
        }
        if (type->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED) {
            XL6_IrValueId saved = ir.operands[write->firstOperand];
            ir.operands[write->firstOperand] = read->result; reject(&ir);
            ir.operands[write->firstOperand] = saved; ++managed;
        }
        ++transactions;
    }
    for (i = 0; i < ir.asyncMachineFrameSlotCount + ir.generatorFrameSlotCount; ++i) {
        XL6_IrContinuationFrameSlot* frame = i < ir.asyncMachineFrameSlotCount
            ? &ir.asyncMachineFrameSlots[i] : &ir.generatorFrameSlots[i - ir.asyncMachineFrameSlotCount];
        const XL6_IrType* type = xl6IrTypeGet(&ir.types, frame->type);
        XL6_IrContinuationFrameSlot saved;
        if (!type || type->cAbi != XL6_IR_C_ABI_RUNTIME_CELL_PTR) continue;
        assert(frame->source == XL6_IR_ASYNC_SPILL_SOURCE_STORAGE &&
            frame->initBit != SIZE_MAX && frame->destroyAbi == XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_CELL);
        saved = *frame;
        frame->initBit = SIZE_MAX; reject(&ir); *frame = saved;
        frame->destroyAbi = XL6_IR_ASYNC_FRAME_DESTROY_NONE; reject(&ir); *frame = saved;
        frame->type = type->params[0]; reject(&ir); *frame = saved;
        ++cellFrames;
    }
    assert(transactions == 49 && cells == 21 && tagged == 21 && managed == 26 && cellFrames == 6 &&
        xl6IrVerifyModule(&ir, &parse.diags));
    assert(argc == 4 && xl6OutputOpenFile(&out, argv[1]));
    assert(xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]));
    assert(xl6IrCgenEmitHeader(&out, &ir, "XLIR_STORAGE_CONDITIONAL", &parse.diags)); xl6OutputClose(&out);
    for (i = 0; i < ir.storageCount; ++i) {
        const XL6_IrStorage* storage = &ir.storages[i];
        const XL6_IrType* type = xl6IrTypeGet(&ir.types, storage->type);
        if (storage->kind == XL6_IR_STORAGE_GLOBAL && type->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR) {
            assert(!pending); pending = storage->symbol;
        }
    }
    assert(pending && xl6OutputOpenFile(&out, argv[3]));
    xl6OutputPrintf(&out, "#define STORAGE_PENDING s%u\n", (unsigned int)pending); xl6OutputClose(&out);
    printf("conditional storage: %llu transactions (%llu cell, %llu tagged, %llu managed), %llu ABI/ownership mutations rejected before C/header output\n",
        (unsigned long long)transactions, (unsigned long long)cells, (unsigned long long)tagged,
        (unsigned long long)managed, (unsigned long long)mutations);
    printf("conditional storage: %llu async/generator cell frames verified with init/type/destroy mutations\n",
        (unsigned long long)cellFrames);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
#ifndef XLIR_STORAGE_SKIP_EXTRA_OUTPUTS
    emitPositiveFixture("test/cases/xlir_nullable_storage_assign_if_null.xl",
        "build/test/xlir_nullable_storage_assign_if_null_generated.c");
    emitPositiveFixture("test/cases/xlir_nullable_sentinel_assign_if_null.xl",
        "build/test/xlir_nullable_sentinel_assign_if_null_generated.c");
#endif
    return 0;
}
