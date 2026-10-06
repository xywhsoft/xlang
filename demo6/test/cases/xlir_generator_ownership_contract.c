#include "xlir_continuation_owner_test.h"
#include "xlir_activation_capture_assert.h"
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; char* text;
    size_t items = 0, slots = 0, iterators = 0, generators = 0, consumers = 0; unsigned kinds = 0;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_generator_ownership.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "generator_ownership") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) || !xl6IrLowerModule(&ir, &parse.diags)) {
        for (size_t i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
        for (size_t i = 1; i <= ir.types.count; ++i) {
            const XL6_IrType* t = xl6IrTypeGet(&ir.types, (XL6_IrTypeId)i);
            if (t && t->cAbi == XL6_IR_C_ABI_RUNTIME_ERROR_PTR)
                fprintf(stderr, "error type t%zu kind=%d source=%u abi=%u identity=%llu null=%d name=%s\n",
                    i, t->kind, (unsigned)t->sourceTypeId, (unsigned)t->abiId,
                    (unsigned long long)t->typeValueId, t->nullAbi, t->name ? t->name : "");
        }
        return 1;
    }
    verify(&ir);
    {
        size_t errorTypes = 0;
        for (size_t i = 1; i <= ir.types.count; ++i) {
            const XL6_IrType* t = xl6IrTypeGet(&ir.types, (XL6_IrTypeId)i);
            if (t && t->kind == XL6_IR_TYPE_BUILTIN && t->cAbi == XL6_IR_C_ABI_RUNTIME_ERROR_PTR) {
                assert(t->abiId == xl6TypeAbiIdText("xlang.error")); ++errorTypes;
            }
        }
        assert(errorTypes == 1); /* catch and declared error share one exact TypeId */
    }
    for (size_t i = 0; i < ir.generatorMachineCount; ++i) {
        XL6_IrGeneratorMachine* machine = &ir.generatorMachines[i];
        XL6_IrGeneratorMachine saved = *machine; ++items;
        kinds |= 1u << saved.itemTraceAbi;
        machine->itemTraceAbi = XL6_IR_FRAME_TRACE_INVALID; reject(&ir, "canonical function/target ABI"); *machine = saved;
        machine->itemTraceAbi = saved.itemTraceAbi == XL6_IR_FRAME_TRACE_NONE ? XL6_IR_FRAME_TRACE_XVALUE : XL6_IR_FRAME_TRACE_NONE;
        reject(&ir, "canonical function/target ABI"); *machine = saved;
        machine->itemTraceAbi = (XL6_IrContinuationFrameTraceAbi)99; reject(&ir, "canonical function/target ABI"); *machine = saved;
    }
    for (size_t family = 0; family < 2; ++family) {
        XL6_IrContinuationFrameSlot* frames = family ? ir.generatorFrameSlots : ir.asyncMachineFrameSlots;
        size_t count = family ? ir.generatorFrameSlotCount : ir.asyncMachineFrameSlotCount;
        for (size_t i = 0; i < count; ++i) {
            XL6_IrContinuationFrameSlot saved = frames[i]; ++slots;
            iterators += saved.traceAbi == XL6_IR_FRAME_TRACE_VALUE_ITERATOR;
            generators += saved.traceAbi == XL6_IR_FRAME_TRACE_GENERATOR;
            frames[i].traceAbi = XL6_IR_FRAME_TRACE_INVALID; reject(&ir, "source/lifetime ABI"); frames[i] = saved;
            frames[i].traceAbi = saved.traceAbi == XL6_IR_FRAME_TRACE_NONE ? XL6_IR_FRAME_TRACE_XVALUE : XL6_IR_FRAME_TRACE_NONE;
            reject(&ir, "source/lifetime ABI"); frames[i] = saved;
            frames[i].traceAbi = XL6_IR_FRAME_TRACE_OPAQUE; reject(&ir, "source/lifetime ABI"); frames[i] = saved;
        }
    }
    assert(items == 8 && iterators && generators && mutations == 3 * (slots + items));
    assert(kinds == ((1u << XL6_IR_FRAME_TRACE_NONE) | (1u << XL6_IR_FRAME_TRACE_XVALUE) |
        (1u << XL6_IR_FRAME_TRACE_FUTURE) | (1u << XL6_IR_FRAME_TRACE_CLOSURE) |
        (1u << XL6_IR_FRAME_TRACE_ERROR) | (1u << XL6_IR_FRAME_TRACE_GENERATOR)));
    for (size_t i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* x = &ir.instructions[i]; XL6_IrInstruction saved = *x;
        if (x->kind != XL6_IR_INSTR_GENERATOR_START && x->kind != XL6_IR_INSTR_GENERATOR_NEXT &&
            x->kind != XL6_IR_INSTR_GENERATOR_ITEM_GET && x->kind != XL6_IR_INSTR_GENERATOR_ITEM_TAKE) continue;
        ++consumers;
        x->operandCount = SIZE_MAX; reject(&ir, NULL); *x = saved;
        x->type = XL6_IR_TYPE_INVALID; reject(&ir, NULL); *x = saved;
        x->result = XL6_IR_VALUE_INVALID; reject(&ir, NULL); *x = saved;
    }
    assert(consumers && mutations == 3 * (slots + items + consumers));
    verify(&ir);
    assert(xl6OutputOpenMemory(&out));
    if (!xl6IrCgenEmitC(&out, &ir, &parse.diags)) {
        for (size_t i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
        return 1;
    }
    text = xl6OutputTakeMemoryText(&out, NULL);
    assert(text && strstr(text, "XL6_CaptureLayoutV1 xlir_generator_item_layout_") && strstr(text, "xl6ValueIteratorRuntimeV2Get()->Ownership") &&
        strstr(text, "__xl_runtime/xl6_value_iterator.h") && !strstr(text, "xlir_xvalue_iterator_ownership_ops") &&
        strstr(text, "XL6_CAPTURE_GENERATOR") && strstr(text, "XL6_CAPTURE_VALUE_ITERATOR"));
    for(size_t i=0;i<ir.asyncMachineCount;++i){const XL6_IrAsyncMachine* machine=&ir.asyncMachines[i];
        assert_activation_layout(text,"xlir_async_machine",machine->targetSymbol,
            machine->frameSlotCount?ir.asyncMachineFrameSlots+machine->firstFrameSlot:NULL,machine->frameSlotCount,machine->firstFrameSlot,
            machine->destroyActionCount?ir.asyncFrameActions+machine->firstDestroyAction:NULL,machine->destroyActionCount);}
    for(size_t i=0;i<ir.generatorMachineCount;++i){const XL6_IrGeneratorMachine* machine=&ir.generatorMachines[i];
        assert_activation_layout(text,"xlir_generator",machine->targetSymbol,
            machine->frameSlotCount?ir.generatorFrameSlots+machine->firstFrameSlot:NULL,machine->frameSlotCount,machine->firstFrameSlot,
            machine->destroyActionCount?ir.generatorFrameActions+machine->firstDestroyAction:NULL,machine->destroyActionCount);}
    xrtFree(text);
    assert(xl6OutputOpenMemory(&out)); xl6IrDumpModuleText(&out, &ir);
    text = xl6OutputTakeMemoryText(&out, NULL);
    assert(text && strstr(text, "generator-item-trace g") && strstr(text, " trace=8 destroy=destroy-value-iterator"));
    xrtFree(text);
    if (argc == 2) {
        const char* names[] = {"generatorGraphValues", "generatorGraphClosures", "generatorGraphFutures",
            "generatorGraphErrors", "generatorGraphNested", "generatorGraphStrings", "generatorGraphScalars", "generatorGraphForward"};
        size_t bindings = 0;
        assert(xl6OutputOpenFile(&out, argv[1]));
        for (size_t i = 0; i < ir.callTargetCount; ++i) {
            const XL6_IrCallTarget* target = &ir.callTargets[i];
            const XL6_IrNode* declaration = xl6IrNodeGet(&ir, target->declaration);
            const XL6_IrNode* name = declaration && declaration->childCount ? xl6IrNodeGet(&ir, ir.children[declaration->firstChild]) : NULL;
            if (target->kind != XL6_IR_CALL_TARGET_INTERNAL_GENERATOR || !name || !name->text) continue;
            for (size_t n = 0; n < 8; ++n) if (!strcmp(name->text, names[n])) {
                xl6OutputPrintf(&out, "#define GENERATOR_GRAPH_%lu xlir_generator_start_%u\n", (unsigned long)n, (unsigned)target->symbol); ++bindings;
            }
        }
        assert(bindings == 8); xl6OutputClose(&out);
    }
    printf("Generator ownership IR: %zu machines, %zu slots, %zu iterator slots, %zu generator slots, %zu consumer operations, %zu C/header-gated mutations, kinds=%u\n",
        items, slots, iterators, generators, consumers, mutations, kinds);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
