#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned mutations;
static void reject(const XL6_IrModule* ir)
{
    XL6_DiagList diags = {0}; XL6_Output out; char* text;
    assert(!xl6IrVerifyModule(ir, &diags) && diags.count);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out, ir, "BAD_TERMINAL", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; size_t i, state, outcome;
    unsigned awaits = 0, nonAwaits = 0, entries = 0, bindings = 0;
    char bindingPath[4096]; int length;
    assert(argc == 4); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_async_terminal_outcome.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "terminal_outcome") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrVerifyModule(&ir, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    assert(XL6_IR_ASYNC_RESUME_ENTRY_COUNT == 4);
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i];
        if (instruction->kind == XL6_IR_INSTR_AWAIT) {
            assert(instruction->futureOutcomeAbi == XL6_IR_FUTURE_OUTCOME_XRT_ERROR_KIND);
            instruction->futureOutcomeAbi = XL6_IR_FUTURE_OUTCOME_INVALID; reject(&ir);
            instruction->futureOutcomeAbi = (XL6_IrFutureOutcomeAbi)99; reject(&ir);
            instruction->futureOutcomeAbi = XL6_IR_FUTURE_OUTCOME_XRT_ERROR_KIND; ++awaits;
        } else if (!nonAwaits) {
            assert(instruction->futureOutcomeAbi == XL6_IR_FUTURE_OUTCOME_INVALID);
            instruction->futureOutcomeAbi = XL6_IR_FUTURE_OUTCOME_XRT_ERROR_KIND; reject(&ir);
            instruction->futureOutcomeAbi = XL6_IR_FUTURE_OUTCOME_INVALID; ++nonAwaits;
        }
    }
    for (i = 0; i < ir.asyncMachineCount; ++i) {
        XL6_IrAsyncMachine *machine = &ir.asyncMachines[i], saved = *machine;
        assert(machine->futureOutcomeAbi == XL6_IR_FUTURE_OUTCOME_XRT_ERROR_KIND);
        assert(machine->entryCount == 1 + 4 * machine->suspensionCount);
        machine->futureOutcomeAbi = XL6_IR_FUTURE_OUTCOME_INVALID; reject(&ir); *machine = saved;
        machine->futureOutcomeAbi = (XL6_IrFutureOutcomeAbi)99; reject(&ir); *machine = saved;
        /* The old success/failure-only encoding must never be accepted. */
        machine->entryCount = 1 + 2 * machine->suspensionCount; reject(&ir); *machine = saved;
        for (state = 0; state < machine->suspensionCount; ++state) {
            size_t suspensionId = machine->firstSuspension + state;
            const XL6_IrAsyncSuspension* suspension = &ir.asyncSuspensions[suspensionId];
            size_t canonical = ir.asyncMachineEntries[machine->firstEntry + 1 + 4 * state].programBlock;
            for (outcome = 0; outcome < 4; ++outcome) {
                XL6_IrAsyncMachineEntry* entry = &ir.asyncMachineEntries[machine->firstEntry + 1 + 4 * state + outcome];
                XL6_IrAsyncMachineEntry original = *entry;
                assert(entry->kind == (XL6_IrAsyncMachineEntryKind)(XL6_IR_ASYNC_MACHINE_ENTRY_RESUME_SUCCESS + outcome));
                assert(entry->machine == i && entry->suspension == suspensionId);
                assert(entry->block == suspension->block && entry->firstInstruction == suspension->awaitInstruction + 1);
                assert(entry->programBlock == canonical && canonical < ir.asyncMachineBlockCount);
                entry->kind = XL6_IR_ASYNC_MACHINE_ENTRY_INVALID; reject(&ir); *entry = original;
                entry->kind = (XL6_IrAsyncMachineEntryKind)(XL6_IR_ASYNC_MACHINE_ENTRY_RESUME_SUCCESS + (outcome + 1) % 4); reject(&ir); *entry = original;
                entry->machine = ir.asyncMachineCount; reject(&ir); *entry = original;
                entry->suspension = ir.asyncSuspensionCount; reject(&ir); *entry = original;
                entry->block = 0; reject(&ir); *entry = original;
                entry->firstInstruction = suspension->awaitInstruction; reject(&ir); *entry = original;
                entry->programBlock = SIZE_MAX; reject(&ir); *entry = original;
                /* Skipping to ERROR_TEST or a terminal block would bypass
                 * source cleanup. All outcomes must resume exactly after AWAIT. */
                ++entry->firstInstruction; reject(&ir); *entry = original;
                ++entries;
            }
        }
    }
    assert(ir.asyncMachineCount == 2 && ir.asyncSuspensionCount == 10 && awaits == 11 && nonAwaits == 1 && entries == 40);
    /* Every projection/transfer carries exact source-independent identities.
     * In particular, unused instruction fields cannot hide a second owner. */
    for (i = 0; i < ir.asyncFrameActionCount; ++i) {
        XL6_IrAsyncFrameAction* action = &ir.asyncFrameActions[i];
        XL6_IrAsyncFrameAction saved = *action;
        action->value = saved.value ? 0 : ir.valueCount + 1; reject(&ir); *action = saved;
        action->instruction = saved.instruction == SIZE_MAX ? 0 : SIZE_MAX; reject(&ir); *action = saved;
        action->frameSlot = ir.asyncMachineFrameSlotCount; reject(&ir); *action = saved;
    }
    for (i = 0; i < ir.asyncLiveSlotCount; ++i) {
        XL6_IrAsyncLiveSlot* live = &ir.asyncLiveSlots[i];
        XL6_IrValueId saved = live->value;
        live->value = saved ? 0 : ir.valueCount + 1; reject(&ir); live->value = saved;
    }
    assert(xl6IrVerifyModule(&ir, &parse.diags));
    assert(xl6OutputOpenFile(&out, argv[1])); assert(xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2])); assert(xl6IrCgenEmitHeader(&out, &ir, "TERMINAL_OUTCOME", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    length = snprintf(bindingPath, sizeof(bindingPath), "%s.bind.h", argv[2]);
    assert(length > 0 && (size_t)length < sizeof(bindingPath) && xl6OutputOpenFile(&out, bindingPath));
    for (i = 0; i < ir.callTargetCount; ++i) {
        const XL6_IrCallTarget* target = &ir.callTargets[i];
        const XL6_IrNode* declaration = xl6IrNodeGet(&ir, target->declaration);
        const XL6_IrNode* name = declaration && declaration->childCount
            ? xl6IrNodeGet(&ir, ir.children[declaration->firstChild]) : NULL;
        if (target->kind != XL6_IR_CALL_TARGET_INTERNAL_ASYNC || !name || !name->text) continue;
        if (!strcmp(name->text, "terminalCase")) {
            xl6OutputPrintf(&out, "#define TERMINAL_START xlir_async_start_%u\n", (unsigned)target->symbol); ++bindings;
        } else if (!strcmp(name->text, "terminalAllReturn")) {
            xl6OutputPrintf(&out, "#define TERMINAL_ALL_RETURN xlir_async_start_%u\n", (unsigned)target->symbol); ++bindings;
        }
    }
    assert(bindings == 2); xl6OutputClose(&out);
    printf("terminal outcome contract: %u awaits, %u canonical resume entries, %u rejected mutations\n", awaits, entries, mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d: %s\n",
        parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
    abort();
}
