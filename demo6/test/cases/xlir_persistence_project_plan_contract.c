#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned mutations;
static const XL6_IrInstruction* definition(const XL6_IrModule* ir, XL6_IrValueId value)
{
    for (size_t i = 0; i < ir->instructionCount; ++i)
        if (ir->instructions[i].result == value) return &ir->instructions[i];
    return NULL;
}
static void literal(const char* source, const char* expected)
{
    char* actual = xl6IrCanonicalCStringLiteral(source);
    assert(actual && !strcmp(actual, expected)); xrtFree(actual);
}
static void literals(void)
{
    literal("\"a\\0b\"", "\"a\\000b\"");
    literal("\"a\\0007\"", "\"a\\0007\"");
    literal("\"a\\x00Z\"", "\"a\\000Z\"");
    literal("r\"a\\0b\"", "\"a\\\\0b\"");
    literal("\"\"\"a\r\nb\"\"\"", "\"a\\nb\"");
    literal("\"你\"", "\"你\"");
    assert(!xl6IrCanonicalCStringLiteral("\"\\x100\""));
    assert(!xl6IrCanonicalCStringLiteral("\"\\q\""));
    assert(!xl6IrCanonicalCStringLiteral(NULL));
    assert(!xl6IrCanonicalIdentifierCString("bad\nname"));
    char* actual = xl6IrCanonicalCStringBytes("a\"b");
    assert(actual && !strcmp(actual, "\"a\\\"b\"")); xrtFree(actual);
}
static void rejectedOutput(const XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir, NULL));
    for (unsigned header = 0; header < 2; ++header) {
        XL6_Output output; XL6_DiagList diagnostics; size_t size = SIZE_MAX;
        xl6DiagInit(&diagnostics); assert(xl6OutputOpenMemory(&output));
        assert(!(header ? xl6IrCgenEmitHeader(&output, ir, "REJECTED_PROJECT", &diagnostics) :
            xl6IrCgenEmitC(&output, ir, &diagnostics)));
        char* text = xl6OutputTakeMemoryText(&output, &size);
        assert(size == 0); xrtFree(text); xl6OutputDiscard(&output); xl6DiagFree(&diagnostics);
    }
    ++mutations;
}
static void rejected(const XL6_IrModule* ir)
{
    assert(!xl6IrVerifyPersistenceProjectPlans(ir));
    rejectedOutput(ir);
}

static const XL6_IrPersistenceProjectSchema* schema(
    const XL6_IrModule* ir, const XL6_IrPersistenceProjectPlan* plan, const char* name)
{
    for (size_t i = 0; i < plan->schemaCount; ++i) {
        const XL6_IrPersistenceProjectSchema* row = &ir->persistenceProjectSchemas[plan->firstSchema + i];
        const XL6_IrType* type = xl6IrTypeGet(&ir->types, ir->classLayouts[row->classLayout].type);
        if (type->name && strcmp(type->name, name) == 0) return row;
    }
    return NULL;
}

static void inspect(const XL6_IrModule* ir)
{
    assert(ir->persistenceProjectPlanCount == 2);
    for (size_t i = 0; i < 2; ++i) {
        const XL6_IrPersistenceProjectPlan* plan = &ir->persistenceProjectPlans[i];
        const XL6_IrPersistenceProjectSchema* child = schema(ir, plan, "IRProjectChild");
        const XL6_IrPersistenceProjectSchema* open = schema(ir, plan, "IRProjectOpen");
        const XL6_IrPersistenceProjectSchema* secret = schema(ir, plan, "IRProjectSecret");
        assert(plan->format == (i ? XL6_IR_PERSIST_FORMAT_XLON : XL6_IR_PERSIST_FORMAT_JSON));
        assert(child && open && secret && !child->disabled && !open->disabled && secret->disabled);
        assert(!schema(ir, plan, "IRProjectUnused"));
        assert(child->fieldCount == 5 && secret->fieldCount == 0 && !secret->dynamicDestroySymbol);
        assert(child->identity == xl6IrTypeGet(&ir->types, ir->classLayouts[child->classLayout].type)->typeValueId);
        assert(child->dynamicField == XL6_IR_CLASS_FIELD_INDEX_INVALID && !child->dynamicDestroySymbol);
        assert(open->dynamicField < ir->classFieldCount && open->dynamicHandleField < ir->classFieldCount);
        assert(open->dynamicDestroySymbol == ir->classFields[open->dynamicHandleField].nativeDestroySymbol);
        assert(open->dynamicDestroySymbol && xl6IrCallTargetGet(ir, open->dynamicDestroySymbol));
        const XL6_IrPersistenceProjectField* fields = &ir->persistenceProjectFields[child->firstField];
        assert(!strcmp(ir->classFields[fields[0].classField].name, "inherited"));
        assert(!strcmp(ir->classFields[fields[1].classField].name, "own"));
        assert(fields[0].targetKeyAbi == XL6_IR_PERSIST_PROJECT_KEY_C_LITERAL);
        assert(!strcmp(fields[0].targetKey, i ? "\"base\"" : "\"shared\""));
        assert(!strcmp(fields[1].targetKey, i ? "\"child\"" : "\"shared\""));
        assert(fields[2].targetKeyAbi == XL6_IR_PERSIST_PROJECT_KEY_C_LITERAL);
        assert(i ? !strcmp(fields[2].targetKey, "\"other\"") :
            (strstr(fields[2].targetKey, "\\0") != NULL || strstr(fields[2].targetKey, "\\x00") != NULL));
        assert(fields[3].omitFlags == (i ? XL6_IR_FIELD_PERSIST_OMIT_NULL : XL6_IR_FIELD_PERSIST_OMIT_EMPTY));
        assert(fields[4].targetKeyAbi == XL6_IR_PERSIST_PROJECT_KEY_FIELD_NAME);
        for (size_t f = 0; f < child->fieldCount; ++f) {
            const char* name = ir->classFields[fields[f].classField].name;
            assert(strcmp(name, "privateText") && strcmp(name, "skipped"));
        }
    }
}

