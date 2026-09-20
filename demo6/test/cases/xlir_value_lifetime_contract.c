#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t mutations, graphChecks;
static void rejectModule(XL6_IrModule* ir)
{
    XL6_DiagList diags = {0}; XL6_Output out; size_t i; int found = 0; char* text;
    int valid = xl6IrVerifyModule(ir, &diags);
    if (valid) fprintf(stderr, "value lifetime mutation %llu unexpectedly accepted\n", (unsigned long long)mutations);
    assert(!valid);
    for (i = 0; i < diags.count; ++i)
        if (strstr(diags.items[i].message, "XLIR value lifetime:") != NULL) found = 1;
    assert(found && xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(text == NULL || *text == '\0'); xrtFree(text);
    assert(xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitHeader(&out, ir, "INVALID_LOAN", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(text == NULL || *text == '\0'); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}

/* Abstract CFG tests exercise this pass only. Full-module mutation tests
 * below separately prove structural validity and the C/header output gate. */
typedef struct Graph {
    XL6_IrModule ir; XL6_IrFunction function;
    XL6_IrInstruction instructions[32]; XL6_IrBlock blocks[8];
    XL6_IrStorage storages[2]; XL6_IrValueId operands[40]; XL6_IrTypeId type;
} Graph;
static void begin(Graph* g, const XL6_IrModule* source, XL6_IrTypeId type)
{
    memset(g, 0, sizeof(*g)); g->ir.types = source->types; g->type = type;
    g->ir.stage = XL6_IR_STAGE_LOWERED; g->ir.functions = &g->function; g->ir.functionCount = 1;
    g->ir.instructions = g->instructions; g->ir.blocks = g->blocks; g->ir.operands = g->operands;
    g->ir.storages = g->storages; g->ir.storageCount = 2;
    g->storages[0].symbol = 1; g->storages[0].type = type; g->storages[0].kind = XL6_IR_STORAGE_LOCAL;
    g->storages[1] = g->storages[0]; g->storages[1].symbol = 2;
    g->function.entryBlock = 1;
}
static void block(Graph* g)
{
    g->blocks[g->ir.blockCount++].firstInstruction = g->ir.instructionCount;
    g->function.blockCount = g->ir.blockCount;
}
static XL6_IrInstruction* emit(Graph* g, XL6_IrInstructionKind kind, XL6_IrValueId value,
    XL6_SymbolId symbol, XL6_IrValueId operand)
{
    XL6_IrInstruction* x = &g->instructions[g->ir.instructionCount++];
    x->kind = kind; x->result = value; x->type = g->type; x->targetSymbol = symbol;
    x->firstOperand = g->ir.operandCount;
    if (operand) { x->operandCount = 1; g->operands[g->ir.operandCount++] = operand; }
    if (value > g->ir.valueCount) g->ir.valueCount = value;
    if (kind == XL6_IR_INSTR_RUNTIME_CALL) x->runtimeOp = operand ? XL6_IR_RUNTIME_STRING_COPY : XL6_IR_RUNTIME_STRING_DUP;
    ++g->blocks[g->ir.blockCount - 1].instructionCount;
    g->function.instructionCount = g->ir.instructionCount; return x;
}
static void check(Graph* g, int expected)
{
    XL6_DiagList diags = {0}; int actual = xl6IrVerifyValueLifetimes(&g->ir, &diags);
    if (actual != expected) fprintf(stderr, "loan graph %llu expected %d, got %d\n", (unsigned long long)graphChecks, expected, actual);
    assert(actual == expected && (expected || diags.count != 0)); xl6DiagFree(&diags); ++graphChecks;
}
static void graphs(const XL6_IrModule* source, XL6_IrTypeId type)
{
    Graph g; XL6_IrInstruction* edge;
    begin(&g, source, type); block(&g);
    emit(&g, XL6_IR_INSTR_LOAD, 1, 1, 0); emit(&g, XL6_IR_INSTR_RUNTIME_CALL, 2, 0, 1);
    emit(&g, XL6_IR_INSTR_RETURN, 0, 0, 0); check(&g, 1);
    /* RELEASE kills a storage-backed SSA, even if the slot is later refilled. */
    g.instructions[1].kind = XL6_IR_INSTR_RELEASE; g.instructions[1].result = 0;
    g.instructions[1].operandCount = 0; g.instructions[1].targetSymbol = 1;
    g.instructions[2].kind = XL6_IR_INSTR_RUNTIME_CALL; g.instructions[2].result = 2;
    g.instructions[2].runtimeOp = XL6_IR_RUNTIME_STRING_COPY; g.instructions[2].operandCount = 1;
    g.instructions[2].firstOperand = 0; check(&g, 0);
    /* MOVE detaches the loaded owner: observing it before consume is valid. */
    g.instructions[1].kind = XL6_IR_INSTR_MOVE; g.instructions[1].operandCount = 1;
    /* MOVE and COPY intentionally share an operand slice: effects belong to
     * instruction uses, not to the shared arena offset. */
    check(&g, 1);
    emit(&g, XL6_IR_INSTR_STORE, 0, 2, 1); emit(&g, XL6_IR_INSTR_RUNTIME_CALL, 3, 0, 1); check(&g, 0);

    begin(&g, source, type); block(&g);
    emit(&g, XL6_IR_INSTR_RUNTIME_CALL, 1, 0, 0); emit(&g, XL6_IR_INSTR_VALUE, 2, 0, 1);
    emit(&g, XL6_IR_INSTR_STORE, 0, 1, 1); emit(&g, XL6_IR_INSTR_RUNTIME_CALL, 3, 0, 2); check(&g, 0);
    /* A new LOAD starts a storage observation after transfer. */
    g.instructions[1].kind = XL6_IR_INSTR_LOAD; g.instructions[1].targetSymbol = 1;
    g.instructions[1].operandCount = 0;
    { XL6_IrInstruction swap = g.instructions[1]; g.instructions[1] = g.instructions[2]; g.instructions[2] = swap; }
    check(&g, 1);

    begin(&g, source, type); block(&g);
    emit(&g, XL6_IR_INSTR_LOAD, 1, 1, 0); emit(&g, XL6_IR_INSTR_CALL, 0, 0, 0);
    emit(&g, XL6_IR_INSTR_RUNTIME_CALL, 2, 0, 1); check(&g, 1);
    g.storages[0].kind = XL6_IR_STORAGE_GLOBAL; check(&g, 0);
    g.storages[0].kind = XL6_IR_STORAGE_LOCAL; g.instructions[0].kind = XL6_IR_INSTR_CELL_LOAD; check(&g, 0);

    /* Resuming a generator follows arbitrary caller execution, exactly like
     * an await. Private storage survives, shared global/cell loans do not. */
    begin(&g, source, type); block(&g);
    emit(&g, XL6_IR_INSTR_LOAD, 1, 1, 0); emit(&g, XL6_IR_INSTR_YIELD, 0, 0, 0);
    emit(&g, XL6_IR_INSTR_RUNTIME_CALL, 2, 0, 1); check(&g, 1);
    g.storages[0].kind = XL6_IR_STORAGE_GLOBAL; check(&g, 0);
    g.storages[0].kind = XL6_IR_STORAGE_EXTERN_GLOBAL; check(&g, 0);
    g.storages[0].kind = XL6_IR_STORAGE_LOCAL; g.instructions[0].kind = XL6_IR_INSTR_CELL_LOAD; check(&g, 0);

    begin(&g, source, type); block(&g); emit(&g, XL6_IR_INSTR_LOAD, 1, 1, 0);
    edge = emit(&g, XL6_IR_INSTR_BRANCH, 0, 0, 0); edge->trueTarget = 2; edge->falseTarget = 3;
    block(&g); emit(&g, XL6_IR_INSTR_RELEASE, 0, 1, 0); emit(&g, XL6_IR_INSTR_JUMP, 0, 0, 0)->trueTarget = 3;
    block(&g); emit(&g, XL6_IR_INSTR_RUNTIME_CALL, 2, 0, 1); check(&g, 0);
    g.instructions[4].kind = XL6_IR_INSTR_LOAD; g.instructions[4].operandCount = 0;
    g.instructions[4].result = 2; g.instructions[4].targetSymbol = 1;
    emit(&g, XL6_IR_INSTR_RUNTIME_CALL, 3, 0, 2); check(&g, 1);
    /* The SSA loan pass allows a fresh observation; separate storage flow
     * proves whether that LOAD reads an initialized slot. */

    begin(&g, source, type); block(&g); emit(&g, XL6_IR_INSTR_RUNTIME_CALL, 1, 0, 0);
    emit(&g, XL6_IR_INSTR_VALUE, 2, 0, 1); emit(&g, XL6_IR_INSTR_RUNTIME_CALL, 3, 0, 2);
    emit(&g, XL6_IR_INSTR_JUMP, 0, 0, 0)->trueTarget = 1; check(&g, 1);
    { XL6_IrInstruction swap = g.instructions[1]; g.instructions[1] = g.instructions[2]; g.instructions[2] = swap; }
    check(&g, 0);

    begin(&g, source, type); block(&g); g.storages[0].kind = XL6_IR_STORAGE_TEMPORARY;
    emit(&g, XL6_IR_INSTR_RUNTIME_CALL, 1, 0, 0); emit(&g, XL6_IR_INSTR_STORE, 0, 1, 1);
    emit(&g, XL6_IR_INSTR_RETURN, 0, 0, 0); check(&g, 0);
    g.instructions[2].kind = XL6_IR_INSTR_RELEASE; g.instructions[2].targetSymbol = 1;
    emit(&g, XL6_IR_INSTR_RETURN, 0, 0, 0); check(&g, 1);
}
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; size_t i, f;
    XL6_IrTypeId type = 0; int tested = 0;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/xlir_value_lifetime.xl", &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "value_lifetime"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    assert(xl6IrLowerModule(&ir, &parse.diags) && xl6IrVerifyModule(&ir, &parse.diags));
    /* Follow the checked success CFG, not arena adjacency. A copy can split
     * the initial STORE/LOAD from replacement and later use; a RELEASE in its
     * failure continuation cannot invalidate a loan on the success path.
     * Keep the first loan, not a fresh observation of the replacement. */
    for (f = 0; f < ir.functionCount && !tested; ++f) {
      const XL6_IrFunction* function = &ir.functions[f];
      XL6_IrBlockId next = function->entryBlock;
      size_t traversed = 0;
      XL6_SymbolId slot = 0;
      XL6_IrValueId original = 0, loan = 0, detached = 0;
      int invalidated = 0;
      while (next && !tested && traversed++ < function->blockCount) {
      const XL6_IrBlock* block = &ir.blocks[next - 1];
      const XL6_IrInstruction* terminal;
      assert(next - 1 >= function->firstBlock && next - 1 < function->firstBlock + function->blockCount);
      for (i = block->firstInstruction; i < block->firstInstruction + block->instructionCount; ++i) {
        XL6_IrInstruction* x = &ir.instructions[i]; const XL6_IrStorage* storage = xl6IrStorageGet(&ir, x->targetSymbol);
        const XL6_IrType* info = xl6IrTypeGet(&ir.types, x->type);
        if (!slot && x->kind == XL6_IR_INSTR_STORE && storage && storage->kind == XL6_IR_STORAGE_LOCAL &&
            info && info->cAbi == XL6_IR_C_ABI_MANAGED_STRING) {
            slot = x->targetSymbol; type = x->type; original = ir.operands[x->firstOperand];
        }
        if (slot && !loan && x->kind == XL6_IR_INSTR_LOAD && x->targetSymbol == slot) loan = x->result;
        if (slot && x->targetSymbol == slot &&
            (x->kind == XL6_IR_INSTR_RELEASE || x->kind == XL6_IR_INSTR_MOVE)) {
            invalidated = 1;
            if (x->kind == XL6_IR_INSTR_MOVE) detached = ir.operands[x->firstOperand];
        }
        if (invalidated && x->kind == XL6_IR_INSTR_RUNTIME_CALL && x->runtimeOp == XL6_IR_RUNTIME_STRING_COPY) {
            XL6_IrValueId saved = ir.operands[x->firstOperand]; assert(original && loan);
            ir.operands[x->firstOperand] = original; rejectModule(&ir);
            ir.operands[x->firstOperand] = loan; rejectModule(&ir);
            if (detached) { ir.operands[x->firstOperand] = detached; rejectModule(&ir); }
            ir.operands[x->firstOperand] = saved; tested = 1; break;
        }
      }
      assert(block->instructionCount);
      terminal = &ir.instructions[block->firstInstruction + block->instructionCount - 1];
      next = terminal->kind == XL6_IR_INSTR_ERROR_TEST || terminal->kind == XL6_IR_INSTR_JUMP
          ? terminal->trueTarget : XL6_IR_BLOCK_INVALID;
      }
    }
    assert(tested && xl6IrVerifyModule(&ir, &parse.diags)); graphs(&ir, type);
    {
        size_t f, scan; int suspendedLoans = 0; unsigned int operationMask = 0;
        for (f = 0; f < ir.functionCount; ++f) {
            const XL6_IrFunction* function = &ir.functions[f];
            XL6_IrValueId globalLoan = 0; int awaited = 0;
            if (function->kind != XL6_IR_FUNCTION_ASYNC_WORKER) continue;
            for (scan = function->firstInstruction; scan < function->firstInstruction + function->instructionCount; ++scan) {
                XL6_IrInstruction* x = &ir.instructions[scan];
                const XL6_IrStorage* storage = xl6IrStorageGet(&ir, x->targetSymbol);
                const XL6_IrType* info = xl6IrTypeGet(&ir.types, x->type);
                unsigned int operationBit = 0;
                if (x->kind == XL6_IR_INSTR_LOAD && storage && storage->kind == XL6_IR_STORAGE_GLOBAL &&
                    info && info->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR) {
                    globalLoan = x->result; awaited = 0;
                }
                if (x->kind == XL6_IR_INSTR_AWAIT && globalLoan) awaited = 1;
                if (!awaited || x->kind != XL6_IR_INSTR_RUNTIME_CALL) continue;
                switch (x->runtimeOp) {
                case XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_INTEGER: operationBit = 1u; break;
                case XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_OR_NULL: operationBit = 2u; break;
                case XL6_IR_RUNTIME_XVALUE_ARRAY_SET_I64_NEW: operationBit = 4u; break;
                case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_STRING: operationBit = 8u; break;
                case XL6_IR_RUNTIME_XVALUE_OBJECT_SET_STRING_NEW: operationBit = 16u; break;
                case XL6_IR_RUNTIME_XVALUE_INDEX_GET_XVALUE_KEY: operationBit = 32u; break;
                case XL6_IR_RUNTIME_XVALUE_INDEX_SET_XVALUE_KEY_NEW: operationBit = 64u; break;
                default: break;
                }
                if (operationBit) {
                    XL6_IrValueId saved = ir.operands[x->firstOperand];
                    assert(saved != globalLoan);
                    ir.operands[x->firstOperand] = globalLoan; rejectModule(&ir);
                    ir.operands[x->firstOperand] = saved;
                    ++suspendedLoans; operationMask |= operationBit; awaited = 0;
                }
            }
        }
        assert(suspendedLoans == 11 && operationMask == 127u && xl6IrVerifyModule(&ir, &parse.diags));
    }
    if (argc == 4) {
        XL6_Output out;
        XL6_SymbolId pending = 0;
        assert(xl6OutputOpenFile(&out, argv[1]));
        assert(xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
        assert(xl6OutputOpenFile(&out, argv[2]));
        assert(xl6IrCgenEmitHeader(&out, &ir, "XLIR_VALUE_LIFETIME", &parse.diags)); xl6OutputClose(&out);
        /* The host observes the fixture's one internal Future without
         * inventing a public Future export ABI or hard-coding a SymbolId. */
        for (i = 0; i < ir.storageCount; ++i) {
            const XL6_IrStorage* storage = &ir.storages[i];
            const XL6_IrType* info = xl6IrTypeGet(&ir.types, storage->type);
            if (storage->kind == XL6_IR_STORAGE_GLOBAL && info && info->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR) {
                assert(pending == 0); pending = storage->symbol;
            }
        }
        assert(pending != 0 && xl6OutputOpenFile(&out, argv[3]));
        xl6OutputPrintf(&out, "#define XLIR_LIFETIME_PENDING s%u\n", (unsigned int)pending);
        xl6OutputClose(&out);
    } else assert(argc == 1);
    printf("value lifetime: %llu full-module C/header-gated mutations, %llu abstract CFG checks\n",
        (unsigned long long)mutations, (unsigned long long)graphChecks);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
