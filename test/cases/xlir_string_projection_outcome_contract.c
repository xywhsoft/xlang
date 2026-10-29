#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>

static void reject(XL6_IrModule* ir)
{
    XL6_DiagList diags = {0};
    XL6_Output out;
    char* text;
    assert(!xl6IrVerifyModule(ir, &diags) && diags.count != 0);
    assert(xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL);
    assert(text == NULL || text[0] == '\0'); xrtFree(text);
    assert(xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitHeader(&out, ir, "REJECTED_PROJECTION", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL);
    assert(text == NULL || text[0] == '\0'); xrtFree(text);
    xl6DiagFree(&diags);
}

int main(int argc, char** argv)
{
    XL6_ParseResult parse;
    XL6_IrModule ir;
    size_t i, checked = 0;
    assert(argc == 2);
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile(argv[1], &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "projection_outcome") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) {
        for (i = 0; i < parse.diags.count; ++i) puts(parse.diags.items[i].message);
        return 1;
    }
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* source = &ir.instructions[i];
        XL6_IrInstruction* test;
        XL6_IrValueId saved;
        size_t j, observations = 0;
        if (source->kind != XL6_IR_INSTR_RUNTIME_CALL ||
            source->runtimeOp != XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_STRING_COPY) continue;
        assert(source->result != XL6_IR_VALUE_INVALID && i + 1 < ir.instructionCount);
        test = &ir.instructions[i + 1];
        assert(test->kind == XL6_IR_INSTR_ERROR_TEST && test->operandCount == 1 &&
            test->firstOperand < ir.operandCount && ir.operands[test->firstOperand] == source->result);
        for (j = 0; j < ir.instructionCount; ++j) {
            const XL6_IrInstruction* other = &ir.instructions[j];
            if (other->kind == XL6_IR_INSTR_ERROR_TEST && other->operandCount == 1 &&
                other->firstOperand < ir.operandCount &&
                ir.operands[other->firstOperand] == source->result) ++observations;
        }
        assert(observations == 1);
        saved = ir.operands[test->firstOperand];
        ir.operands[test->firstOperand] = XL6_IR_VALUE_INVALID;
        reject(&ir); ir.operands[test->firstOperand] = saved;
        ++checked;
    }
    assert(checked >= 2);
    printf("string projection outcomes: %zu immediate single observations; missing outcomes reject C and header\n", checked);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    return 0;
}
