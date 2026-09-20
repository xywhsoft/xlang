#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "xlir_callable_bridge_mutations.h"
static unsigned mutations;
static void reject(const XL6_IrModule* ir)
{
    XL6_DiagList diags = {0}; XL6_Output out; char* text;
    assert(!xl6IrVerifyModule(ir, &diags) && diags.count);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out, ir, "BAD_BYVAL", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; size_t i, j;
    unsigned take = 0, scalar = 0, context = 0, moves = 0, causes = 0;
    int direct = argc == 5 && !strcmp(argv[4], "--direct");
    assert(argc == 4 || direct); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile(direct ? "test/cases/xlir_task_byval_direct.xl" : "test/cases/xlir_task_byval.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "task_byval") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrVerifyModule(&ir, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    xlirCallableBridgeMutations(&ir, reject);
    assert(ir.callbackBoundaryCount == 8 && ir.callbackCaptureCount == 18);
    for (i = 0; i < ir.callbackCaptureCount; ++i) {
        XL6_IrCallbackCapture* c = &ir.callbackCaptures[i], saved = *c;
        if (c->invokeAbi == XL6_IR_CALLBACK_INVOKE_TAKE) {
            assert(c->role == XL6_IR_CALLBACK_CAPTURE_ARGUMENT &&
                c->destroyAbi != XL6_IR_ASYNC_FRAME_DESTROY_NONE);
            ++take;
        } else if (c->invokeAbi == XL6_IR_CALLBACK_INVOKE_VALUE) ++scalar;
        else { assert(c->invokeAbi == XL6_IR_CALLBACK_INVOKE_CONTEXT); ++context; }
        c->invokeAbi = XL6_IR_CALLBACK_INVOKE_INVALID; reject(&ir); *c = saved;
        c->invokeAbi = saved.invokeAbi == XL6_IR_CALLBACK_INVOKE_TAKE
            ? XL6_IR_CALLBACK_INVOKE_BORROW : XL6_IR_CALLBACK_INVOKE_TAKE;
        reject(&ir); *c = saved;
        c->transferAbi = XL6_IR_ASYNC_FRAME_TRANSFER_MOVE_OWNED;
        reject(&ir); *c = saved;
    }
    assert(take == 8 && scalar == 2 && context == 8);
    for (i = 0; i < ir.functionCount; ++i) {
        const XL6_IrFunction* f = &ir.functions[i];
        if (f->kind != XL6_IR_FUNCTION_NAMED_ADAPTER) continue;
        for (j = f->firstInstruction; j < f->firstInstruction + f->instructionCount; ++j) {
            XL6_IrInstruction* move = &ir.instructions[j];
            XL6_IrInstruction saved;
            XL6_DiagList diagnostics = {0};
            if (move->kind != XL6_IR_INSTR_MOVE) continue;
            saved = *move;
            /* LOAD -> CALL is a borrow fed to a consuming edge. Confirm the
             * affine verifier itself rejects it, not only structural checks. */
            move->kind = XL6_IR_INSTR_DISCARD; move->targetSymbol = 0;
            assert(!xl6IrVerifyOwnerConsumption(&ir, &diagnostics) && diagnostics.count);
            xl6DiagFree(&diagnostics); reject(&ir); *move = saved; ++moves;
        }
    }
    assert(moves == 9);
    for (i = 0; i < ir.instructionCount; ++i) {
        const XL6_IrInstruction* cause = &ir.instructions[i];
        XL6_SymbolId owner = 0; unsigned releases = 0;
        if (cause->kind != XL6_IR_INSTR_RUNTIME_CALL ||
            cause->runtimeOp != XL6_IR_RUNTIME_ERROR_CAUSE_REF) continue;
        for (j = i + 1; j < ir.instructionCount; ++j) {
            const XL6_IrInstruction* op = &ir.instructions[j];
            if (op->kind == XL6_IR_INSTR_STORE && op->operandCount == 1 &&
                ir.operands[op->firstOperand] == cause->result) {
                assert(!owner); owner = op->targetSymbol;
            }
            if (owner && op->kind == XL6_IR_INSTR_RELEASE && op->targetSymbol == owner) ++releases;
        }
        assert(owner && releases); ++causes;
    }
    assert(causes == 1);
    assert(xl6OutputOpenFile(&out, argv[1]));
    assert(xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]));
    assert(xl6IrCgenEmitHeader(&out, &ir, "TASK_BYVAL", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    printf("task byval %s contract: %u consuming slots, %u scalar slots, %u contexts, %u named adapter moves, %u cause receiver owners, %u rejected mutations\n",
        direct ? "direct" : "full", take, scalar, context, moves, causes, mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for (i = 0; i < parse.diags.count; ++i)
        fprintf(stderr, "%d:%d: %s\n", parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 1;
}
