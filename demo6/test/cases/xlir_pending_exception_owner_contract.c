#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t mutations, owners, restores, preserves, cancels, frames;
static void reject(XL6_IrModule* ir, const char* field)
{
    XL6_DiagList diags = {0};
    XL6_Output out;
    char* text;
    if (xl6IrVerifyModule(ir, &diags)) {
        fprintf(stderr, "pending exception owner mutation escaped: %s\n", field); assert(0);
    }
    assert(diags.count != 0 && xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL);
    assert(text == NULL || text[0] == '\0'); xrtFree(text);
    assert(xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitHeader(&out, ir, "REJECTED_PENDING_OWNER", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL);
    assert(text == NULL || text[0] == '\0'); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
#define MUTATE(Type, item, field, value) do { \
    Type saved = *(item); (item)->field = (value); \
    reject(ir, #item "." #field "=" #value); *(item) = saved; \
} while (0)
static void verify_owners(XL6_IrModule* ir)
{
    size_t i, j;
    assert(ir->pendingExceptionCount != 0);
    MUTATE(XL6_IrModule, ir, pendingExceptions, NULL);
    MUTATE(XL6_IrModule, ir, pendingExceptionCount, ir->pendingExceptionCapacity + 1);
    MUTATE(XL6_IrModule, ir, pendingExceptionCount, 0);
    /* Removing metadata and every instruction tag together must not bypass
     * the capture's declaration-owned lifetime proof. */
    {
        XL6_IrInstruction* saved = xrtMalloc(ir->instructionCount * sizeof(*saved));
        size_t count = ir->pendingExceptionCount;
        assert(saved != NULL); memcpy(saved, ir->instructions, ir->instructionCount * sizeof(*saved));
        for (i = 0; i < ir->instructionCount; ++i) {
            ir->instructions[i].pendingException = 0; ir->instructions[i].pendingFailure = 0;
        }
        ir->pendingExceptionCount = 0; reject(ir, "all owner metadata and tags removed");
        ir->pendingExceptionCount = count;
        memcpy(ir->instructions, saved, ir->instructionCount * sizeof(*saved)); xrtFree(saved);
    }
    for (i = 0; i < ir->pendingExceptionCount; ++i) {
        XL6_IrPendingException* owner = &ir->pendingExceptions[i];
        XL6_IrInstruction* capture = &ir->instructions[owner->capture - 1];
        XL6_IrInstruction* take = capture - 1;
        XL6_IrStorage* storage = NULL;
        ++owners;
        MUTATE(XL6_IrPendingException, owner, origin, owner->body);
        MUTATE(XL6_IrPendingException, owner, body, owner->origin);
        MUTATE(XL6_IrPendingException, owner, owner, 0);
        MUTATE(XL6_IrPendingException, owner, symbol, 0);
        MUTATE(XL6_IrPendingException, owner, type, 0);
        MUTATE(XL6_IrPendingException, owner, capture, 0);
        MUTATE(XL6_IrPendingException, owner, capture, owner->capture - 1);
        MUTATE(XL6_IrPendingException, owner, capture, ir->instructionCount + 1);
        MUTATE(XL6_IrPendingException, owner, outer, i + 1);
        MUTATE(XL6_IrPendingException, owner, outer, ir->pendingExceptionCount + 1);
        if (owner->outer != 0) MUTATE(XL6_IrPendingException, owner, outer, 0);
        if (i != 0) {
            MUTATE(XL6_IrPendingException, owner, symbol, ir->pendingExceptions[0].symbol);
            MUTATE(XL6_IrPendingException, owner, capture, ir->pendingExceptions[0].capture);
        }
        MUTATE(XL6_IrInstruction, capture, pendingException, 0);
        MUTATE(XL6_IrInstruction, capture, pendingFailure, take->result);
        MUTATE(XL6_IrInstruction, capture, targetSymbol, 0);
        MUTATE(XL6_IrInstruction, capture, origin, owner->body);
        MUTATE(XL6_IrInstruction, take, kind, XL6_IR_INSTR_NULL);
        MUTATE(XL6_IrInstruction, take, origin, owner->body);
        for (j = 0; j < ir->storageCount; ++j)
            if (ir->storages[j].symbol == owner->symbol) storage = &ir->storages[j];
        assert(storage != NULL);
        MUTATE(XL6_IrStorage, storage, kind, XL6_IR_STORAGE_TEMPORARY);
        MUTATE(XL6_IrStorage, storage, declaration, owner->body);
        MUTATE(XL6_IrStorage, storage, owner, 0);
        for (j = 0; j < ir->instructionCount; ++j) {
            XL6_IrInstruction* instruction = &ir->instructions[j];
            if (instruction->pendingException != i + 1 || instruction == capture) continue;
            MUTATE(XL6_IrInstruction, instruction, pendingException, 0);
            MUTATE(XL6_IrInstruction, instruction, pendingException, ir->pendingExceptionCount + 1);
            if (instruction->kind == XL6_IR_INSTR_MOVE) {
                XL6_IrInstruction *load = instruction - 1, *set = instruction + 1;
                XL6_IrValueId original = ir->operands[instruction->firstOperand];
                ++restores;
                MUTATE(XL6_IrInstruction, instruction, targetSymbol, 0);
                MUTATE(XL6_IrInstruction, load, kind, XL6_IR_INSTR_NULL);
                MUTATE(XL6_IrInstruction, set, kind, XL6_IR_INSTR_ERROR_CLEAR);
                ir->operands[instruction->firstOperand] = take->result;
                reject(ir, "restoration reuses captured SSA instead of active storage");
                ir->operands[instruction->firstOperand] = original;
                if (instruction->pendingFailure != 0) {
                    ++preserves;
                    MUTATE(XL6_IrInstruction, instruction, pendingFailure, 0);
                    MUTATE(XL6_IrInstruction, instruction, pendingFailure, take->result);
                    /* Keep the same instruction and block layout, replace
                     * transfer by a balanced cancellation. CFG lifetime alone
                     * is insufficient: the original outcome must still win. */
                    {
                        XL6_IrInstruction a = *load, b = *instruction, c = *set;
                        load->kind = XL6_IR_INSTR_NULL; load->targetSymbol = 0;
                        instruction->kind = XL6_IR_INSTR_RELEASE; instruction->operandCount = 0;
                        instruction->pendingFailure = 0; instruction->origin = owner->body;
                        set->kind = XL6_IR_INSTR_DISCARD;
                        reject(ir, "balanced cancellation instead of preserving primary outcome");
                        *load = a; *instruction = b; *set = c;
                    }
                } else MUTATE(XL6_IrInstruction, instruction, pendingFailure, take->result);
            } else if (instruction->kind == XL6_IR_INSTR_RELEASE) {
                ++cancels;
                MUTATE(XL6_IrInstruction, instruction, origin, owner->origin);
                MUTATE(XL6_IrInstruction, instruction, targetSymbol, 0);
                MUTATE(XL6_IrInstruction, instruction, pendingFailure, take->result);
            } else {
                assert(instruction->kind == XL6_IR_INSTR_CLEANUP_BEGIN);
                MUTATE(XL6_IrInstruction, instruction, exitStep, 0);
                MUTATE(XL6_IrInstruction, instruction, origin, owner->origin);
            }
        }
    }
}
static void verify_frames(XL6_IrModule* ir, XL6_IrContinuationFrameSlot* slots, size_t count)
{
    size_t i, j;
    for (i = 0; i < count; ++i) {
        XL6_IrContinuationFrameSlot* frame = &slots[i];
        for (j = 0; j < ir->pendingExceptionCount; ++j)
            if (ir->pendingExceptions[j].symbol == frame->storageSymbol) break;
        if (j == ir->pendingExceptionCount) continue;
        assert(frame->residence == XL6_IR_ASYNC_FRAME_RESIDENCE_FRAME_STORAGE &&
            frame->transferAbi == XL6_IR_ASYNC_FRAME_TRANSFER_NONE &&
            frame->destroyAbi == XL6_IR_ASYNC_FRAME_DESTROY_FREE_ERROR && frame->initBit != SIZE_MAX);
        ++frames;
        MUTATE(XL6_IrContinuationFrameSlot, frame, destroyAbi, XL6_IR_ASYNC_FRAME_DESTROY_NONE);
        MUTATE(XL6_IrContinuationFrameSlot, frame, destroyAbi, XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_XVALUE);
        MUTATE(XL6_IrContinuationFrameSlot, frame, transferAbi, XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_ERROR);
        MUTATE(XL6_IrContinuationFrameSlot, frame, initBit, SIZE_MAX);
        MUTATE(XL6_IrContinuationFrameSlot, frame, storageSymbol, 0);
        MUTATE(XL6_IrContinuationFrameSlot, frame, residence, XL6_IR_ASYNC_FRAME_RESIDENCE_SSA_SPILL);
    }
}
static int run(const char* path)
{
    XL6_ParseResult parse; XL6_IrModule ir; size_t i;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile(path, &parse) || !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "pending_owner_contract") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    verify_owners(&ir);
    verify_frames(&ir, ir.asyncMachineFrameSlots, ir.asyncMachineFrameSlotCount);
    verify_frames(&ir, ir.generatorFrameSlots, ir.generatorFrameSlotCount);
    assert(xl6IrVerifyModule(&ir, NULL));
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 1;
failed:
    for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
int main(void)
{
    if (!run("test/cases/xlir_pending_exception_outcome.xl") ||
        !run("test/cases/xlir_pending_exception_control.xl")) return 1;
    assert(owners == 19 && frames == 2 && preserves != 0 && cancels != 0);
    printf("pending exception owner contract: %lu owners, %lu restores (%lu cleanup preservation), %lu cancellations, %lu continuation error frames, %lu mutations rejected before C/header output\n",
        (unsigned long)owners, (unsigned long)restores, (unsigned long)preserves,
        (unsigned long)cancels, (unsigned long)frames, (unsigned long)mutations);
    return 0;
}
