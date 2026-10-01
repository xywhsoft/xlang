#include "xlir_continuation_owner_test.h"
int main(int argc, char** argv)
{
    const char* names[] = {"sourceLeaseInt", "sourceLeaseTemporary", "sourceLeaseText", "sourceLeaseValue", "sourceLeaseFuture",
        "sourceLeaseTemporaryText", "sourceLeaseTemporaryValue", "sourceLeaseTemporaryFuture"};
    XL6_IrAsyncResultAbi abis[] = {XL6_IR_ASYNC_RESULT_COPY_I64, XL6_IR_ASYNC_RESULT_COPY_I64,
        XL6_IR_ASYNC_RESULT_OWNED_STRING, XL6_IR_ASYNC_RESULT_RETAINED_XVALUE, XL6_IR_ASYNC_RESULT_COPY_I64,
        XL6_IR_ASYNC_RESULT_OWNED_STRING, XL6_IR_ASYNC_RESULT_RETAINED_XVALUE, XL6_IR_ASYNC_RESULT_COPY_I64};
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; XL6_SymbolId targets[8] = {0}, entryTarget = 0;
    size_t i, n, transactions = 0, takes = 0, copies = 0, entryCopies = 0; char binding[1024];
    assert(argc == 4); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_async_source_lease.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) || !xl6SemCheckModule(parse.root, &parse.diags, "source_lease") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) || !xl6IrLowerModule(&ir, &parse.diags)) {
        for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d %s\n", parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
        return 2;
    }
    verify(&ir); assert(ir.asyncMachineCount == 8);
    for (i = 0; i < ir.functionCount; ++i) {
        const XL6_IrNode* declaration = xl6IrNodeGet(&ir, ir.functions[i].declaration);
        const XL6_IrNode* name = declaration && declaration->childCount ? xl6IrNodeGet(&ir, ir.children[declaration->firstChild]) : NULL;
        if (name && name->text && !strcmp(name->text, "sourceLeaseEntry")) entryTarget = ir.functions[i].symbol;
    }
    assert(entryTarget);
    for (i = 0; i < ir.asyncMachineCount; ++i) {
        XL6_IrAsyncMachine* machine = &ir.asyncMachines[i];
        const XL6_IrFunction* fn = &ir.functions[machine->function];
        const XL6_IrNode* declaration = xl6IrNodeGet(&ir, fn->declaration);
        const XL6_IrNode* name = declaration && declaration->childCount ? xl6IrNodeGet(&ir, ir.children[declaration->firstChild]) : NULL;
        XL6_IrCallTarget* target = NULL;
        for (n = 0; n < ir.callTargetCount; ++n) if (ir.callTargets[n].symbol == machine->targetSymbol) target = &ir.callTargets[n];
        assert(target && name && name->text);
        for (n = 0; n < 8; ++n) if (!strcmp(names[n], name->text)) break;
        assert(n < 8 && !targets[n] && target->asyncResultAbi == abis[n]); targets[n] = target->symbol;
        assert(machine->sourceLeaseAbi == XL6_IR_ASYNC_SOURCE_LEASE_CAPTURE_THEN_RELEASE);
        machine->sourceLeaseAbi = XL6_IR_ASYNC_SOURCE_LEASE_INVALID; reject(&ir, "source lease lifetime");
        machine->sourceLeaseAbi = (XL6_IrAsyncSourceLeaseAbi)99; reject(&ir, "source lease lifetime");
        machine->sourceLeaseAbi = XL6_IR_ASYNC_SOURCE_LEASE_CAPTURE_THEN_RELEASE;
    }
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i];
        const XL6_IrNode* origin = xl6IrNodeGet(&ir, instruction->origin);
        if (instruction->kind == XL6_IR_INSTR_STORAGE_TAKE) {
            XL6_IrInstruction saved = *instruction;
            ++takes; assert(instruction->result && !instruction->operandCount &&
                xl6IrValueOwnershipGet(&ir, instruction->result) == XL6_IR_OWNERSHIP_MOVE);
            instruction->kind = XL6_IR_INSTR_LOAD; reject(&ir, NULL); *instruction = saved;
            instruction->targetSymbol = 0; reject(&ir, NULL); *instruction = saved;
            instruction->result = 0; reject(&ir, NULL); *instruction = saved;
            instruction->firstOperand = 0; instruction->operandCount = 1; reject(&ir, NULL); *instruction = saved;
            instruction->evaluationScope = 0; reject(&ir, NULL); *instruction = saved;
        }
        if (instruction->kind == XL6_IR_INSTR_RUNTIME_CALL &&
            (instruction->runtimeOp == XL6_IR_RUNTIME_STRING_COPY || instruction->runtimeOp == XL6_IR_RUNTIME_STRING_NULLABLE_COPY)) {
            XL6_IrInstruction* test = &ir.instructions[i + 1]; XL6_IrInstruction saved = *test;
            ++copies; assert(test->kind == XL6_IR_INSTR_ERROR_TEST && test->operandCount == 1 && ir.operands[test->firstOperand] == instruction->result);
            test->falseTarget = test->trueTarget; reject(&ir, NULL); *test = saved;
            test->kind = XL6_IR_INSTR_DISCARD; reject(&ir, NULL); *test = saved;
            if (origin && origin->kind == XL6_IR_NODE_PARAM_DECL) {
                const XL6_IrBlock* failure = &ir.blocks[test->falseTarget - 1];
                for (n = 0; n < failure->instructionCount; ++n) {
                    XL6_IrInstruction* release = &ir.instructions[failure->firstInstruction + n];
                    if (release->kind == XL6_IR_INSTR_RELEASE) {
                        XL6_SymbolId symbol = release->targetSymbol;
                        release->targetSymbol = origin->symbol; reject(&ir, NULL); release->targetSymbol = symbol;
                        break;
                    }
                }
                ++entryCopies;
            }
        }
        if (instruction->kind == XL6_IR_INSTR_AWAIT) {
            const XL6_IrInstruction* test = &ir.instructions[i + 1];
            assert(test->kind == XL6_IR_INSTR_ERROR_TEST && test->operandCount == 1 && ir.operands[test->firstOperand] == instruction->result);
        }
        if (instruction->kind == XL6_IR_INSTR_CLEANUP_BEGIN && instruction->evaluationScope && origin && origin->kind == XL6_IR_NODE_AWAIT) {
            ++transactions;
            if (instruction->evaluationResult) {
                XL6_SymbolId saved = instruction->evaluationResult;
                instruction->evaluationResult = 0; reject(&ir, NULL); instruction->evaluationResult = saved;
            }
        }
    }
    /* Only the rebound first parameter needs an entry copy. Read-only
     * arguments remain transport borrows; await-result acquisition is owned. */
    assert(transactions == 4 && takes >= 3 && copies == 3 && entryCopies == 1);
    for (i = 0; i < ir.callTargetCount; ++i) if (ir.callTargets[i].symbol == entryTarget) {
        const XL6_IrCallTarget* target = &ir.callTargets[i];
        assert(target->parameterCount == 4);
        for (n = 0; n < 4; ++n) {
            if (n != 2) assert(xl6IrCallTargetParameterCanBorrowReadOnly(&ir, target, n) == (n != 0));
        }
    }
    verify(&ir);
    assert(xl6OutputOpenFile(&out, argv[1]) && xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitHeader(&out, &ir, "SOURCE_LEASE", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    assert(snprintf(binding, sizeof(binding), "%s.bind.h", argv[2]) > 0 && xl6OutputOpenFile(&out, binding));
    for (n = 0; n < 8; ++n) xl6OutputPrintf(&out, "#define SOURCE_LEASE_%lu xlir_async_start_%u\n", (unsigned long)n, targets[n]);
    xl6OutputPrintf(&out, "#define SOURCE_LEASE_ENTRY xlir_fn_%u\n", entryTarget);
    xl6OutputClose(&out);
    printf("async source lease: 8 machines, 4 await-operand cleanup transactions, scalar/string/object/nested Future acquisition, %llu C/header-gated mutations\n", (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
