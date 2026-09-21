#include "xlir_continuation_owner_test.h"
int main(void)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; char* text;
    size_t aggregates = 0, abiMutations = 0, errorPayloadMutations = 0;
    const char* abiDiagnostic = "Future aggregate mapper/code ownership ABI is unresolved";
    const char* fixtures[] = {"test/cases/xlir_future_aggregate_ownership.xl",
        "test/cases/xlir_task_all_void_outcome.xl", "test/cases/xlir_task_all_error_abi.xl"};
    for (size_t fixture = 0; fixture < sizeof(fixtures)/sizeof(*fixtures); ++fixture) {
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile(fixtures[fixture], &parse) &&
        xl6StdlibExpandModule(parse.root, &parse.diags) &&
        xl6SemCheckModule(parse.root, &parse.diags, "aggregate_ownership") &&
        xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    assert(ir.abi.futureAggregateAbi == XL6_IR_FUTURE_AGGREGATE_NONE); verify(&ir);
    ir.abi.futureAggregateAbi = XL6_IR_FUTURE_AGGREGATE_RESIDENT_OWNED_MAPPER_V1;
    reject(&ir, abiDiagnostic); ++abiMutations;
    ir.abi.futureAggregateAbi = XL6_IR_FUTURE_AGGREGATE_NONE;
    assert(xl6IrLowerModule(&ir, &parse.diags));
    assert(ir.abi.futureAggregateAbi == XL6_IR_FUTURE_AGGREGATE_RESIDENT_OWNED_MAPPER_V1);
    assert(xl6IrModuleFutureAggregateRequirement(&ir) == ir.abi.futureAggregateAbi);
    for (int invalid = -1; invalid <= 2; ++invalid) {
        if (invalid == XL6_IR_FUTURE_AGGREGATE_RESIDENT_OWNED_MAPPER_V1) continue;
        ir.abi.futureAggregateAbi = (XL6_IrFutureAggregateAbi)invalid;
        reject(&ir, abiDiagnostic); ++abiMutations;
    }
    ir.abi.futureAggregateAbi = XL6_IR_FUTURE_AGGREGATE_RESIDENT_OWNED_MAPPER_V1;
    verify(&ir);
    for (size_t i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i]; XL6_IrInstruction saved = *instruction;
        if (instruction->runtimeOp != XL6_IR_RUNTIME_FUTURE_ALL_XVALUES &&
            instruction->runtimeOp != XL6_IR_RUNTIME_FUTURE_ALL_XVALUES_ARRAY &&
            instruction->runtimeOp != XL6_IR_RUNTIME_FUTURE_RACE &&
            instruction->runtimeOp != XL6_IR_RUNTIME_FUTURE_RACE_ARRAY &&
            instruction->runtimeOp != XL6_IR_RUNTIME_FUTURE_WHEN_ANY_INDEX &&
            instruction->runtimeOp != XL6_IR_RUNTIME_FUTURE_WHEN_ANY_INDEX_ARRAY) continue;
        ++aggregates;
        instruction->asyncResultAbi = XL6_IR_ASYNC_RESULT_INVALID; reject(&ir, NULL); *instruction = saved;
        instruction->asyncResultAbi = XL6_IR_ASYNC_RESULT_COPY_F64; reject(&ir, NULL); *instruction = saved;
        instruction->payloadType = 0; reject(&ir, NULL); *instruction = saved;
        instruction->operandCount = 0; reject(&ir, NULL); *instruction = saved;
        if ( saved.runtimeOp == XL6_IR_RUNTIME_FUTURE_ALL_XVALUES &&
             saved.asyncResultAbi == XL6_IR_ASYNC_RESULT_RETAINED_ERROR ) {
            XL6_IrType* payload = &ir.types.items[saved.payloadType - 1];
            XL6_IrType savedPayload = *payload;
            payload->typeValueId = 0; reject(&ir, NULL); *payload = savedPayload;
            payload->ownershipAbi = XL6_IR_OWNERSHIP_ABI_TRIVIAL;
            reject(&ir, NULL); *payload = savedPayload;
            payload->nullAbi = XL6_IR_NULL_ABI_NONE;
            reject(&ir, NULL); *payload = savedPayload;
            errorPayloadMutations += 3;
        }
    }
    verify(&ir);
    assert(xl6OutputOpenMemory(&out) && xl6IrCgenEmitC(&out, &ir, &parse.diags)); text = xl6OutputTakeMemoryText(&out, NULL);
    assert(text && strstr(text, "#include <__xl_runtime/xl6_future_aggregate.h>") &&
        strstr(text, "(xl6FutureAggregateRuntimeV1Get()->All)") && strstr(text, "xlir_future_aggregate_owner"));
    if (fixture == 0) assert(strstr(text, "(xl6FutureAggregateRuntimeV1Get()->AllArray)") &&
        strstr(text, "(xl6FutureAggregateRuntimeV1Get()->Race)") && strstr(text, "(xl6FutureAggregateRuntimeV1Get()->WhenAnyIndex)") &&
        strstr(text, "(xl6FutureAggregateRuntimeV1Get()->RaceArray)") && strstr(text, "(xl6FutureAggregateRuntimeV1Get()->WhenAnyIndexArray)"));
    assert(!strstr(text, "xl6FutureAllXValues(") && !strstr(text, "xl6FutureAllChecked(") &&
        !strstr(text, "xl6FutureRaceChecked(") && !strstr(text, "xl6FutureWhenAnyIndex(") &&
        !strstr(text, "xlir_future_array_unpack") && !strstr(text, "xlir_future_when_any_array"));
    assert(!strstr(text, "xlir_future_when_any_index_continue") && !strstr(text, "xrtFutureThenOwnedCancelSource(") && !strstr(text, "xl6FutureThenOwned("));
    xrtFree(text);
    assert(xl6OutputOpenMemory(&out)); xl6IrDumpModuleText(&out, &ir); text = xl6OutputTakeMemoryText(&out, NULL);
    assert(text && strstr(text, "future-aggregate: 1\n")); xrtFree(text);
    assert(xl6OutputOpenMemory(&out)); xl6IrDumpModuleJson(&out, &ir); text = xl6OutputTakeMemoryText(&out, NULL);
    assert(text && strstr(text, "\"futureAggregateAbi\": 1")); xrtFree(text);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    }
    /* The capability is computed from actual instructions, not a needs bit or
     * function name. Every supported operation independently requires it. */
    {
        const XL6_IrRuntimeOp ops[] = {XL6_IR_RUNTIME_FUTURE_ALL_XVALUES, XL6_IR_RUNTIME_FUTURE_RACE,
            XL6_IR_RUNTIME_FUTURE_WHEN_ANY_INDEX, XL6_IR_RUNTIME_FUTURE_ALL_XVALUES_ARRAY,
            XL6_IR_RUNTIME_FUTURE_RACE_ARRAY, XL6_IR_RUNTIME_FUTURE_WHEN_ANY_INDEX_ARRAY};
        XL6_IrModule probe = {0}; XL6_IrInstruction instruction = {0};
        probe.instructions = &instruction; probe.instructionCount = 1;
        for (size_t i = 0; i < sizeof(ops)/sizeof(*ops); ++i) {
            instruction.kind = XL6_IR_INSTR_RUNTIME_CALL; instruction.runtimeOp = ops[i];
            assert(xl6IrModuleFutureAggregateRequirement(&probe) == XL6_IR_FUTURE_AGGREGATE_RESIDENT_OWNED_MAPPER_V1);
            instruction.kind = XL6_IR_INSTR_VALUE;
            assert(xl6IrModuleFutureAggregateRequirement(&probe) == XL6_IR_FUTURE_AGGREGATE_NONE);
        }
        xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
        assert(xl6ParseSource("assert 6 * 7 == 42\n", "aggregate_none.xl", &parse) &&
            xl6SemCheckModule(parse.root, &parse.diags, "aggregate_none") &&
            xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) && xl6IrLowerModule(&ir, &parse.diags));
        assert(ir.abi.futureAggregateAbi == XL6_IR_FUTURE_AGGREGATE_NONE); verify(&ir);
        ir.abi.futureAggregateAbi = XL6_IR_FUTURE_AGGREGATE_RESIDENT_OWNED_MAPPER_V1;
        reject(&ir, abiDiagnostic); ++abiMutations;
        ir.abi.futureAggregateAbi = XL6_IR_FUTURE_AGGREGATE_NONE; verify(&ir);
        xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    }
    assert(aggregates == 10 && errorPayloadMutations == 3 && abiMutations == 13 &&
        mutations == 43 + abiMutations);
    printf("Aggregate ownership IR: %zu operations, %zu C/header-gated mutations\n", aggregates, mutations - abiMutations);
    printf("Aggregate module ABI: 13 additional C/header-gated mutations, all 6 operations independently derived\n"); return 0;
}
