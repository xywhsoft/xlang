#include "xlir_continuation_owner_test.h"
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; size_t f, i, changed = 0; XL6_SymbolId target = 0;
    assert(argc == 4 || (argc == 5 && !strcmp(argv[4], "--single-view")));
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/xlir_continuation_owner_views.xl", &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "frame_alias"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    assert(xl6IrLowerModule(&ir, &parse.diags)); verify(&ir);
    for (f = 0; f < ir.functionCount; ++f) {
        XL6_IrFunction* fn = &ir.functions[f];
        size_t yields[3], copies[3], loads[3], n, count = 0;
        const XL6_IrType* type; XL6_IrValueId root, alias;
        if (fn->kind != XL6_IR_FUNCTION_GENERATOR_WORKER || fn->parameterCount != 1) continue;
        for (i = fn->firstInstruction; i < fn->firstInstruction + fn->instructionCount; ++i)
            if (ir.instructions[i].kind == XL6_IR_INSTR_YIELD) { assert(count < 3); yields[count++] = i; }
        assert(count == 3);
        for (n = 0; n < count; ++n) {
            copies[n] = syntheticDefinition(&ir, ir.operands[ir.instructions[yields[n]].firstOperand]);
            assert(ir.instructions[copies[n]].runtimeOp == XL6_IR_RUNTIME_STRING_COPY);
            loads[n] = syntheticDefinition(&ir, ir.operands[ir.instructions[copies[n]].firstOperand]);
            assert(ir.instructions[loads[n]].kind == XL6_IR_INSTR_LOAD);
        }
        type = xl6IrTypeGet(&ir.types, ir.instructions[copies[0]].type);
        if (!type || type->cAbi != XL6_IR_C_ABI_MANAGED_STRING) continue;
        root = ir.instructions[copies[0]].result; alias = ++ir.valueCount;
        /* Materialize a two-yield IR owner region: root and a transparent
         * alias both survive the first yield; the last yield consumes once. */
        asAlias(&ir, &ir.instructions[yields[0]], alias, root);
        for (n = 1; n < count; ++n)
            asAlias(&ir, &ir.instructions[loads[n]], ir.instructions[loads[n]].result, root);
        asAlias(&ir, &ir.instructions[copies[2]], ir.instructions[copies[2]].result, argc == 5 ? root : alias);
        syntheticRetireOutcome(&ir, copies[2]);
        syntheticCloseOwnerExits(&ir, fn, root);
        relocateOwnerViews(&ir, fn, alias);
        ++changed; target = fn->symbol;
    }
    assert(changed == 1 && xl6IrBuildGeneratorMachines(&ir, &parse.diags)); verify(&ir);
    checkOwnerViews(&ir, target, argc == 4);
    for (i = 0; i < ir.generatorFrameSlotCount; ++i) {
        const XL6_IrContinuationFrameSlot* frame = &ir.generatorFrameSlots[i];
        if (frame->source == XL6_IR_ASYNC_SPILL_SOURCE_VALUE)
            printf("frame%llu value%u transfer%d destroy%d bit%llu\n", (unsigned long long)i,
                frame->value, frame->transferAbi, frame->destroyAbi, (unsigned long long)frame->initBit);
    }
    assert(xl6OutputOpenFile(&out, argv[1]) && xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitHeader(&out, &ir, "FRAME_ALIAS", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    {
        char binding[1024]; assert(snprintf(binding, sizeof(binding), "%s.bind.h", argv[2]) > 0);
        assert(target && xl6OutputOpenFile(&out, binding));
        xl6OutputPrintf(&out, "#define OWNER_VIEWS_START xlir_generator_start_%u\n", target);
        xl6OutputClose(&out);
    }
    printf("continuation owner views: %s IR emitted for target %u, %llu invalid-IR mutations rejected\n",
        argc == 5 ? "single" : "aliased", target, (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