#define ALTER(Row, Member, Value) do { \
    unsigned char saved[sizeof((Row)->Member)]; \
    memcpy(saved, &(Row)->Member, sizeof(saved)); \
    (Row)->Member = (Value); rejected(&ir); memcpy(&(Row)->Member, saved, sizeof(saved)); \
} while (0)
#define ALTER_OP(Row, Member, Value) do { \
    unsigned char saved[sizeof((Row)->Member)]; \
    memcpy(saved, &(Row)->Member, sizeof(saved)); \
    (Row)->Member = (Value); rejectedOutput(&ir); memcpy(&(Row)->Member, saved, sizeof(saved)); \
} while (0)

int main(int argc, char** argv)
{
    assert(argc == 4); XL6_ParseResult parse; XL6_IrModule ir; XL6_Output output; int ok = 0;
    literals();
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile(argv[1], &parse) || !xl6StdlibExpandModule(parse.root, &parse.diags)) goto done;
    if (!strcmp(argv[3], "reject")) {
        assert(!xl6SemCheckModule(parse.root, &parse.diags, "project_plan_contract") && parse.diags.count);
        puts("Included non-persistent field refused by semantic checking"); ok = 1; goto done;
    }
    if (!xl6SemCheckModule(parse.root, &parse.diags, "project_plan_contract") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags)) goto done;
    assert(xl6IrVerifyPersistenceProjectPlans(&ir) && xl6IrVerifyModule(&ir, NULL));
    for (size_t i = 0; i < ir.callTargetCount; ++i) {
        XL6_IrCallTarget* target = &ir.callTargets[i];
        if (!xl6IrRuntimeSerializationBridgeHasErrorCarrierResult(target->runtimeBridge)) continue;
        assert(target->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE &&
            target->errorPhysicalAbi == XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER);
        ALTER_OP(target, errorAbi, XL6_IR_ERROR_ABI_NO_RAISE);
        ALTER_OP(target, errorPhysicalAbi, XL6_IR_CALL_ERROR_PHYSICAL_NONE);
    }
    for (size_t i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i];
        if (instruction->kind != XL6_IR_INSTR_RUNTIME_CALL) continue;
        if (instruction->runtimeOp == XL6_IR_RUNTIME_PERSISTENCE_JSON_PROJECT ||
            instruction->runtimeOp == XL6_IR_RUNTIME_PERSISTENCE_XLON_PROJECT) {
            assert(xl6IrPersistenceProjectInstructionAbi(&ir, instruction));
            assert(xl6IrRuntimeContainerAllocationErrorPhysicalAbi(instruction->runtimeOp) == XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT);
            const XL6_IrInstruction* source = definition(&ir, ir.operands[instruction->firstOperand]);
            assert(source);
            ALTER_OP(instruction, type, source->type);
            ALTER_OP(instruction, payloadType, XL6_IR_TYPE_INVALID);
            ALTER_OP(instruction, operandCount, 0);
            ALTER_OP(instruction, runtimeOp, XL6_IR_RUNTIME_XVALUE_CLONE);
            XL6_IrType* wire = &ir.types.items[instruction->type - 1];
            ALTER_OP(wire, persistenceWireFormat, XL6_IR_PERSIST_FORMAT_NONE);
            ALTER_OP(wire, persistenceWireFormat, wire->persistenceWireFormat == XL6_IR_PERSIST_FORMAT_JSON
                ? XL6_IR_PERSIST_FORMAT_XLON : XL6_IR_PERSIST_FORMAT_JSON);
            ALTER_OP(wire, sourceTypeId, 1);
        } else if (xl6IrConfiguredPersistenceRuntime(instruction->runtimeOp) != NULL) {
            const XL6_IrConfiguredPersistenceEncoder* row = xl6IrConfiguredPersistenceRuntime(instruction->runtimeOp);
            assert(xl6IrConfiguredPersistenceInstructionAbi(&ir, instruction));
            assert(xl6IrRuntimeStringAllocationErrorPhysicalAbi(instruction->runtimeOp) == XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT);
            ALTER_OP(instruction, payloadType, XL6_IR_TYPE_INVALID);
            ALTER_OP(instruction, operandCount, 0);
            ALTER_OP(instruction, targetSymbol, 1);
            ALTER_OP(instruction, type, definition(&ir, ir.operands[instruction->firstOperand])->type);
            ALTER_OP(instruction, runtimeOp, row->projectionFormat == XL6_IR_PERSIST_FORMAT_JSON
                ? XL6_IR_RUNTIME_PERSISTENCE_XLON_CONFIG_STRINGIFY
                : XL6_IR_RUNTIME_PERSISTENCE_JSON_CONFIG_STRINGIFY);
            XL6_IrValueId* options = &ir.operands[instruction->firstOperand + 1];
            XL6_IrValueId saved = *options;
            *options = ir.operands[instruction->firstOperand];
            rejectedOutput(&ir); *options = saved;
            const XL6_IrInstruction* wire = definition(&ir, ir.operands[instruction->firstOperand]);
            XL6_IrType* wireType = &ir.types.items[wire->type - 1];
            ALTER_OP(wireType, persistenceWireFormat, XL6_IR_PERSIST_FORMAT_NONE);
        } else if (instruction->runtimeOp == XL6_IR_RUNTIME_PERSISTENCE_JSON_STRINGIFY ||
                   instruction->runtimeOp == XL6_IR_RUNTIME_PERSISTENCE_XLON_STRINGIFY) {
            assert(xl6IrRuntimeStringAllocationErrorPhysicalAbi(instruction->runtimeOp) == XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT);
        }
    }
    for (size_t i = 0; i < ir.intrinsicCallCount; ++i) {
        XL6_IrIntrinsicCall* intrinsic = &ir.intrinsicCalls[i];
        if (xl6IrConfiguredPersistenceIntrinsic(intrinsic->kind) == NULL) continue;
        ALTER_OP(intrinsic, errorAbi, XL6_IR_ERROR_ABI_NO_RAISE);
        ALTER_OP(intrinsic, resultAbi, XL6_IR_INTRINSIC_RESULT_VALUE);
        ALTER_OP(intrinsic, payloadType, XL6_IR_TYPE_INVALID);
        ALTER_OP(intrinsic, payloadAbi, XL6_IR_ASYNC_RESULT_VOID);
        ALTER_OP(intrinsic, persistenceSourceOperand, SIZE_MAX);
        ALTER_OP(intrinsic, persistenceSourceOperand, 1 - intrinsic->persistenceSourceOperand);
        XL6_IrIntrinsicOperand* source = &ir.intrinsicOperands[intrinsic->firstOperand + intrinsic->persistenceSourceOperand];
        ALTER_OP(source, abi, XL6_IR_INTRINSIC_OPERAND_CONSUME);
    }
    if (!strcmp(argv[3], "sequences")) {
        assert(ir.persistenceProjectSequenceCount == 6);
        for (size_t i = 0; i < ir.persistenceProjectPlanCount; ++i) {
            const XL6_IrPersistenceProjectPlan* plan = &ir.persistenceProjectPlans[i];
            assert(plan->sequenceCount == 3);
            for (size_t j = 0; j < plan->sequenceCount; ++j) {
                const XL6_IrPersistenceProjectSequence* sequence =
                    &ir.persistenceProjectSequences[plan->firstSequence + j];
                const XL6_IrType* type = xl6IrTypeGet(&ir.types, sequence->type);
                const XL6_IrType* payload = xl6IrTypeGet(&ir.types, type->persistencePayloadType);
                assert(type && sequence->identity == type->typeValueId && sequence->sourceAbi == type->collectionAbi);
                assert(sequence->sourceAbi == XL6_IR_COLLECTION_LIST || sequence->sourceAbi == XL6_IR_COLLECTION_SET);
                assert(payload && payload->cAbi != XL6_IR_C_ABI_MANAGED_STRING);
            }
        }
    }
    if (!strcmp(argv[3], "empty")) {
        assert(!ir.persistenceProjectPlanCount && !ir.persistenceProjectSchemaCount &&
            !ir.persistenceProjectFieldCount && !ir.persistenceProjectSequenceCount);
        ALTER(&ir, persistenceProjectPlanCount, 1);
    } else {
        if (!strcmp(argv[3], "schema")) inspect(&ir);
        else assert(ir.persistenceProjectPlanCount == 2);
        ALTER(&ir, persistenceProjectPlanCount, 0);
        ALTER(&ir, persistenceProjectPlanCount, SIZE_MAX);
        ALTER(&ir, persistenceProjectPlans, NULL);
        ALTER(&ir, persistenceProjectPlanCapacity, 0);
        if (ir.persistenceProjectSchemaCount) {
            ALTER(&ir, persistenceProjectSchemaCount, 0);
            ALTER(&ir, persistenceProjectSchemas, NULL);
        }
        if (ir.persistenceProjectFieldCount) {
            ALTER(&ir, persistenceProjectFieldCount, 0);
            ALTER(&ir, persistenceProjectFields, NULL);
        }
        if (ir.persistenceProjectSequenceCount) {
            ALTER(&ir, persistenceProjectSequenceCount, 0);
            ALTER(&ir, persistenceProjectSequences, NULL);
            ALTER(&ir, persistenceProjectSequenceCapacity, 0);
        }
        for (size_t i = 0; i < ir.persistenceProjectPlanCount; ++i) {
            XL6_IrPersistenceProjectPlan* row = &ir.persistenceProjectPlans[i];
            ALTER(row, format, XL6_IR_PERSIST_FORMAT_NONE);
            ALTER(row, firstSchema, SIZE_MAX); ALTER(row, schemaCount, SIZE_MAX);
            ALTER(row, firstSequence, SIZE_MAX); ALTER(row, sequenceCount, SIZE_MAX);
        }
        for (size_t i = 0; i < ir.persistenceProjectSchemaCount; ++i) {
            XL6_IrPersistenceProjectSchema* row = &ir.persistenceProjectSchemas[i];
            ALTER(row, classLayout, SIZE_MAX); ALTER(row, identity, 0);
            ALTER(row, disabled, 2); ALTER(row, firstField, SIZE_MAX);
            ALTER(row, fieldCount, SIZE_MAX); ALTER(row, dynamicField, SIZE_MAX - 1);
            ALTER(row, dynamicHandleField, SIZE_MAX - 1); ALTER(row, dynamicDestroySymbol, UINT32_MAX);
        }
        for (size_t i = 0; i < ir.persistenceProjectFieldCount; ++i) {
            XL6_IrPersistenceProjectField* row = &ir.persistenceProjectFields[i];
            ALTER(row, classField, SIZE_MAX); ALTER(row, omitFlags, UINT32_MAX);
            ALTER(row, targetKeyAbi, 0); ALTER(row, targetKey, NULL);
        }
        for (size_t i = 0; i < ir.persistenceProjectSequenceCount; ++i) {
            XL6_IrPersistenceProjectSequence* row = &ir.persistenceProjectSequences[i];
            ALTER(row, type, XL6_IR_TYPE_INVALID); ALTER(row, identity, 0);
            ALTER(row, sourceAbi, XL6_IR_COLLECTION_ARRAY);
        }
        XL6_IrPersistenceProjectPlan* savedPlans = ir.persistenceProjectPlans;
        XL6_IrPersistenceProjectSchema* savedSchemas = ir.persistenceProjectSchemas;
        XL6_IrPersistenceProjectField* savedFields = ir.persistenceProjectFields;
        XL6_IrPersistenceProjectSequence* savedSequences = ir.persistenceProjectSequences;
        size_t first = ir.functions[0].firstInstruction;
        ir.functions[0].firstInstruction = SIZE_MAX;
        assert(!xl6IrBuildPersistenceProjectPlans(&ir));
        assert(ir.persistenceProjectPlans == savedPlans && ir.persistenceProjectSchemas == savedSchemas &&
            ir.persistenceProjectFields == savedFields && ir.persistenceProjectSequences == savedSequences);
        ir.functions[0].firstInstruction = first;
        assert(xl6IrBuildPersistenceProjectPlans(&ir));
        if (!strcmp(argv[3], "schema")) inspect(&ir);
        assert(xl6IrVerifyModule(&ir, NULL));
    }
    assert(xl6OutputOpenFile(&output, argv[2])); xl6IrDumpModuleJson(&output, &ir); xl6OutputClose(&output);
    printf("Projection plans: %zu plans, %zu schemas, %zu fields; %u mutations refused before C/header\n",
        ir.persistenceProjectPlanCount, ir.persistenceProjectSchemaCount, ir.persistenceProjectFieldCount, mutations);
    ok = 1;
done:
    for (size_t i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d: %s\n",
        parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return !ok;
}
