#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t mutations, reused, loans, sharedRejections;
static void verify(XL6_IrModule* ir)
{
    XL6_DiagList diags = {0}; size_t i; int valid = xl6IrVerifyModule(ir, &diags);
    if (!valid) for (i = 0; i < diags.count; ++i) fprintf(stderr, "%s\n", diags.items[i].message);
    assert(valid); xl6DiagFree(&diags);
}
static void reject(XL6_IrModule* ir, const char* label, int lifetime)
{
    XL6_DiagList diags = {0}; XL6_Output out; char* text; size_t i; int found = 0;
    if (xl6IrVerifyModule(ir, &diags)) { fprintf(stderr, "generator loan mutation escaped: %s\n", label); assert(0); }
    for (i = 0; i < diags.count; ++i)
        if (strstr(diags.items[i].message, "XLIR value lifetime:")) found = 1;
    assert(diags.count && (!lifetime || found));
    assert(xl6OutputOpenMemory(&out) && !xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    assert(xl6OutputOpenMemory(&out) && !xl6IrCgenEmitHeader(&out, ir, "BAD_GENERATOR_LOAN", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
static const XL6_IrInstruction* definition(const XL6_IrModule* ir, XL6_IrValueId value)
{
    size_t i;
    for (i = 0; i < ir->instructionCount; ++i) if (ir->instructions[i].result == value) return &ir->instructions[i];
    return NULL;
}
static void reuseObservations(XL6_IrModule* ir)
{
    size_t f, i;
    for (f = 0; f < ir->functionCount; ++f) {
        const XL6_IrFunction* function = &ir->functions[f];
        XL6_IrValueId first = 0; XL6_SymbolId symbol = 0; int yielded = 0;
        if (function->kind != XL6_IR_FUNCTION_GENERATOR_WORKER) continue;
        for (i = function->firstInstruction; i < function->firstInstruction + function->instructionCount; ++i) {
            XL6_IrInstruction* instruction = &ir->instructions[i];
            const XL6_IrInstruction* load; const XL6_IrStorage* storage; XL6_IrValueId saved;
            if (instruction->kind == XL6_IR_INSTR_YIELD) yielded = 1;
            if (instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ||
                (instruction->runtimeOp != XL6_IR_RUNTIME_STRING_COPY &&
                 instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_I64)) continue;
            assert(instruction->operandCount == 1);
            saved = ir->operands[instruction->firstOperand]; load = definition(ir, saved);
            assert(load && load->kind == XL6_IR_INSTR_LOAD);
            storage = xl6IrStorageGet(ir, load->targetSymbol); assert(storage);
            if (!first) { first = saved; symbol = storage->symbol; continue; }
            assert(yielded && storage->symbol == symbol && first != saved);
            ir->operands[instruction->firstOperand] = first;
            if (storage->kind == XL6_IR_STORAGE_GLOBAL) {
                XL6_DiagList diags = {0};
                assert(xl6IrBuildGeneratorMachines(ir, &diags));
                reject(ir, "shared observation reused after yield", 1); ++sharedRejections;
                ir->operands[instruction->firstOperand] = saved;
                assert(xl6IrBuildGeneratorMachines(ir, &diags)); xl6DiagFree(&diags);
            } else { assert(storage->kind == XL6_IR_STORAGE_PARAMETER); ++reused; }
        }
    }
    { XL6_DiagList diags = {0}; assert(xl6IrBuildGeneratorMachines(ir, &diags)); xl6DiagFree(&diags); }
    verify(ir);
}
static void checkFrames(XL6_IrModule* ir)
{
    size_t i, s;
    for (i = 0; i < ir->generatorFrameSlotCount; ++i) {
        XL6_IrContinuationFrameSlot* frame = &ir->generatorFrameSlots[i];
        XL6_IrContinuationFrameSlot saved, rootSaved; XL6_IrContinuationFrameSlot* root;
        if (frame->transferAbi != XL6_IR_ASYNC_FRAME_TRANSFER_BORROW_STORAGE) continue;
        ++loans; saved = *frame;
        assert(frame->borrowFrameSlot < ir->generatorFrameSlotCount && frame->initBit == SIZE_MAX &&
            frame->destroyAbi == XL6_IR_ASYNC_FRAME_DESTROY_NONE);
        root = &ir->generatorFrameSlots[frame->borrowFrameSlot]; rootSaved = *root;
        assert(root->storageSymbol && root->destroyAbi != XL6_IR_ASYNC_FRAME_DESTROY_NONE);
        frame->borrowFrameSlot = SIZE_MAX; reject(ir, "missing protection", 0); *frame = saved;
        frame->borrowFrameSlot = i; reject(ir, "self protection", 0); *frame = saved;
        frame->destroyAbi = root->destroyAbi; reject(ir, "loan destructor", 0); *frame = saved;
        frame->initBit = 0; reject(ir, "loan init bit", 0); *frame = saved;
        frame->transferAbi = XL6_IR_ASYNC_FRAME_TRANSFER_COPY_STRING; reject(ir, "hidden retained owner", 0); *frame = saved;
        {
            size_t m;
            for (m = 0; m < ir->generatorMachineCount; ++m) {
                const XL6_IrGeneratorMachine* machine = &ir->generatorMachines[m];
                if (i >= machine->firstFrameSlot && i - machine->firstFrameSlot < machine->frameSlotCount) break;
            }
            assert(m < ir->generatorMachineCount);
            XL6_IrContinuationFrameAction* action = &ir->generatorFrameActions[
                ir->generatorMachines[m].firstAction];
            XL6_IrContinuationFrameAction actionSaved = *action;
            action->frameSlot = i; action->value = frame->value;
            action->kind = XL6_IR_ASYNC_FRAME_ACTION_TRANSFER_BEFORE_INSTRUCTION;
            action->instruction = ir->functions[ir->generatorMachines[m].function].firstInstruction;
            reject(ir, "loan consumption", 0); *action = actionSaved;
        }
        root->destroyAbi = XL6_IR_ASYNC_FRAME_DESTROY_NONE; reject(ir, "nonowning root", 0); *root = rootSaved;
        for (s = 0; s < ir->generatorSuspensionCount; ++s) {
            const XL6_IrGeneratorSuspension* suspension = &ir->generatorSuspensions[s];
            size_t l, membership = SIZE_MAX; int live = 0;
            for (l = suspension->firstLiveSlot; l < suspension->firstLiveSlot + suspension->liveSlotCount; ++l) {
                if (ir->generatorLiveSlots[l].frameSlot == i) live = 1;
                if (ir->generatorLiveSlots[l].frameSlot == saved.borrowFrameSlot) membership = l;
            }
            if (!live) continue;
            assert(membership != SIZE_MAX);
            ir->generatorLiveSlots[membership].frameSlot = i;
            reject(ir, "root absent at suspension", 0);
            ir->generatorLiveSlots[membership].frameSlot = saved.borrowFrameSlot;
        }
    }
    assert(loans == 2 && reused == 4 && sharedRejections == 2); verify(ir);
}
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    assert(argc == 4 || (argc == 5 && strcmp(argv[4], "--baseline") == 0));
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/xlir_generator_frame_loan.xl", &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "generator_frame_loan"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    assert(xl6IrLowerModule(&ir, &parse.diags)); verify(&ir);
    if (argc == 4) { reuseObservations(&ir); checkFrames(&ir); }
    assert(xl6OutputOpenFile(&out, argv[1]) && xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitHeader(&out, &ir, "GENERATOR_FRAME_LOAN", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); assert(!xl6OutputHasFailed(&out)); xl6OutputClose(&out);
    printf("generator frame loan: %llu reused observations, %llu protected loans, %llu C/header-gated mutations (%llu shared-yield rejections)\n",
        (unsigned long long)reused, (unsigned long long)loans, (unsigned long long)mutations, (unsigned long long)sharedRejections);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
