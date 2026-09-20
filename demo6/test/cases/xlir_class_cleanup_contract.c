#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>

static size_t mutations;
static void reject(XL6_IrModule* ir, const char* field)
{
    XL6_DiagList diags = {0};
    XL6_Output out;
    char* text;
    if (xl6IrVerifyModule(ir, &diags)) {
        fprintf(stderr, "class finalizer mutation escaped verification: %s\n", field);
        assert(0);
    }
    assert(diags.count != 0 && xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL);
    assert(text == NULL || text[0] == '\0'); xrtFree(text);
    assert(xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitHeader(&out, ir, "REJECTED_CLASS_CLEANUP", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL);
    assert(text == NULL || text[0] == '\0'); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}

#define MUTATE(Type, item, field, value) do { \
    Type saved = *(item); (item)->field = (value); \
    reject(&ir, #item "." #field "=" #value); *(item) = saved; \
} while (0)

int main(void)
{
    XL6_ParseResult parse;
    XL6_IrModule ir;
    size_t i, j, programs = 0, steps = 0;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_class_cleanup_outcome.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "class_cleanup_contract") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) {
        for (i = 0; i < parse.diags.count; ++i) puts(parse.diags.items[i].message);
        return 1;
    }
    for (i = 0; i < ir.classLayoutCount; ++i) {
        XL6_IrClassLayout* layout = &ir.classLayouts[i];
        XL6_IrFunction* function;
        XL6_IrStorage* parameter;
        XL6_IrInstruction* body;
        size_t end;
        if (layout->destroyStepCount == 0) {
            MUTATE(XL6_IrClassLayout, layout, destroyFunction, 1);
            continue;
        }
        ++programs;
        function = &ir.functions[layout->destroyFunction - 1];
        parameter = (XL6_IrStorage*)xl6IrStorageGet(&ir, ir.functionParameters[function->firstParameter]);
        body = ir.instructions + function->firstInstruction;
        end = function->instructionCount - 4;
        assert(function->kind == XL6_IR_FUNCTION_CLASS_FINI && parameter != NULL);
        MUTATE(XL6_IrClassLayout, layout, destroyFunction, 0);
        MUTATE(XL6_IrClassLayout, layout, destroyFunction, (uint32)ir.functionCount + 1);
        MUTATE(XL6_IrClassLayout, layout, destroyStepCount, layout->destroyStepCount - 1);
        MUTATE(XL6_IrFunction, function, kind, XL6_IR_FUNCTION_MODULE_FINI);
        MUTATE(XL6_IrFunction, function, symbol, 0);
        MUTATE(XL6_IrFunction, function, symbol, ir.functions[0].symbol);
        MUTATE(XL6_IrFunction, function, errorAbi, XL6_IR_ERROR_ABI_NO_RAISE);
        MUTATE(XL6_IrFunction, function, parameterCount, 0);
        MUTATE(XL6_IrFunction, function, entryBlock, body[end + 1].trueTarget);
        MUTATE(XL6_IrFunction, function, entryBlock, body[end + 1].falseTarget);
        MUTATE(XL6_IrBlock, &ir.blocks[function->entryBlock - 1], firstInstruction, function->firstInstruction + 2);
        MUTATE(XL6_IrBlock, &ir.blocks[function->entryBlock - 1], instructionCount, 0);
        MUTATE(XL6_IrBlock, &ir.blocks[function->entryBlock - 1], instructionCount, 1);
        MUTATE(XL6_IrBlock, &ir.blocks[function->entryBlock - 1], instructionCount,
            ir.blocks[function->entryBlock - 1].instructionCount + 1);
        MUTATE(XL6_IrBlock, &ir.blocks[body[end + 1].trueTarget - 1], firstInstruction, function->firstInstruction + end + 2);
        MUTATE(XL6_IrFunction, function, firstParameter, ir.functionParameterCount);
        MUTATE(XL6_IrFunction, function, returnType, parameter->type);
        MUTATE(XL6_IrStorage, parameter, kind, XL6_IR_STORAGE_LOCAL);
        MUTATE(XL6_IrStorage, parameter, owner, 0);
        MUTATE(XL6_IrStorage, parameter, declaration, 0);
        MUTATE(XL6_IrInstruction, &body[0], targetSymbol, 0);
        MUTATE(XL6_IrInstruction, &body[0], kind, XL6_IR_INSTR_RETAIN);
        MUTATE(XL6_IrInstruction, &body[1], kind, XL6_IR_INSTR_CLEANUP_BEGIN);
        MUTATE(XL6_IrInstruction, &body[1], kind, XL6_IR_INSTR_ERROR_UNWIND_BEGIN);
        MUTATE(XL6_IrInstruction, &body[end], kind, XL6_IR_INSTR_ERROR_UNWIND_END);
        MUTATE(XL6_IrInstruction, &body[end], result, 0);
        MUTATE(XL6_IrInstruction, &body[end + 1], trueTarget, body[end + 1].falseTarget);
        MUTATE(XL6_IrInstruction, &body[end + 1], falseTarget, body[end + 1].trueTarget);
        MUTATE(XL6_IrInstruction, &body[end + 2], kind, XL6_IR_INSTR_RETURN);
        MUTATE(XL6_IrInstruction, &body[end + 3], kind, XL6_IR_INSTR_THROW);
        for (j = 1; j <= end; ++j) {
            MUTATE(XL6_IrInstruction, &body[j], errorScope, 0);
            if (body[j].kind == XL6_IR_INSTR_VALUE) {
                XL6_IrValueId saved = ir.operands[body[j].firstOperand];
                MUTATE(XL6_IrInstruction, &body[j], targetSymbol, parameter->symbol);
                MUTATE(XL6_IrInstruction, &body[j], kind, XL6_IR_INSTR_RETAIN);
                ir.operands[body[j].firstOperand] = body[j].result;
                reject(&ir, "receiver view must borrow the original dying receiver");
                ir.operands[body[j].firstOperand] = saved;
            } else if (body[j].kind == XL6_IR_INSTR_CALL) {
                XL6_IrCallTarget* target = (XL6_IrCallTarget*)xl6IrCallTargetGet(&ir, body[j].targetSymbol);
                XL6_IrParameterAbi saved;
                ++steps;
                assert(target != NULL && target->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE);
                MUTATE(XL6_IrInstruction, &body[j], targetSymbol, 0);
                MUTATE(XL6_IrInstruction, &body[j], result, 0);
                MUTATE(XL6_IrInstruction, &body[j], operandCount, 0);
                MUTATE(XL6_IrCallTarget, target, lifecycleAbi, XL6_IR_CALL_TARGET_LIFECYCLE_NONE);
                MUTATE(XL6_IrCallTarget, target, errorAbi, XL6_IR_ERROR_ABI_NO_RAISE);
                saved = target->parameterAbis[0];
                target->parameterAbis[0] = XL6_IR_PARAMETER_ABI_OWNED;
                reject(&ir, "destroy receiver cannot be retained or owned");
                target->parameterAbis[0] = saved;
            } else if (body[j].kind == XL6_IR_INSTR_ERROR_TEST) {
                MUTATE(XL6_IrInstruction, &body[j], trueTarget, body[j].falseTarget);
                MUTATE(XL6_IrInstruction, &body[j], falseTarget, body[j].trueTarget);
            } else if (body[j].kind == XL6_IR_INSTR_CLEANUP_FAIL) {
                MUTATE(XL6_IrInstruction, &body[j], kind, XL6_IR_INSTR_CLEANUP_CAPTURE);
                MUTATE(XL6_IrInstruction, &body[j], kind, XL6_IR_INSTR_ERROR_CLEAR);
            } else if (body[j].kind == XL6_IR_INSTR_CLEANUP_CAPTURE) {
                MUTATE(XL6_IrInstruction, &body[j], kind, XL6_IR_INSTR_CLEANUP_FAIL);
                MUTATE(XL6_IrInstruction, &body[j], kind, XL6_IR_INSTR_ERROR_CLEAR);
            } else if (body[j].kind == XL6_IR_INSTR_JUMP) {
                MUTATE(XL6_IrInstruction, &body[j], trueTarget, body[end + 1].trueTarget);
            }
        }
        if (layout->destroyStepCount > 1) {
            XL6_IrClassDestroyStep* step = &ir.classDestroySteps[layout->firstDestroyStep];
            XL6_IrClassDestroyStep saved = step[0];
            step[0] = step[1]; step[1] = saved;
            reject(&ir, "derived-to-base plan cannot be reordered");
            step[1] = step[0]; step[0] = saved;
        }
        for (j = i + 1; j < ir.classLayoutCount; ++j)
            if (ir.classLayouts[j].destroyFunction != 0) {
                MUTATE(XL6_IrClassLayout, &ir.classLayouts[j], destroyFunction, layout->destroyFunction);
                MUTATE(XL6_IrFunction, &ir.functions[ir.classLayouts[j].destroyFunction - 1], symbol, function->symbol);
                break;
            }
        assert(xl6IrVerifyModule(&ir, NULL));
    }
    assert(programs == 3 && steps == 6 && mutations >= 150);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    printf("class cleanup contract: %lu programs, %lu exact steps, %lu mutations rejected by IR/Cgen/header before output\n",
        (unsigned long)programs, (unsigned long)steps, (unsigned long)mutations);
    return 0;
}
