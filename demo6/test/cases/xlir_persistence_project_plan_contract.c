#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned mutations;
static size_t allocationCalls, allocationFailure = SIZE_MAX, liveAllocations;
static int allocationArmed, allocationHit;
static void* faultAllocate(void* unused, size_t size)
{
    void* value; (void)unused;
    if (allocationArmed && ++allocationCalls >= allocationFailure) { allocationHit = 1; return NULL; }
    value = malloc(size); if (value) ++liveAllocations; return value;
}
static void* faultResize(void* unused, void* pointer, size_t size)
{
    void* value; int existing = pointer != NULL; (void)unused;
    if (allocationArmed && ++allocationCalls >= allocationFailure) { allocationHit = 1; return NULL; }
    assert(size); value = realloc(pointer, size); if (value && !existing) ++liveAllocations; return value;
}
static void faultRelease(void* unused, void* pointer)
{ (void)unused; if (pointer) { assert(liveAllocations); --liveAllocations; free(pointer); } }
static size_t restoreAllocationPrefixes(XL6_IrModule* ir)
{
    size_t calls, before, point;
    allocationCalls = 0; allocationFailure = SIZE_MAX; allocationArmed = 1;
    assert(xl6IrBuildPersistenceRestorePlans(ir)); allocationArmed = 0; calls = allocationCalls;
    assert(xl6IrVerifyModule(ir, NULL));
    for (point = 1; point <= calls; ++point) {
        XL6_IrModule prior = *ir;
        void *plans, *schemas, *fields, *roots, *nodes, *edges;
#define COPY_TABLE(Var, Name) \
        Var = malloc(ir->Name##Count * sizeof(*ir->Name##s) + 1); assert(Var); \
        if (ir->Name##Count) memcpy(Var, ir->Name##s, ir->Name##Count * sizeof(*ir->Name##s))
        COPY_TABLE(plans, persistenceRestorePlan); COPY_TABLE(schemas, persistenceRestoreSchema);
        COPY_TABLE(fields, persistenceRestoreField); COPY_TABLE(roots, persistenceRestoreRoot);
        COPY_TABLE(nodes, persistenceRestoreNode); COPY_TABLE(edges, persistenceRestoreEdge);
#undef COPY_TABLE
        xrtClearError(); before = liveAllocations;
        allocationCalls = 0; allocationHit = 0; allocationFailure = point; allocationArmed = 1;
        assert(!xl6IrBuildPersistenceRestorePlans(ir)); allocationArmed = 0;
        assert(allocationHit); xrtClearError(); assert(liveAllocations == before);
#define SAME_TABLE(Var, Name) \
        assert(ir->Name##s == prior.Name##s && ir->Name##Count == prior.Name##Count && \
            ir->Name##Capacity == prior.Name##Capacity); \
        assert(!ir->Name##Count || !memcmp(Var, ir->Name##s, ir->Name##Count * sizeof(*ir->Name##s))); free(Var)
        SAME_TABLE(plans, persistenceRestorePlan); SAME_TABLE(schemas, persistenceRestoreSchema);
        SAME_TABLE(fields, persistenceRestoreField); SAME_TABLE(roots, persistenceRestoreRoot);
        SAME_TABLE(nodes, persistenceRestoreNode); SAME_TABLE(edges, persistenceRestoreEdge);
#undef SAME_TABLE
        assert(xl6IrVerifyModule(ir, NULL));
    }
    allocationFailure = SIZE_MAX;
    return calls;
}
static int restoreResidentSymbol(void* context, const char* symbol)
{
    if (!strcmp(symbol, "xl6ArrayImportTakeV1") ||
        !strcmp(symbol, "xl6PersistenceRestoreParsedTakeV1")) ++*(size_t*)context;
    return 1;
}
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
static void rejectedOutputAt(const XL6_IrModule* ir, unsigned line, const char* member)
{
    int accepted = xl6IrVerifyModule(ir, NULL);
    if (accepted)
        fprintf(stderr, "Unexpected accepted mutation %u at line %u: %s\n", mutations + 1, line, member);
    assert(!accepted);
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
#define rejectedOutput(Ir) rejectedOutputAt((Ir), __LINE__, "operand/table")
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
    (Row)->Member = (Value); rejectedOutputAt(&ir, __LINE__, #Member); memcpy(&(Row)->Member, saved, sizeof(saved)); \
} while (0)
#define ALTER_RESTORE(Row, Member, Value) do { \
    unsigned char saved[sizeof((Row)->Member)]; \
    memcpy(saved, &(Row)->Member, sizeof(saved)); \
    (Row)->Member = (Value); assert(!xl6IrVerifyPersistenceRestorePlans(&ir)); \
    rejectedOutputAt(&ir, __LINE__, #Member); memcpy(&(Row)->Member, saved, sizeof(saved)); \
} while (0)
#define ALTER_GRAPH(Row, Member, Value) do { \
    unsigned char saved[sizeof((Row)->Member)]; \
    memcpy(saved, &(Row)->Member, sizeof(saved)); \
    (Row)->Member = (Value); assert(!xl6IrVerifyPersistenceRestoreGraphs(&ir)); \
    rejectedOutputAt(&ir, __LINE__, #Member); memcpy(&(Row)->Member, saved, sizeof(saved)); \
} while (0)

int main(int argc, char** argv)
{
    assert(argc == 4); XL6_ParseResult parse; XL6_IrModule ir; XL6_Output output; int ok = 0;
    xallocator allocator = {NULL, faultAllocate, faultResize, faultRelease};
    assert(xrtSetAllocator(&allocator));
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
    assert(xl6IrVerifyPersistenceProjectPlans(&ir) && xl6IrVerifyPersistenceRestorePlans(&ir) &&
        xl6IrVerifyModule(&ir, NULL));
    unsigned graphStart = mutations;
    if (!ir.persistenceRestoreRootCount) {
        assert(!ir.persistenceRestoreNodeCount && !ir.persistenceRestoreEdgeCount);
        ALTER_GRAPH(&ir, persistenceRestoreRootCount, 1);
        ALTER_GRAPH(&ir, persistenceRestoreNodeCount, 1);
        ALTER_GRAPH(&ir, persistenceRestoreEdgeCount, 1);
    } else {
        ALTER_GRAPH(&ir, persistenceRestoreRootCount, 0);
        ALTER_GRAPH(&ir, persistenceRestoreRootCount, SIZE_MAX);
        ALTER_GRAPH(&ir, persistenceRestoreRootCapacity, 0);
        ALTER_GRAPH(&ir, persistenceRestoreRoots, NULL);
        ALTER_GRAPH(&ir, persistenceRestoreNodeCount, 0);
        ALTER_GRAPH(&ir, persistenceRestoreNodeCapacity, 0);
        ALTER_GRAPH(&ir, persistenceRestoreNodes, NULL);
        if (ir.persistenceRestoreEdgeCount) {
            ALTER_GRAPH(&ir, persistenceRestoreEdgeCount, 0);
            ALTER_GRAPH(&ir, persistenceRestoreEdgeCapacity, 0);
            ALTER_GRAPH(&ir, persistenceRestoreEdges, NULL);
        }
        for (size_t i = 0; i < ir.persistenceRestoreRootCount; ++i) {
            XL6_IrPersistenceRestoreRoot* row = &ir.persistenceRestoreRoots[i];
            assert(xl6IrPersistenceRestoreRootGet(&ir, row->type, row->format) == row);
            ALTER_GRAPH(row, type, XL6_IR_TYPE_INVALID); ALTER_GRAPH(row, format, 0);
            ALTER_GRAPH(row, pending, row->pending ^ XL6_IR_RESTORE_PENDING_LIST);
            ALTER_GRAPH(row, firstNode, SIZE_MAX); ALTER_GRAPH(row, nodeCount, SIZE_MAX);
            ALTER_GRAPH(row, root, SIZE_MAX); ALTER_GRAPH(row, firstEdge, SIZE_MAX);
            ALTER_GRAPH(row, edgeCount, SIZE_MAX); ALTER_GRAPH(row, seedCount, SIZE_MAX);
        }
        for (size_t i = 0; i < ir.persistenceRestoreNodeCount; ++i) {
            XL6_IrPersistenceRestoreNode* row = &ir.persistenceRestoreNodes[i];
            ALTER_GRAPH(row, type, XL6_IR_TYPE_INVALID); ALTER_GRAPH(row, payloadType, XL6_IR_TYPE_INVALID);
            ALTER_GRAPH(row, abi, 0); ALTER_GRAPH(row, flags, row->flags ^ XL6_IR_RESTORE_NODE_NULLABLE);
            ALTER_GRAPH(row, flags, row->flags ^ XL6_IR_RESTORE_NODE_FIELD_INSTANCE);
            ALTER_GRAPH(row, identity, row->identity ^ UINT64_C(1));
            ALTER_GRAPH(row, contextField, row->contextField == SIZE_MAX ? 0 : SIZE_MAX);
            ALTER_GRAPH(row, scalarRule, row->scalarRule == XL6_VALUE_ADMIT_NONE ? XL6_VALUE_ADMIT_ANY : XL6_VALUE_ADMIT_NONE);
            ALTER_GRAPH(row, carrierAbi, row->carrierAbi == XL6_IR_COLLECTION_CARRIER_NONE
                ? XL6_IR_COLLECTION_CARRIER_SEQUENCE_V1 : XL6_IR_COLLECTION_CARRIER_NONE);
            ALTER_GRAPH(row, seed, row->seed == SIZE_MAX ? 0 : SIZE_MAX);
            ALTER_GRAPH(row, element, row->element == SIZE_MAX ? 0 : SIZE_MAX);
            ALTER_GRAPH(row, schema, row->schema == SIZE_MAX ? 0 : SIZE_MAX);
            ALTER_GRAPH(row, firstEdge, row->firstEdge == SIZE_MAX ? 0 : SIZE_MAX);
            ALTER_GRAPH(row, edgeCount, SIZE_MAX); ALTER_GRAPH(row, defaultFunction, UINT32_MAX);
            ALTER_GRAPH(row, defaultSymbol, row->defaultSymbol ^ UINT32_C(1));
            ALTER_GRAPH(row, defaultErrorAbi, row->defaultErrorAbi == XL6_IR_ERROR_ABI_NO_RAISE
                ? XL6_IR_ERROR_ABI_MAY_RAISE : XL6_IR_ERROR_ABI_NO_RAISE);
            ALTER_GRAPH(row, isolationAbi, row->isolationAbi == XL6_IR_PERSIST_ISOLATION_NONE
                ? XL6_IR_PERSIST_ISOLATION_CHECKED_REJECT : XL6_IR_PERSIST_ISOLATION_NONE);
            ALTER_GRAPH(row, cloneFunction, UINT32_MAX);
            ALTER_GRAPH(row, cloneSymbol, row->cloneSymbol ^ UINT32_C(1));
            ALTER_GRAPH(row, cloneErrorAbi, row->cloneErrorAbi == XL6_IR_ERROR_ABI_NO_RAISE
                ? XL6_IR_ERROR_ABI_MAY_RAISE : XL6_IR_ERROR_ABI_NO_RAISE);
            if (row->cloneFunction) {
                XL6_IrFunction* clone = &ir.functions[row->cloneFunction - 1];
                XL6_IrCallTarget* target = (XL6_IrCallTarget*)xl6IrCallTargetGet(&ir, row->cloneSymbol);
                assert(target && clone->symbol == target->symbol && clone->parameterCount == 1);
                ALTER_GRAPH(clone, kind, XL6_IR_FUNCTION_CLASS_DEFAULT);
                ALTER_GRAPH(clone, declaration, XL6_IR_NODE_INVALID);
                ALTER_GRAPH(clone, returnType, XL6_IR_TYPE_INVALID);
                ALTER_GRAPH(clone, parameterCount, 0);
                ALTER_GRAPH(clone, errorAbi, XL6_IR_ERROR_ABI_INVALID);
                ALTER_OP(clone, reachability, XL6_IR_REACHABILITY_UNREACHABLE);
                ALTER_GRAPH(target, kind, XL6_IR_CALL_TARGET_INTERNAL_ASYNC);
                ALTER_GRAPH(target, returnType, XL6_IR_TYPE_INVALID);
                ALTER_GRAPH(target, parameterCount, 0);
                ALTER_GRAPH(target, parameterTypes, NULL);
                ALTER_GRAPH(target, parameterAbis, NULL);
                ALTER_GRAPH(target, parameterPhysicalAbis, NULL);
                ALTER_GRAPH(target, parameterTypes[0], XL6_IR_TYPE_INVALID);
                ALTER_GRAPH(target, parameterAbis[0], XL6_IR_PARAMETER_ABI_INVALID);
                ALTER_GRAPH(target, parameterPhysicalAbis[0], XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW);
                ALTER_GRAPH(target, resultPhysicalAbi, XL6_IR_CALL_RESULT_PHYSICAL_NATIVE_CLASS_FIELD_TAKE);
                ALTER_GRAPH(target, resultConversionAbi, XL6_IR_CALL_RESULT_CONVERSION_BORROWED_STRING_COPY);
                ALTER_GRAPH(target, resultCAbi, XL6_IR_CALL_RESULT_C_ABI_CHAR_POINTER);
                ALTER_GRAPH(target, errorAbi, XL6_IR_ERROR_ABI_INVALID);
                ALTER_GRAPH(target, errorPhysicalAbi, XL6_IR_CALL_ERROR_PHYSICAL_INVALID);
                ALTER_OP(target, reachability, XL6_IR_REACHABILITY_UNREACHABLE);
            }
        }
        for (size_t i = 0; i < ir.persistenceRestoreEdgeCount; ++i) {
            XL6_IrPersistenceRestoreEdge* row = &ir.persistenceRestoreEdges[i];
            ALTER_GRAPH(row, field, SIZE_MAX); ALTER_GRAPH(row, node, SIZE_MAX);
        }
    }
    unsigned graphMutations = mutations - graphStart;
    size_t allocationPrefixes = restoreAllocationPrefixes(&ir);
    for (size_t i = 0; i < ir.classLayoutCount; ++i) {
        XL6_IrClassLayout* layout = &ir.classLayouts[i];
        if (!layout->persistenceDefaultFunction) {
            ALTER_OP(layout, persistenceDefaultFunction, UINT32_MAX);
            ALTER_OP(layout, persistenceConstructionAbi, XL6_IR_PERSISTENCE_CONSTRUCTION_DECLARATION_DEFAULT);
            continue;
        }
        XL6_IrFunction* factory = &ir.functions[layout->persistenceDefaultFunction - 1];
        assert(factory->kind == XL6_IR_FUNCTION_CLASS_DEFAULT && factory->parameterCount == 0 &&
            factory->returnType == layout->type && factory->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE);
        ALTER_OP(layout, persistenceDefaultFunction, 0);
        ALTER_OP(layout, persistenceDefaultFunction, UINT32_MAX);
        ALTER_OP(layout, persistenceConstructionAbi, XL6_IR_PERSISTENCE_CONSTRUCTION_NONE);
        ALTER_OP(factory, kind, XL6_IR_FUNCTION_USER);
        ALTER_OP(factory, symbol, 0);
        ALTER_OP(factory, parameterCount, 1);
        ALTER_OP(factory, returnType, XL6_IR_TYPE_INVALID);
        ALTER_OP(factory, declaration, ir.root);
        ALTER_OP(factory, errorAbi, XL6_IR_ERROR_ABI_NO_RAISE);
        ALTER_OP(factory, reachability, factory->reachability == XL6_IR_REACHABILITY_UNREACHABLE
            ? XL6_IR_REACHABILITY_ROOT : XL6_IR_REACHABILITY_UNREACHABLE);
        for (size_t instruction = factory->firstInstruction;
             instruction < factory->firstInstruction + factory->instructionCount; ++instruction) {
            XL6_IrInstruction* row = &ir.instructions[instruction];
            if (row->kind == XL6_IR_INSTR_RETURN) ALTER_OP(row, operandCount, 0);
        }
    }
    size_t restoreImports = 0, restoreArrays = 0;
    for (size_t i = 0; i < ir.persistenceRestoreFieldCount; ++i)
        if (xl6IrTypeGet(&ir.types, ir.persistenceRestoreFields[i].payloadType)->persistenceRestoreCarrierAbi ==
            XL6_IR_COLLECTION_CARRIER_SEQUENCE_V1) ++restoreArrays;
    assert(xl6IrCgenVisitResidentSymbols(&ir, NULL, NULL, restoreResidentSymbol, &restoreImports));
    if (restoreArrays) assert(restoreImports);
    assert(xl6IrDynamicFieldsRuntimeBridgeErrorAbi(NULL) == XL6_IR_ERROR_ABI_INVALID &&
        xl6IrDynamicFieldsRuntimeBridgeErrorAbi("not_a_bridge") == XL6_IR_ERROR_ABI_INVALID);
    for (size_t i = 0; i < ir.callTargetCount; ++i) {
        XL6_IrCallTarget* target = &ir.callTargets[i];
        if (target->runtimeBridge != XL6_IR_RUNTIME_BRIDGE_DYNAMIC_FIELDS) continue;
        XL6_IrErrorAbi saved = target->errorAbi;
        assert(saved == xl6IrDynamicFieldsRuntimeBridgeErrorAbi(target->cName));
        assert(target->errorPhysicalAbi == (saved == XL6_IR_ERROR_ABI_MAY_RAISE
            ? XL6_IR_CALL_ERROR_PHYSICAL_XRT_THREAD_ERROR : XL6_IR_CALL_ERROR_PHYSICAL_NONE));
        target->errorAbi = saved == XL6_IR_ERROR_ABI_MAY_RAISE
            ? XL6_IR_ERROR_ABI_NO_RAISE : XL6_IR_ERROR_ABI_MAY_RAISE;
        rejectedOutput(&ir); target->errorAbi = saved;
    }
    if (!ir.persistenceRestorePlanCount) {
        assert(!ir.persistenceRestoreSchemaCount && !ir.persistenceRestoreFieldCount);
        ALTER_RESTORE(&ir, persistenceRestorePlanCount, 1);
    } else {
        ALTER_RESTORE(&ir, persistenceRestorePlanCount, 0);
        ALTER_RESTORE(&ir, persistenceRestorePlanCount, SIZE_MAX);
        ALTER_RESTORE(&ir, persistenceRestorePlans, NULL);
        ALTER_RESTORE(&ir, persistenceRestorePlanCapacity, 0);
        ALTER_RESTORE(&ir, persistenceRestoreSchemas, NULL);
        ALTER_RESTORE(&ir, persistenceRestoreSchemaCount, 0);
        if (ir.persistenceRestoreFieldCount) {
            ALTER_RESTORE(&ir, persistenceRestoreFields, NULL);
            ALTER_RESTORE(&ir, persistenceRestoreFieldCount, 0);
            ALTER_RESTORE(&ir, persistenceRestoreFieldCapacity, 0);
        }
        for (size_t i = 0; i < ir.persistenceRestorePlanCount; ++i) {
            XL6_IrPersistenceRestorePlan* row = &ir.persistenceRestorePlans[i];
            ALTER_RESTORE(row, format, XL6_IR_PERSIST_FORMAT_NONE);
            ALTER_RESTORE(row, firstSchema, SIZE_MAX); ALTER_RESTORE(row, schemaCount, SIZE_MAX);
        }
        for (size_t i = 0; i < ir.persistenceRestoreSchemaCount; ++i) {
            XL6_IrPersistenceRestoreSchema* row = &ir.persistenceRestoreSchemas[i];
            assert(row == xl6IrPersistenceRestoreSchemaGet(&ir,
                ir.classLayouts[row->classLayout].type,
                i < ir.persistenceRestorePlans[0].schemaCount
                    ? ir.persistenceRestorePlans[0].format : ir.persistenceRestorePlans[1].format));
            ALTER_RESTORE(row, classLayout, SIZE_MAX);
            ALTER_RESTORE(row, firstField, SIZE_MAX); ALTER_RESTORE(row, fieldCount, SIZE_MAX);
            ALTER_RESTORE(row, dynamicField, SIZE_MAX - 1);
            ALTER_RESTORE(row, dynamicHandleField, SIZE_MAX - 1);
            ALTER_RESTORE(row, dynamicDestroySymbol, UINT32_MAX);
        }
        for (size_t i = 0; i < ir.persistenceRestoreFieldCount; ++i) {
            XL6_IrPersistenceRestoreField* row = &ir.persistenceRestoreFields[i];
            ALTER_RESTORE(row, classField, SIZE_MAX);
            ALTER_RESTORE(row, payloadType, XL6_IR_TYPE_INVALID);
            ALTER_RESTORE(row, sourceKeyAbi, 0); ALTER_RESTORE(row, sourceKey, NULL);
            ALTER_RESTORE(row, sourceKey, "not-canonical"); ALTER_RESTORE(row, assignAbi, 0);
            ALTER_RESTORE(row, acceptNull, !row->acceptNull); ALTER_RESTORE(row, required, !row->required);
        }
        for (size_t i = 0; i < ir.types.count; ++i) {
            XL6_IrType* type = &ir.types.items[i];
            ALTER_OP(type, persistenceRestoreScalarAbi, type->persistenceRestoreScalarAbi == XL6_VALUE_ADMIT_NONE
                ? XL6_VALUE_ADMIT_ANY : XL6_VALUE_ADMIT_NONE);
            ALTER_OP(type, persistenceRestoreCarrierAbi, type->persistenceRestoreCarrierAbi == XL6_IR_COLLECTION_CARRIER_NONE
                ? XL6_IR_COLLECTION_CARRIER_SEQUENCE_V1 : XL6_IR_COLLECTION_CARRIER_NONE);
        }
        XL6_IrPersistenceRestorePlan* plans = ir.persistenceRestorePlans;
        XL6_IrPersistenceRestoreSchema* schemas = ir.persistenceRestoreSchemas;
        XL6_IrPersistenceRestoreField* fields = ir.persistenceRestoreFields;
        XL6_IrPersistenceRestoreRoot* roots = ir.persistenceRestoreRoots;
        XL6_IrPersistenceRestoreNode* nodes = ir.persistenceRestoreNodes;
        XL6_IrPersistenceRestoreEdge* edges = ir.persistenceRestoreEdges;
        size_t first = ir.functions[0].firstInstruction;
        ir.functions[0].firstInstruction = SIZE_MAX;
        assert(!xl6IrBuildPersistenceRestorePlans(&ir));
        assert(ir.persistenceRestorePlans == plans && ir.persistenceRestoreSchemas == schemas &&
            ir.persistenceRestoreFields == fields && ir.persistenceRestoreRoots == roots &&
            ir.persistenceRestoreNodes == nodes && ir.persistenceRestoreEdges == edges);
        ir.functions[0].firstInstruction = first;
        assert(xl6IrBuildPersistenceRestorePlans(&ir) && xl6IrVerifyModule(&ir, NULL));
    }
    for (size_t i = 0; i < ir.callTargetCount; ++i) {
        XL6_IrCallTarget* target = &ir.callTargets[i];
        if (target->kind == XL6_IR_CALL_TARGET_NATIVE_FACTORY &&
            (target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_JSON_WRITER ||
             target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_XLON_WRITER)) {
            assert(target->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE &&
                target->errorPhysicalAbi == XL6_IR_CALL_ERROR_PHYSICAL_XRT_THREAD_ERROR);
            ALTER_OP(target, errorAbi, XL6_IR_ERROR_ABI_NO_RAISE);
            ALTER_OP(target, errorPhysicalAbi, XL6_IR_CALL_ERROR_PHYSICAL_NONE);
        }
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
        } else if (instruction->runtimeOp == XL6_IR_RUNTIME_PERSISTENCE_JSON_PARSE ||
                   instruction->runtimeOp == XL6_IR_RUNTIME_PERSISTENCE_XLON_PARSE ||
                   instruction->runtimeOp == XL6_IR_RUNTIME_PERSISTENCE_JSON_READ ||
                   instruction->runtimeOp == XL6_IR_RUNTIME_PERSISTENCE_XLON_READ) {
            assert(xl6IrRuntimeContainerAllocationErrorPhysicalAbi(instruction->runtimeOp) ==
                XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT);
            assert(i + 1 < ir.instructionCount);
            XL6_IrInstruction* edge = &ir.instructions[i + 1];
            assert(edge->kind == XL6_IR_INSTR_ERROR_TEST);
            ALTER_OP(instruction, payloadType, XL6_IR_TYPE_INVALID);
            ALTER_OP(instruction, operandCount, 0);
            ALTER_OP(edge, kind, XL6_IR_INSTR_VALUE);
        } else if (xl6IrPersistenceInvocationRuntime(instruction->runtimeOp) != NULL) {
            const XL6_IrPersistenceInvocationContract* row = xl6IrPersistenceInvocationRuntime(instruction->runtimeOp);
            assert(xl6IrPersistenceInvocationInstructionAbi(&ir, instruction));
            assert(xl6IrPersistenceInvocationErrorPhysicalAbi(instruction->runtimeOp) ==
                (row->action == XL6_IR_PERSISTENCE_INVOKE_ENCODE ? XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT :
                    XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT));
            ALTER_OP(instruction, payloadType, XL6_IR_TYPE_INVALID);
            ALTER_OP(instruction, operandCount, 0);
            ALTER_OP(instruction, targetSymbol, 1);
            ALTER_OP(instruction, type, definition(&ir, ir.operands[instruction->firstOperand])->type);
            if (row->action != XL6_IR_PERSISTENCE_INVOKE_ENCODE) {
                assert(i + 1 < ir.instructionCount);
                XL6_IrInstruction* edge = &ir.instructions[i + 1];
                assert(edge->kind == XL6_IR_INSTR_ERROR_TEST && edge->operandCount == 1 &&
                    ir.operands[edge->firstOperand] == instruction->result);
                ALTER_OP(edge, kind, XL6_IR_INSTR_VALUE);
                if (instruction->operandCount == 3) {
                    XL6_IrValueId* destination = &ir.operands[instruction->firstOperand + 2];
                    XL6_IrValueId saved = *destination;
                    *destination = ir.operands[instruction->firstOperand];
                    rejectedOutput(&ir); *destination = saved;
                }
            }
            if (xl6IrPersistenceInvocationIsWriter(row)) {
                ALTER_OP(instruction, runtimeIdentityType, XL6_IR_TYPE_INVALID);
                XL6_IrInstruction* handle = (XL6_IrInstruction*)definition(&ir,
                    ir.operands[instruction->firstOperand + 1]);
                assert(handle && handle->runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_NATIVE_HANDLE_BORROW);
                ALTER_OP(handle, immediateText, "\"not-the-writer-field\"");
                ALTER_OP(handle, runtimeOp, XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_RAW_POINTER);
            }
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
        if (intrinsic->kind == XL6_IR_INTRINSIC_PERSISTENCE_JSON_PARSE ||
            intrinsic->kind == XL6_IR_INTRINSIC_PERSISTENCE_XLON_PARSE ||
            intrinsic->kind == XL6_IR_INTRINSIC_PERSISTENCE_JSON_READ ||
            intrinsic->kind == XL6_IR_INTRINSIC_PERSISTENCE_XLON_READ) {
            assert(intrinsic->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE);
            ALTER_OP(intrinsic, errorAbi, XL6_IR_ERROR_ABI_NO_RAISE);
        }
        int legacy = intrinsic->kind == XL6_IR_INTRINSIC_PERSISTENCE_JSON_STRINGIFY ||
            intrinsic->kind == XL6_IR_INTRINSIC_PERSISTENCE_XLON_STRINGIFY ||
            intrinsic->kind == XL6_IR_INTRINSIC_PERSISTENCE_JSON_WRITE ||
            intrinsic->kind == XL6_IR_INTRINSIC_PERSISTENCE_XLON_WRITE;
        if (xl6IrPersistenceInvocationIntrinsic(intrinsic->kind) == NULL && !legacy) continue;
        if (legacy && intrinsic->admission == XL6_IR_INTRINSIC_ADMISSION_VERIFIED)
            assert(xl6IrPersistenceLegacyIntrinsicAbi(&ir, intrinsic));
        ALTER_OP(intrinsic, errorAbi, XL6_IR_ERROR_ABI_NO_RAISE);
        ALTER_OP(intrinsic, resultAbi, intrinsic->resultAbi == XL6_IR_INTRINSIC_RESULT_VALUE
            ? XL6_IR_INTRINSIC_RESULT_OWNED : XL6_IR_INTRINSIC_RESULT_VALUE);
        ALTER_OP(intrinsic, payloadType, XL6_IR_TYPE_INVALID);
        ALTER_OP(intrinsic, payloadAbi, XL6_IR_ASYNC_RESULT_VOID);
        ALTER_OP(intrinsic, persistenceSourceOperand, SIZE_MAX);
        ALTER_OP(intrinsic, persistenceSourceOperand, 1 - intrinsic->persistenceSourceOperand);
        ALTER_OP(intrinsic, persistenceOptionsOperand, intrinsic->persistenceOptionsOperand == SIZE_MAX
            ? 0 : SIZE_MAX);
        if (xl6IrPersistenceInvocationIsWriter(xl6IrPersistenceInvocationIntrinsic(intrinsic->kind))) {
            ALTER_OP(intrinsic, receiverType, XL6_IR_TYPE_INVALID);
            ALTER_OP(intrinsic, receiver, XL6_IR_NODE_INVALID);
        }
        if (intrinsic->persistenceDestinationOperand != SIZE_MAX) {
            ALTER_OP(intrinsic, persistenceDestinationOperand, SIZE_MAX);
            ALTER_OP(intrinsic, persistenceDestinationOperand, intrinsic->persistenceSourceOperand);
            XL6_IrIntrinsicOperand* destination = &ir.intrinsicOperands[
                intrinsic->firstOperand + intrinsic->persistenceDestinationOperand];
            ALTER_OP(destination, abi, XL6_IR_INTRINSIC_OPERAND_CONSUME);
        }
        XL6_IrIntrinsicOperand* source = &ir.intrinsicOperands[intrinsic->firstOperand + intrinsic->persistenceSourceOperand];
        ALTER_OP(source, abi, XL6_IR_INTRINSIC_OPERAND_CONSUME);
        if (legacy && intrinsic->persistenceOptionsOperand != SIZE_MAX) {
            XL6_IrIntrinsicOperand* pretty = &ir.intrinsicOperands[
                intrinsic->firstOperand + intrinsic->persistenceOptionsOperand];
            ALTER_OP(pretty, type, XL6_IR_TYPE_INVALID);
            ALTER_OP(pretty, abi, XL6_IR_INTRINSIC_OPERAND_CONSUME);
        }
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
    printf("Restore plans: %zu plans, %zu schemas, %zu fields\n",
        ir.persistenceRestorePlanCount, ir.persistenceRestoreSchemaCount, ir.persistenceRestoreFieldCount);
    printf("Restore graphs: %zu roots, %zu nodes, %zu edges; %u graph mutations refused before C/header\n",
        ir.persistenceRestoreRootCount, ir.persistenceRestoreNodeCount, ir.persistenceRestoreEdgeCount, graphMutations);
    printf("Restore rebuild: %zu exhaustive allocation-failure prefixes preserved all six tables\n", allocationPrefixes);
    ok = 1;
done:
    for (size_t i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d: %s\n",
        parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return !ok;
}
