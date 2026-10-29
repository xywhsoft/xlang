#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t mutations;
static void reject(XL6_IrModule* ir, const char* label)
{
    XL6_DiagList diags = {0}; XL6_Output out; char* text;
    if (xl6IrVerifyModule(ir, &diags)) {
        fprintf(stderr, "frame loan mutation escaped: %s\n", label); assert(0);
    }
    assert(diags.count && xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    assert(xl6OutputOpenMemory(&out) && !xl6IrCgenEmitHeader(&out, ir, "BAD_LOAN", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
static void checkConstructionEffects(void)
{
    XL6_ParseResult parse; XL6_IrModule ir; size_t i, checked = 0;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/xlir_managed_storage_inputs.xl", &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "construction_effect"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    for (i = 0; i < ir.nodeCount; ++i) {
        XL6_IrNode* node = &ir.nodes[i]; XL6_DiagList diags = {0};
        if (node->constructionErrorAbi != XL6_IR_ERROR_ABI_MAY_RAISE) continue;
        assert(xl6IrNodeRequiresThrowExitPlan(&ir, (XL6_IrNodeId)(i + 1)));
        node->constructionErrorAbi = XL6_IR_ERROR_ABI_INVALID;
        assert(!xl6IrVerifyModule(&ir, &diags) && diags.count);
        node->constructionErrorAbi = XL6_IR_ERROR_ABI_MAY_RAISE;
        xl6DiagFree(&diags); ++checked;
    }
    assert(checked >= 2 && xl6IrLowerModule(&ir, &parse.diags) && xl6IrVerifyModule(&ir, &parse.diags));
    printf("construction effects: %llu independent Typed insertion effects rejected when removed; nested object lowering passed\n",
        (unsigned long long)checked);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
}
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    size_t i, m, loans = 0, awaits = 0, takes = 0, writes = 0;
    XL6_SymbolId pending = 0;
    char binding[1024]; FILE* file;
    assert(argc == 4);
    checkConstructionEffects();
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/xlir_product_await_owner.xl", &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "product_await_owner"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    assert(xl6IrLowerModule(&ir, &parse.diags) && xl6IrVerifyModule(&ir, &parse.diags));
    for (m = 0; m < ir.asyncMachineCount; ++m) {
        XL6_IrAsyncMachine* machine = &ir.asyncMachines[m];
        if (!machine->suspensionCount) continue;
        /* Both Future parameters now belong to the queued root at birth;
         * the first is used before the first suspension but cannot be borrowed
         * from a caller which has already returned. Three local owners remain. */
        assert(machine->suspensionCount == 2 && machine->initBitCount == 5);
        size_t parameters=0;
        for(i=0;i<machine->frameSlotCount;++i){
            const XL6_IrAsyncMachineFrameSlot* frame=&ir.asyncMachineFrameSlots[machine->firstFrameSlot+i];
            if(frame->residence==XL6_IR_ASYNC_FRAME_RESIDENCE_LAUNCH_PARAMETER){
                assert(frame->parameter<2&&frame->transferAbi==XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_FUTURE&&
                    frame->destroyAbi==XL6_IR_ASYNC_FRAME_DESTROY_DESTROY_FUTURE&&frame->initBit<machine->initBitCount);
                ++parameters;
            }
        }
        assert(parameters==2);
        awaits += machine->suspensionCount;
        for (i = machine->firstFrameSlot; i < machine->firstFrameSlot + machine->frameSlotCount; ++i) {
            XL6_IrAsyncMachineFrameSlot *frame = &ir.asyncMachineFrameSlots[i], saved;
            XL6_IrAsyncMachineFrameSlot *root, rootSaved;
            size_t state;
            if (frame->transferAbi != XL6_IR_ASYNC_FRAME_TRANSFER_BORROW_STORAGE) continue;
            ++loans; saved = *frame;
            assert(frame->destroyAbi == XL6_IR_ASYNC_FRAME_DESTROY_NONE && frame->initBit == SIZE_MAX);
            assert(frame->borrowFrameSlot != i);
            assert(frame->borrowFrameSlot >= machine->firstFrameSlot &&
                frame->borrowFrameSlot < machine->firstFrameSlot + machine->frameSlotCount);
            root = &ir.asyncMachineFrameSlots[frame->borrowFrameSlot]; rootSaved = *root;
            assert(root->source == XL6_IR_ASYNC_SPILL_SOURCE_STORAGE &&
                root->destroyAbi == XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_XVALUE);
            frame->borrowFrameSlot = SIZE_MAX; reject(&ir, "missing owner"); *frame = saved;
            frame->borrowFrameSlot = i; reject(&ir, "self protection"); *frame = saved;
            frame->borrowFrameSlot = ir.asyncMachineFrameSlotCount; reject(&ir, "outside arena"); *frame = saved;
            frame->borrowFrameSlot = machine->firstFrameSlot; reject(&ir, "other SSA is not storage"); *frame = saved;
            frame->destroyAbi = XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_XVALUE; reject(&ir, "loan destructor"); *frame = saved;
            frame->initBit = 0; reject(&ir, "loan owns initialization bit"); *frame = saved;
            frame->transferAbi = XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_XVALUE; reject(&ir, "hidden retained owner"); *frame = saved;
            frame->transferAbi = XL6_IR_ASYNC_FRAME_TRANSFER_COPY_VALUE; reject(&ir, "managed trivial copy"); *frame = saved;
            {
                XL6_IrAsyncFrameAction* action = &ir.asyncFrameActions[machine->firstAction];
                XL6_IrAsyncFrameAction actionSaved = *action;
                action->frameSlot = i; action->value = frame->value;
                action->kind = XL6_IR_ASYNC_FRAME_ACTION_TRANSFER_BEFORE_INSTRUCTION;
                action->instruction = ir.functions[machine->function].firstInstruction;
                reject(&ir, "loan consumption"); *action = actionSaved;
            }
            frame->storageSymbol = root->storageSymbol; reject(&ir, "ambiguous source identity"); *frame = saved;
            root->destroyAbi = XL6_IR_ASYNC_FRAME_DESTROY_NONE; reject(&ir, "nonowning protection source"); *root = rootSaved;
            for (state = 0; state < machine->suspensionCount; ++state) {
                XL6_IrAsyncSuspension* suspension = &ir.asyncSuspensions[machine->firstSuspension + state];
                size_t l, rootMembership = SIZE_MAX; int live = 0;
                for (l = suspension->firstLiveSlot; l < suspension->firstLiveSlot + suspension->liveSlotCount; ++l) {
                    if (ir.asyncLiveSlots[l].frameSlot == i) live = 1;
                    if (ir.asyncLiveSlots[l].frameSlot == saved.borrowFrameSlot) rootMembership = l;
                }
                if (!live) continue;
                assert(rootMembership != SIZE_MAX);
                ir.asyncLiveSlots[rootMembership].frameSlot = i;
                reject(&ir, "owner absent at live suspension");
                ir.asyncLiveSlots[rootMembership].frameSlot = saved.borrowFrameSlot;
            }
        }
    }
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i];
        if (instruction->kind == XL6_IR_INSTR_PRODUCT_TAKE) ++takes;
        if (instruction->kind == XL6_IR_INSTR_RUNTIME_CALL &&
            instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_ARRAY_SET_I64_NEW) {
            XL6_IrInstruction saved;
            ++writes;
            assert(xl6IrCallInstructionErrorPhysicalAbi(&ir, instruction) == XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE);
            assert(i + 1 < ir.instructionCount && ir.instructions[i + 1].kind == XL6_IR_INSTR_ERROR_TEST);
            saved = ir.instructions[i + 1]; ir.instructions[i + 1].operandCount = 0;
            reject(&ir, "unchecked replacement callback outcome"); ir.instructions[i + 1] = saved;
        }
    }
    for (i = 0; i < ir.storageCount; ++i) {
        const XL6_IrStorage* storage = &ir.storages[i];
        const XL6_IrType* type = xl6IrTypeGet(&ir.types, storage->type);
        if (storage->kind == XL6_IR_STORAGE_GLOBAL && type && type->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR) {
            assert(!pending); pending = storage->symbol;
        }
    }
    assert(awaits == 2 && loans == 4 && takes == 2 && writes == 2 && pending && mutations >= 50);
    assert(xl6IrVerifyModule(&ir, &parse.diags));
    assert(xl6OutputOpenFile(&out, argv[1]) && xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitHeader(&out, &ir, "PRODUCT_AWAIT_OWNER", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); assert(!xl6OutputHasFailed(&out)); xl6OutputClose(&out);
    assert(snprintf(binding, sizeof(binding), "%s.bind.h", argv[2]) > 0);
    file = fopen(binding, "wb"); assert(file);
    fprintf(file, "/* Exact pending Future storage resolved from verified XLIR. */\n#define PRODUCT_AWAIT_PENDING s%u\n", (unsigned)pending);
    assert(!fclose(file));
    printf("product await owner: %llu protected frame loans, %llu real awaits, %llu invalid-IR mutations rejected before C/header output\n",
        (unsigned long long)loans, (unsigned long long)awaits, (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
