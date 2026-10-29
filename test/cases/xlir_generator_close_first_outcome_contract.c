#include "xlir_continuation_owner_test.h"

int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    size_t i; XL6_SymbolId start = 0; char binding[1024];
    assert(argc == 4); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_generator_close_first_outcome.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "generator_close_first_outcome") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) || !xl6IrLowerModule(&ir, &parse.diags)) {
        for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d %s\n",
            parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
        return 2;
    }
    verify(&ir); assert(ir.generatorMachineCount == 1);
    for (i = 0; i < ir.callTargetCount; ++i) {
        if (ir.callTargets[i].kind != XL6_IR_CALL_TARGET_INTERNAL_GENERATOR) continue;
        assert(!start && ir.callTargets[i].parameterCount == 1); start = ir.callTargets[i].symbol;
    }
    assert(start && ir.generatorMachines[0].closeAbi == XL6_IR_GENERATOR_CLOSE_UNWIND_DRAIN_FINALLY);
    ir.generatorMachines[0].closeAbi = XL6_IR_GENERATOR_CLOSE_INVALID;
    reject(&ir, NULL); ir.generatorMachines[0].closeAbi = XL6_IR_GENERATOR_CLOSE_UNWIND_DRAIN_FINALLY;
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i];
        XL6_IrGeneratorClosePointAbi saved = instruction->generatorClosePointAbi;
        if (instruction->kind != XL6_IR_INSTR_YIELD && instruction->kind != XL6_IR_INSTR_GENERATOR_CLOSE_TEST) continue;
        instruction->generatorClosePointAbi = XL6_IR_GENERATOR_CLOSE_POINT_INVALID;
        reject(&ir, NULL); instruction->generatorClosePointAbi = saved;
    }
    verify(&ir);
    assert(xl6OutputOpenFile(&out, argv[1]) && xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitHeader(&out, &ir, "GENERATOR_CLOSE_FIRST_OUTCOME", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir);
    assert(!xl6OutputHasFailed(&out)); xl6OutputClose(&out);
    assert(snprintf(binding, sizeof(binding), "%s.bind.h", argv[2]) > 0 && xl6OutputOpenFile(&out, binding));
    xl6OutputPrintf(&out, "#define CLOSE_OUTCOME_START xlir_generator_start_%u\n", start); xl6OutputClose(&out);
    printf("close first outcome: %llu C/header-gated mutations; direct close host bound to verified generator target\n",
        (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
