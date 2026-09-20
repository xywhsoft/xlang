#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t mutations, stores, cellNews, cellStores, matches, detached, nullableCopies, trapEdges, constantWrites;
static const XL6_IrInstruction* definition(const XL6_IrModule* ir, XL6_IrValueId value)
{
    size_t i;
    for (i = 0; i < ir->instructionCount; ++i)
        if (value != 0 && ir->instructions[i].result == value) return &ir->instructions[i];
    return NULL;
}
static void reject(XL6_IrModule* ir, const char* name)
{
    XL6_DiagList diags = {0}; XL6_Output out; char* text;
    if (xl6IrVerifyModule(ir, &diags)) { fprintf(stderr, "storage mutation escaped: %s\n", name); assert(0); }
    assert(diags.count != 0 && xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(text == NULL || *text == '\0'); xrtFree(text);
    assert(xl6OutputOpenMemory(&out) && !xl6IrCgenEmitHeader(&out, ir, "BAD_STORAGE_INPUT", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(text == NULL || *text == '\0'); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
int main(void)
{
    XL6_ParseResult parse; XL6_IrModule ir; size_t i, j;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/xlir_managed_storage_inputs.xl", &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "managed_storage"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    assert(xl6IrLowerModule(&ir, &parse.diags) && xl6IrVerifyModule(&ir, &parse.diags));
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i];
        const XL6_IrInstruction *producer, *acquisition, *borrow;
        const XL6_IrStorage* storage;
        const XL6_IrType* type;
        XL6_IrValueId input;
        size_t inputIndex;
        if (instruction->kind == XL6_IR_INSTR_TRAP_IF_FALSE && instruction->operandCount == 1) {
            XL6_IrValueId condition = ir.operands[instruction->firstOperand];
            for (j = 0; j < i; ++j) {
                XL6_IrInstruction* constant = &ir.instructions[j];
                if (constant->result == condition && constant->kind == XL6_IR_INSTR_CONST &&
                    constant->immediateText != NULL && strcmp(constant->immediateText, "false") == 0) {
                    char* saved = constant->immediateText;
                    constant->immediateText = xrtStrDupN("true", 4); assert(constant->immediateText != NULL);
                    reject(&ir, "a nontrapping path must initialize the match owner");
                    xrtFree(constant->immediateText); constant->immediateText = saved; ++trapEdges;
                    {
                        XL6_IrInstruction savedTrap = *instruction;
                        /* Branch results now use storage, so no mutable VALUE
                         * merge is expected in valid lowering. Construct the
                         * forbidden write explicitly instead of depending on
                         * an obsolete producer shape to generate this test. */
                        instruction->kind = XL6_IR_INSTR_VALUE;
                        instruction->type = constant->type;
                        instruction->targetValue = constant->result;
                        reject(&ir, "VALUE cannot rewrite a constant producer");
                        *instruction = savedTrap; ++constantWrites;
                    }
                }
            }
        }
        if (instruction->runtimeOp == XL6_IR_RUNTIME_STRING_NULLABLE_COPY) {
            XL6_IrInstruction saved = *instruction;
            instruction->kind = XL6_IR_INSTR_VALUE; instruction->runtimeOp = XL6_IR_RUNTIME_NONE;
            reject(&ir, "nullable copy cannot become a borrowed alias"); *instruction = saved; ++nullableCopies;
        }
        if (instruction->kind == XL6_IR_INSTR_VIRTUAL_CALL)
            assert(xl6IrValueOwnershipGet(&ir, instruction->result) == XL6_IR_OWNERSHIP_MOVE);
        if (instruction->kind != XL6_IR_INSTR_STORE && instruction->kind != XL6_IR_INSTR_CELL_INIT &&
            instruction->kind != XL6_IR_INSTR_CELL_EXCHANGE) continue;
        storage = xl6IrStorageGet(&ir, instruction->targetSymbol);
        type = storage != NULL ? xl6IrTypeGet(&ir.types, storage->type) : NULL;
        if (type == NULL || type->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED) continue;
        assert(instruction->operandCount == (instruction->kind == XL6_IR_INSTR_CELL_INIT ? 2u : 1u));
        inputIndex = instruction->firstOperand + (instruction->kind == XL6_IR_INSTR_CELL_INIT ? 1u : 0u);
        input = ir.operands[inputIndex]; producer = definition(&ir, input);
        assert(producer != NULL);
        acquisition = producer;
        if (instruction->kind == XL6_IR_INSTR_CELL_INIT && producer->kind == XL6_IR_INSTR_LOAD) {
            for (j = 0; j < i; ++j)
                if (ir.instructions[j].kind == XL6_IR_INSTR_STORE &&
                    ir.instructions[j].targetSymbol == producer->targetSymbol) {
                    acquisition = definition(&ir, ir.operands[ir.instructions[j].firstOperand]);
                    break;
                }
        }
        assert(acquisition);
        if (acquisition->operandCount == 1 && (acquisition->runtimeOp == XL6_IR_RUNTIME_STRING_COPY ||
            acquisition->runtimeOp == XL6_IR_RUNTIME_STRING_NULLABLE_COPY || acquisition->runtimeOp == XL6_IR_RUNTIME_XVALUE_RETAIN ||
            acquisition->kind == XL6_IR_INSTR_RETAIN)) {
            borrow = definition(&ir, ir.operands[acquisition->firstOperand]);
            if (borrow != NULL && borrow->type == acquisition->type &&
                xl6IrValueOwnershipGet(&ir, borrow->result) == XL6_IR_OWNERSHIP_BORROW) {
                ir.operands[inputIndex] = borrow->result;
                reject(&ir, "managed write cannot consume an unacquired borrow");
                ir.operands[inputIndex] = input;
                stores += instruction->kind == XL6_IR_INSTR_STORE;
                cellNews += instruction->kind == XL6_IR_INSTR_CELL_INIT;
                cellStores += instruction->kind == XL6_IR_INSTR_CELL_EXCHANGE;
            }
        }
        if (producer->kind == XL6_IR_INSTR_LOAD) {
            for (j = 0; j < i; ++j) {
                XL6_IrInstruction* move = &ir.instructions[j];
                if (move->kind == XL6_IR_INSTR_MOVE && move->targetSymbol == producer->targetSymbol &&
                    move->operandCount == 1 && ir.operands[move->firstOperand] == input) {
                    XL6_IrInstruction saved = *move;
                    move->kind = XL6_IR_INSTR_RELEASE; move->operandCount = 0;
                    reject(&ir, "release does not detach a storage owner"); *move = saved; ++detached;
                }
            }
        }
    }
    for (i = 0; i < ir.storageCount; ++i) {
        const XL6_IrStorage* storage = &ir.storages[i];
        const XL6_IrNode* source = xl6IrNodeGet(&ir, storage->declaration);
        if (storage->kind != XL6_IR_STORAGE_TEMPORARY || source == NULL || source->sourceKind != XL6_AST_MATCH) continue;
        for (j = 0; j < ir.instructionCount; ++j) {
            XL6_IrInstruction* instruction = &ir.instructions[j];
            if (instruction->targetSymbol == storage->symbol && instruction->kind == XL6_IR_INSTR_MOVE) {
                XL6_IrInstruction saved = *instruction;
                instruction->kind = XL6_IR_INSTR_RELEASE; instruction->operandCount = 0;
                reject(&ir, "managed match must transfer its selected owner"); *instruction = saved; ++matches;
            }
        }
    }
    printf("managed storage inputs: %llu store, %llu cell-init, %llu cell-exchange borrow mutations; %llu match merges, %llu detached inputs, %llu nullable copies, %llu nontrapping edges, %llu constant writes; %llu rejected mutations\n",
        (unsigned long long)stores, (unsigned long long)cellNews, (unsigned long long)cellStores,
        (unsigned long long)matches, (unsigned long long)detached, (unsigned long long)nullableCopies, (unsigned long long)trapEdges, (unsigned long long)constantWrites, (unsigned long long)mutations);
    fflush(stdout);
    assert(stores != 0 && cellNews != 0 && cellStores != 0 && matches >= 2 && detached != 0 && nullableCopies != 0 && trapEdges >= 2 && constantWrites >= 2);
    assert(xl6IrVerifyModule(&ir, &parse.diags));
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
