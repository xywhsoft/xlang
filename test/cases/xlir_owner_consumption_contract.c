#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t rejected, transfers, graphChecks;

static const XL6_IrInstruction* definition(const XL6_IrModule* ir, XL6_IrValueId value)
{
    size_t i;
    for (i = 0; i < ir->instructionCount; ++i)
        if (value != 0 && ir->instructions[i].result == value) return &ir->instructions[i];
    return NULL;
}

static void reject(XL6_IrModule* ir, const char* label, int requireConsumption)
{
    XL6_DiagList diags = {0}; XL6_Output out; char* text; size_t i; int found = 0;
    if (xl6IrVerifyModule(ir, &diags)) { fprintf(stderr, "owner mutation escaped: %s\n", label); assert(0); }
    for (i = 0; i < diags.count; ++i)
        if (strstr(diags.items[i].message, "XLIR owner consumption:") != NULL) found = 1;
    assert(!requireConsumption || found);
    assert(diags.count != 0 && xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(text == NULL || *text == '\0'); xrtFree(text);
    assert(xl6OutputOpenMemory(&out) && !xl6IrCgenEmitHeader(&out, ir, "BAD_OWNER", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(text == NULL || *text == '\0'); xrtFree(text);
    xl6DiagFree(&diags); ++rejected;
}

/* Unit graphs exercise the transfer analysis independently of source lowering.
 * Types come from a real verified module; instructions, operand slices and
 * block layout are built explicitly. These are pass-level tests, not claims
 * that a synthetic graph meets all other module contracts. */
typedef struct Graph {
    XL6_IrModule ir;
    XL6_IrFunction function;
    XL6_IrInstruction instructions[32];
    XL6_IrBlock blocks[8];
    XL6_IrValueId operands[40];
    XL6_IrTypeId stringType;
} Graph;

static void begin(Graph* graph, const XL6_IrModule* source, XL6_IrTypeId stringType)
{
    memset(graph, 0, sizeof(*graph));
    graph->ir.types = source->types;
    graph->ir.stage = XL6_IR_STAGE_LOWERED;
    graph->ir.functions = &graph->function; graph->ir.functionCount = 1;
    graph->ir.instructions = graph->instructions;
    graph->ir.blocks = graph->blocks; graph->ir.operands = graph->operands;
    graph->function.entryBlock = 1; graph->stringType = stringType;
}

static void block(Graph* graph)
{
    graph->blocks[graph->ir.blockCount++].firstInstruction = graph->ir.instructionCount;
    graph->function.blockCount = graph->ir.blockCount;
}

static XL6_IrInstruction* emit(Graph* graph, XL6_IrInstructionKind kind, XL6_IrValueId result,
    XL6_IrValueId input)
{
    XL6_IrInstruction* instruction = &graph->instructions[graph->ir.instructionCount++];
    instruction->kind = kind; instruction->result = result;
    instruction->type = graph->stringType;
    instruction->firstOperand = graph->ir.operandCount;
    if (input != 0) {
        instruction->operandCount = 1;
        graph->operands[graph->ir.operandCount++] = input;
    }
    if (kind == XL6_IR_INSTR_RUNTIME_CALL) instruction->runtimeOp = XL6_IR_RUNTIME_STRING_DUP;
    graph->blocks[graph->ir.blockCount - 1].instructionCount++;
    graph->function.instructionCount = graph->ir.instructionCount;
    return instruction;
}

static void jump(Graph* graph, size_t target)
{
    emit(graph, XL6_IR_INSTR_JUMP, 0, 0)->trueTarget = (XL6_IrBlockId)target;
}

static void check(Graph* graph, int expected)
{
    XL6_DiagList diags = {0}; int result = xl6IrVerifyOwnerConsumption(&graph->ir, &diags);
    if (result != expected) fprintf(stderr, "ownership graph %llu: expected %d, got %d\n",
        (unsigned long long)graphChecks, expected, result);
    assert(result == expected && (expected || diags.count != 0));
    xl6DiagFree(&diags); ++graphChecks;
}

static void graphs(const XL6_IrModule* source, XL6_IrTypeId stringType)
{
    Graph g; XL6_IrInstruction* instruction;
    /* Two branch sinks share one dominating owner, but never one path. */
    begin(&g, source, stringType); block(&g);
    emit(&g, XL6_IR_INSTR_RUNTIME_CALL, 1, 0);
    instruction = emit(&g, XL6_IR_INSTR_BRANCH, 0, 0); instruction->trueTarget = 2; instruction->falseTarget = 3;
    block(&g); emit(&g, XL6_IR_INSTR_STORE, 0, 1); jump(&g, 4);
    block(&g); emit(&g, XL6_IR_INSTR_STORE, 0, 1); jump(&g, 4);
    block(&g); emit(&g, XL6_IR_INSTR_RETURN, 0, 0); check(&g, 1);
    /* A consumer after the merge is a second consume on both paths. */
    g.instructions[6].operandCount = 1; g.instructions[6].firstOperand = g.ir.operandCount;
    g.operands[g.ir.operandCount++] = 1; check(&g, 0);

    /* An available/consumed merge is also unsafe, regardless of visit order. */
    begin(&g, source, stringType); block(&g); emit(&g, XL6_IR_INSTR_RUNTIME_CALL, 1, 0);
    instruction = emit(&g, XL6_IR_INSTR_BRANCH, 0, 0); instruction->trueTarget = 2; instruction->falseTarget = 3;
    block(&g); emit(&g, XL6_IR_INSTR_STORE, 0, 1); jump(&g, 3);
    block(&g); emit(&g, XL6_IR_INSTR_STORE, 0, 1); emit(&g, XL6_IR_INSTR_RETURN, 0, 0); check(&g, 0);
    g.instructions[1].trueTarget = 3; g.instructions[1].falseTarget = 2; check(&g, 0);

    /* Reacquire in the loop body, including when the body is entry. */
    begin(&g, source, stringType); block(&g);
    emit(&g, XL6_IR_INSTR_RUNTIME_CALL, 1, 0); emit(&g, XL6_IR_INSTR_STORE, 0, 1); jump(&g, 1); check(&g, 1);
    /* Moving acquisition outside the backedge makes repetition invalid. */
    begin(&g, source, stringType); block(&g); emit(&g, XL6_IR_INSTR_RUNTIME_CALL, 1, 0); jump(&g, 2);
    block(&g); emit(&g, XL6_IR_INSTR_STORE, 0, 1); jump(&g, 2); check(&g, 0);

    /* All transparent spellings preserve the one token. */
    begin(&g, source, stringType); block(&g); emit(&g, XL6_IR_INSTR_RUNTIME_CALL, 1, 0);
    emit(&g, XL6_IR_INSTR_VALUE, 2, 1); emit(&g, XL6_IR_INSTR_NULLABLE_WRAP, 3, 2);
    emit(&g, XL6_IR_INSTR_NULLABLE_UNWRAP, 4, 3); emit(&g, XL6_IR_INSTR_STORE, 0, 4);
    emit(&g, XL6_IR_INSTR_STORE, 0, 1); emit(&g, XL6_IR_INSTR_RETURN, 0, 0); check(&g, 0);
    /* A copy has its own token and does not consume the observed input. */
    g.instructions[1].kind = XL6_IR_INSTR_RUNTIME_CALL; g.instructions[1].runtimeOp = XL6_IR_RUNTIME_STRING_COPY;
    check(&g, 1);

    /* An undetached LOAD is a borrow; one MOVE acquires exactly once. */
    begin(&g, source, stringType); block(&g); emit(&g, XL6_IR_INSTR_LOAD, 1, 0);
    emit(&g, XL6_IR_INSTR_STORE, 0, 1); check(&g, 0);
    g.instructions[1].kind = XL6_IR_INSTR_MOVE;
    emit(&g, XL6_IR_INSTR_STORE, 0, 1); check(&g, 1);
    emit(&g, XL6_IR_INSTR_STORE, 0, 1); check(&g, 0);
    g.instructions[3].kind = XL6_IR_INSTR_MOVE; check(&g, 0);
    /* A repeated LOAD starts a new SSA observation. Whether the underlying
     * storage was reactivated is a separate storage-liveness obligation. */
    begin(&g, source, stringType); block(&g); emit(&g, XL6_IR_INSTR_LOAD, 1, 0);
    emit(&g, XL6_IR_INSTR_MOVE, 0, 1); emit(&g, XL6_IR_INSTR_STORE, 0, 1); jump(&g, 1); check(&g, 1);

    /* Two consuming operands of a single product still share one path. */
    begin(&g, source, stringType); block(&g); emit(&g, XL6_IR_INSTR_RUNTIME_CALL, 1, 0);
    instruction = emit(&g, XL6_IR_INSTR_PRODUCT_MAKE, 2, 1);
    instruction->operandCount++; g.operands[g.ir.operandCount++] = 1; check(&g, 0);

    /* Enumerating consumers is insufficient: this owner has no use at all. */
    begin(&g, source, stringType); block(&g);
    emit(&g, XL6_IR_INSTR_RUNTIME_CALL, 1, 0);
    emit(&g, XL6_IR_INSTR_RETURN, 0, 0); check(&g, 0);
    /* Merely observing or aliasing the result does not discharge it. */
    g.instructions[1].kind = XL6_IR_INSTR_NULL_TEST;
    g.instructions[1].operandCount = 1; g.instructions[1].firstOperand = g.ir.operandCount;
    g.operands[g.ir.operandCount++] = 1;
    emit(&g, XL6_IR_INSTR_RETURN, 0, 0); check(&g, 0);

    /* Reentering an acquisition with its previous owner live loses a value,
     * even if the CFG has no finite exit. */
    begin(&g, source, stringType); block(&g);
    emit(&g, XL6_IR_INSTR_RUNTIME_CALL, 1, 0); jump(&g, 1); check(&g, 0);

    /* One branch consumes, the other silently exits: not a complete owner. */
    begin(&g, source, stringType); block(&g);
    emit(&g, XL6_IR_INSTR_RUNTIME_CALL, 1, 0);
    instruction = emit(&g, XL6_IR_INSTR_BRANCH, 0, 0);
    instruction->trueTarget = 2; instruction->falseTarget = 3;
    block(&g); emit(&g, XL6_IR_INSTR_STORE, 0, 1); jump(&g, 3);
    block(&g); emit(&g, XL6_IR_INSTR_RETURN, 0, 0); check(&g, 0);

    /* Checked NULL-result failure never acquires an owner. The success edge
     * must still consume; an arbitrary branch cannot erase this obligation. */
    g.instructions[1].kind = XL6_IR_INSTR_ERROR_TEST;
    g.instructions[1].operandCount = 1; g.instructions[1].firstOperand = g.ir.operandCount;
    g.operands[g.ir.operandCount++] = 1; check(&g, 1);
    /* Borrowed string acquisition now has the same checked failure contract
     * as a literal allocation. An unrelated branch still cannot erase it. */
    g.instructions[0].runtimeOp = XL6_IR_RUNTIME_STRING_COPY; check(&g, 1);
    g.instructions[1].kind = XL6_IR_INSTR_BRANCH; check(&g, 0);
    g.instructions[1].kind = XL6_IR_INSTR_ERROR_TEST;
    g.instructions[0].runtimeOp = XL6_IR_RUNTIME_STRING_DUP;
    g.instructions[2].kind = XL6_IR_INSTR_NULL_TEST; check(&g, 0);

    /* A detached storage observation also needs its final sink. */
    begin(&g, source, stringType); block(&g);
    emit(&g, XL6_IR_INSTR_LOAD, 1, 0);
    emit(&g, XL6_IR_INSTR_MOVE, 0, 1);
    emit(&g, XL6_IR_INSTR_RETURN, 0, 0); check(&g, 0);

    /* NULL is a duplicable empty owner, including through a transparent alias. */
    begin(&g, source, stringType); block(&g); emit(&g, XL6_IR_INSTR_NULL, 1, 0);
    emit(&g, XL6_IR_INSTR_VALUE, 2, 1); emit(&g, XL6_IR_INSTR_STORE, 0, 1);
    emit(&g, XL6_IR_INSTR_STORE, 0, 2); check(&g, 1);
}

#include "xlir_product_owner_graphs.h"

static void moduleMutations(const char* path)
{
    XL6_ParseResult parse; XL6_IrModule ir; size_t b, i, j, p, q, before = rejected;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile(path, &parse) && xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "owner_consumption"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    assert(xl6IrLowerModule(&ir, &parse.diags) && xl6IrVerifyModule(&ir, &parse.diags));
    for (b = 0; b < ir.blockCount; ++b) {
        const XL6_IrBlock* blockInfo = &ir.blocks[b];
        size_t limit = blockInfo->firstInstruction + blockInfo->instructionCount;
        for (i = blockInfo->firstInstruction; i < limit; ++i) {
            XL6_IrInstruction* first = &ir.instructions[i];
            for (p = 0; p < first->operandCount; ++p) {
                XL6_IrValueId owner = ir.operands[first->firstOperand + p];
                const XL6_IrInstruction* producer = definition(&ir, owner);
                const XL6_IrType* type = producer != NULL ? xl6IrTypeGet(&ir.types, producer->type) : NULL;
                if (xl6IrInstructionOperandOwnership(&ir, first, p) != XL6_IR_OPERAND_OWNERSHIP_CONSUME ||
                    type == NULL || type->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
                    producer->kind == XL6_IR_INSTR_NULL || producer->kind == XL6_IR_INSTR_VALUE ||
                    producer->kind == XL6_IR_INSTR_NULLABLE_WRAP || producer->kind == XL6_IR_INSTR_NULLABLE_UNWRAP ||
                    xl6IrValueOwnershipGet(&ir, owner) != XL6_IR_OWNERSHIP_MOVE) continue;
                ++transfers;
                for (j = i + 1; j < limit; ++j) {
                    XL6_IrInstruction* second = &ir.instructions[j];
                    for (q = 0; q < second->operandCount; ++q) {
                        XL6_IrValueId saved = ir.operands[second->firstOperand + q];
                        const XL6_IrInstruction* original = definition(&ir, saved);
                        if (original == NULL || original->type != producer->type ||
                            xl6IrInstructionOperandOwnership(&ir, second, q) != XL6_IR_OPERAND_OWNERSHIP_CONSUME) continue;
                        ir.operands[second->firstOperand + q] = owner;
                        reject(&ir, "same-path repeated managed transfer", 0);
                        ir.operands[second->firstOperand + q] = saved;
                    }
                }
            }
        }
    }
    if (strstr(path, "managed_storage_inputs") != NULL) {
        XL6_IrValueId saved, owner = 0; XL6_IrTypeId stringType = 0; size_t second = SIZE_MAX;
        XL6_SymbolId firstStorage = 0; XL6_IrNodeId functionOwner = 0;
        /* Reproduce the original local -> later local counterexample
         * without depending on instruction, symbol or SSA arena numbering. */
        for (i = 0; i < ir.instructionCount; ++i) {
            const XL6_IrInstruction* store = &ir.instructions[i];
            const XL6_IrStorage* slot = xl6IrStorageGet(&ir, store->targetSymbol);
            const XL6_IrType* type = slot != NULL ? xl6IrTypeGet(&ir.types, slot->type) : NULL;
            const XL6_IrInstruction* producer;
            if (store->kind != XL6_IR_INSTR_STORE || slot == NULL || slot->kind != XL6_IR_STORAGE_LOCAL ||
                type == NULL || type->cAbi != XL6_IR_C_ABI_MANAGED_STRING) continue;
            producer = definition(&ir, ir.operands[store->firstOperand]); assert(producer != NULL);
            if (owner == 0) {
                owner = producer->result; stringType = producer->type;
                firstStorage = slot->symbol; functionOwner = slot->owner;
            } else if (slot->type == stringType && slot->symbol != firstStorage && slot->owner == functionOwner) {
                second = i; break;
            }
        }
        assert(owner != 0 && second != SIZE_MAX);
        saved = ir.operands[ir.instructions[second].firstOperand];
        ir.operands[ir.instructions[second].firstOperand] = owner;
        reject(&ir, "previously accepted cross-block duplicate STORE", 1);
        ir.operands[ir.instructions[second].firstOperand] = saved;
        graphs(&ir, stringType);
    }
    assert(xl6IrVerifyModule(&ir, &parse.diags));
    printf("%s: %llu rejected owner mutations\n", path, (unsigned long long)(rejected - before));
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
}

static int generate(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; size_t i; int ok = 0;
    assert(argc == 5); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile(argv[1], &parse) || !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "owner_completion") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto done;
    if (!xl6OutputOpenFile(&out, argv[2])) goto done;
    ok = xl6IrCgenEmitC(&out, &ir, &parse.diags); xl6OutputClose(&out);
    if (!ok || !xl6OutputOpenFile(&out, argv[3])) { ok = 0; goto done; }
    ok = xl6IrCgenEmitHeader(&out, &ir, "OWNER_COMPLETION", &parse.diags); xl6OutputClose(&out);
    if (!ok || !xl6OutputOpenFile(&out, argv[4])) { ok = 0; goto done; }
    xl6IrDumpModuleJson(&out, &ir); ok = !xl6OutputHasFailed(&out); xl6OutputClose(&out);
done:
    for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d: %s\n",
        parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return !ok;
}

int main(int argc, char** argv)
{
    size_t op;
    if (argc != 1) return generate(argc, argv);
    for (op = 1; op <= XL6_IR_RUNTIME_CLASS_FIELD_INIT_NEW; ++op)
        assert(xl6IrRuntimeOperandOwnership((XL6_IrRuntimeOp)op, 0) != XL6_IR_OPERAND_OWNERSHIP_INVALID);
    assert(xl6IrRuntimeOperandOwnership(XL6_IR_RUNTIME_NONE, 0) == XL6_IR_OPERAND_OWNERSHIP_INVALID);
    assert(xl6IrRuntimeOperandOwnership((XL6_IrRuntimeOp)(XL6_IR_RUNTIME_CLASS_FIELD_INIT_NEW + 1), 0)
        == XL6_IR_OPERAND_OWNERSHIP_INVALID);
    assert(xl6IrRuntimeOperandOwnership(XL6_IR_RUNTIME_XVALUE_TYPE_BIND, 0) == XL6_IR_OPERAND_OWNERSHIP_CONSUME);
    assert(xl6IrRuntimeOperandOwnership(XL6_IR_RUNTIME_XVALUE_ARRAY_APPEND_NEW, 0) == XL6_IR_OPERAND_OWNERSHIP_OBSERVE);
    assert(xl6IrRuntimeOperandOwnership(XL6_IR_RUNTIME_XVALUE_ARRAY_APPEND_NEW, 1) == XL6_IR_OPERAND_OWNERSHIP_CONSUME);
    assert(xl6IrRuntimeOperandOwnership(XL6_IR_RUNTIME_XVALUE_OBJECT_SET_STRING_NEW, 1) == XL6_IR_OPERAND_OWNERSHIP_OBSERVE);
    assert(xl6IrRuntimeOperandOwnership(XL6_IR_RUNTIME_XVALUE_OBJECT_SET_STRING_NEW, 2) == XL6_IR_OPERAND_OWNERSHIP_CONSUME);
    assert(xl6IrRuntimeOperandOwnership(XL6_IR_RUNTIME_XVALUE_DYNAMIC_CALL, 4) == XL6_IR_OPERAND_OWNERSHIP_CONSUME);
    moduleMutations("test/cases/xlir_managed_storage_inputs.xl");
    moduleMutations("test/cases/xlir_parameter_cleanup_outcome.xl");
    moduleMutations("test/cases/xlir_loop_cleanup_outcome.xl");
    moduleMutations("release/examples/error_model.xl");
    productMutations();
    assert(rejected >= 14 && transfers >= 20 && graphChecks >= 46);
    printf("owner consumption: %llu RuntimeOps classified, %llu consuming inputs, %llu rejected mutations, %llu CFG checks\n",
        (unsigned long long)(op - 1), (unsigned long long)transfers, (unsigned long long)rejected, (unsigned long long)graphChecks);
    return 0;
}
