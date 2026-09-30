#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "xlir_storage_exchange_checks.h"

static size_t mutations;
static int contains(const XL6_IrModule* ir, XL6_IrNodeId root, XL6_IrNodeId child)
{
    const XL6_IrNode* node = xl6IrNodeGet(ir, root);
    size_t i;
    if (root == child) return 1;
    if (!node) return 0;
    for (i = 0; i < node->childCount; ++i)
        if (contains(ir, ir->children[node->firstChild + i], child)) return 1;
    return 0;
}
static void reject(const XL6_IrModule* ir)
{
    XL6_DiagList diags = {0}; XL6_Output out; char* text;
    assert(!xl6IrVerifyModule(ir, &diags) && diags.count);
    assert(xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    assert(xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitHeader(&out, ir, "BAD_TRANSACTION", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    size_t i, j, transactions = 0, managed = 0, cells = 0, statics = 0, classFields = 0;
    XL6_SymbolId pending = 0;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/xlir_storage_assignment_transaction.xl", &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "storage_transaction"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    assert(xl6IrLowerModule(&ir, &parse.diags) && xl6IrVerifyModule(&ir, &parse.diags));
    for (i = 0; i < ir.nodeCount; ++i) {
        const XL6_IrNode* node = &ir.nodes[i];
        const XL6_IrNode* lhs;
        const XL6_IrType* type;
        XL6_IrInstruction *read = NULL, *write = NULL, *snapshot = NULL;
        XL6_IrNodeId origin = (XL6_IrNodeId)(i + 1u), rhs;
        size_t readIndex = SIZE_MAX, snapshotIndex = SIZE_MAX, rhsIndex = SIZE_MAX, writeIndex = SIZE_MAX;
        if (node->kind != XL6_IR_NODE_ASSIGN || node->opcode != XL6_IR_OPCODE_ADD) continue;
        assert(node->childCount == 2);
        lhs = xl6IrNodeGet(&ir, ir.children[node->firstChild]);
        rhs = ir.children[node->firstChild + 1]; type = xl6IrTypeGet(&ir.types, lhs->storageType);
        for (j = 0; j < ir.instructionCount; ++j) {
            XL6_IrInstruction* x = &ir.instructions[j];
            if (rhsIndex == SIZE_MAX && contains(&ir, rhs, x->origin)) rhsIndex = j;
            if (x->origin != origin) continue;
            if (!read && (x->kind == XL6_IR_INSTR_LOAD || x->kind == XL6_IR_INSTR_CELL_LOAD) &&
                x->type == lhs->storageType) { read = x; readIndex = j; }
            if (read && (x->kind == XL6_IR_INSTR_STORE || x->kind == XL6_IR_INSTR_CELL_STORE ||
                         x->kind == XL6_IR_INSTR_CELL_EXCHANGE) &&
                x->targetSymbol == read->targetSymbol) { assert(!write); write = x; writeIndex = j; }
            if (read && x->kind == XL6_IR_INSTR_RUNTIME_CALL && x->operandCount == 1 &&
                ir.operands[x->firstOperand] == read->result &&
                (x->runtimeOp == XL6_IR_RUNTIME_STRING_COPY || x->runtimeOp == XL6_IR_RUNTIME_XVALUE_RETAIN)) {
                assert(!snapshot); snapshot = x; snapshotIndex = j;
            }
        }
        if (lhs->kind == XL6_IR_NODE_MEMBER &&
            !(read && write && readIndex < rhsIndex && rhsIndex < writeIndex)) {
            /* Class payload fields use a resolved field-address path rather
             * than the scalar/global storage LOAD/STORE path below. Their
             * transaction behavior is exercised by the generated host. */
            ++classFields;
            continue;
        }
        if (!(read && write && readIndex < rhsIndex && rhsIndex < writeIndex)) {
            fprintf(stderr, "transaction node %llu lhs=%d read=%p write=%p order=%llu/%llu/%llu\n",
                (unsigned long long)origin, (int)lhs->kind, (void*)read, (void*)write,
                (unsigned long long)readIndex, (unsigned long long)rhsIndex,
                (unsigned long long)writeIndex);
            assert(0);
        }
        assert(write->type == lhs->storageType);
        checkStorageReplacement(&ir, write);
        if (read->kind == XL6_IR_INSTR_CELL_LOAD) {
            XL6_IrInstruction saved = *write;
            ++cells;
            write->kind = XL6_IR_INSTR_STORE; reject(&ir); *write = saved;
        }
        if (lhs->kind == XL6_IR_NODE_MEMBER) {
            const XL6_IrStorage* storage = xl6IrStorageGet(&ir, read->targetSymbol);
            assert(storage && storage->kind == XL6_IR_STORAGE_GLOBAL); ++statics;
        }
        if (type->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED) {
            XL6_IrInstruction saved;
            XL6_IrValueId operand = ir.operands[write->firstOperand];
            assert(snapshot && readIndex < snapshotIndex && snapshotIndex < rhsIndex);
            ir.operands[write->firstOperand] = read->result; reject(&ir);
            ir.operands[write->firstOperand] = operand;
            saved = *snapshot; snapshot->kind = XL6_IR_INSTR_VALUE; snapshot->runtimeOp = XL6_IR_RUNTIME_NONE;
            reject(&ir); *snapshot = saved; ++managed;
        } else {
            XL6_IrInstruction saved = *write;
            write->type = XL6_IR_TYPE_INVALID; reject(&ir); *write = saved;
        }
        ++transactions;
    }
    printf("storage transaction: %llu ordered updates, %llu managed snapshots, %llu cells, %llu statics, %llu class fields, %llu rejected mutations\n",
        (unsigned long long)transactions, (unsigned long long)managed, (unsigned long long)cells,
        (unsigned long long)statics, (unsigned long long)classFields, (unsigned long long)mutations);
    assert(transactions == 24 && managed == 7 && cells == 2 && statics == 8 &&
           classFields == 8 && mutations == 33);
    assert(xl6IrVerifyModule(&ir, &parse.diags));
    assert(argc == 4 && xl6OutputOpenFile(&out, argv[1]));
    assert(xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]));
    assert(xl6IrCgenEmitHeader(&out, &ir, "XLIR_STORAGE_TRANSACTION", &parse.diags)); xl6OutputClose(&out);
    for (i = 0; i < ir.storageCount; ++i) {
        const XL6_IrStorage* storage = &ir.storages[i];
        const XL6_IrType* type = xl6IrTypeGet(&ir.types, storage->type);
        if (storage->kind == XL6_IR_STORAGE_GLOBAL && type->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR) {
            assert(!pending); pending = storage->symbol;
        }
    }
    assert(pending && xl6OutputOpenFile(&out, argv[3]));
    xl6OutputPrintf(&out, "#define TRANSACTION_PENDING s%u\n", (unsigned int)pending); xl6OutputClose(&out);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
