#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "xlir_storage_exchange_checks.h"

static size_t mutations, exchanges;
static void reject(const XL6_IrModule* ir, const char* label, int ownerFailure)
{
    XL6_DiagList diags = {0}; XL6_Output out; char* text;
    if (ownerFailure) {
        if (xl6IrVerifyOwnerConsumption(ir, &diags)) {
            fprintf(stderr, "exchange owner mutation escaped: %s\n", label); assert(0);
        }
        xl6DiagFree(&diags);
    }
    if (xl6IrVerifyModule(ir, &diags)) {
        fprintf(stderr, "exchange mutation escaped: %s\n", label); assert(0);
    }
    assert(diags.count && xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    assert(xl6OutputOpenMemory(&out) && !xl6IrCgenEmitHeader(&out, ir, "BAD_EXCHANGE", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
static void checkModule(const char* source, const char* cPath, const char* hPath, const char* jsonPath)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; size_t i;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile(source, &parse) && xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "storage_exchange"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    assert(xl6IrLowerModule(&ir, &parse.diags) && xl6IrVerifyModule(&ir, &parse.diags));
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction *write = &ir.instructions[i], *oldStore, saved, sink;
        XL6_IrValueId input;
        if (write->kind != XL6_IR_INSTR_CELL_EXCHANGE) continue;
        ++exchanges; checkStorageReplacement(&ir, write);
        assert(xl6IrValueOwnershipGet(&ir, write->result) == XL6_IR_OWNERSHIP_MOVE);
        assert(xl6IrInstructionOperandOwnership(&ir, write, 0) == XL6_IR_OPERAND_OWNERSHIP_CONSUME);
        saved = *write; oldStore = write + 1; sink = *oldStore;
        write->targetSymbol = 0; reject(&ir, "missing cell", 0); *write = saved;
        write->result = XL6_IR_VALUE_INVALID; reject(&ir, "missing previous owner", 0); *write = saved;
        write->type = XL6_IR_TYPE_INVALID; reject(&ir, "missing payload type", 0); *write = saved;
        write->operandCount = 0; reject(&ir, "missing new owner", 0); *write = saved;
        write->kind = XL6_IR_INSTR_CELL_STORE; reject(&ir, "hidden managed replacement", 0); *write = saved;
        oldStore->kind = XL6_IR_INSTR_DISCARD; oldStore->targetSymbol = 0;
        reject(&ir, "lost previous owner", 1); *oldStore = sink;
        input = ir.operands[oldStore->firstOperand];
        ir.operands[oldStore->firstOperand] = ir.operands[write->firstOperand];
        reject(&ir, "consumed new owner twice", 1);
        ir.operands[oldStore->firstOperand] = input;
        /* A cell load observes its payload. Removing the transfer cannot turn
         * that loan into the independent old owner required by the sink. */
        write->kind = XL6_IR_INSTR_CELL_LOAD; write->operandCount = 0;
        reject(&ir, "borrow instead of exchange owner", 1); *write = saved;
    }
    assert(xl6IrVerifyModule(&ir, &parse.diags));
    if (cPath) {
        assert(xl6OutputOpenFile(&out, cPath) && xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
        assert(xl6OutputOpenFile(&out, hPath) && xl6IrCgenEmitHeader(&out, &ir, "STORAGE_EXCHANGE", &parse.diags)); xl6OutputClose(&out);
        assert(xl6OutputOpenFile(&out, jsonPath)); xl6IrDumpModuleJson(&out, &ir); assert(!xl6OutputHasFailed(&out)); xl6OutputClose(&out);
    }
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
}
int main(int argc, char** argv)
{
    assert(argc == 4);
    checkModule("test/cases/xlir_storage_exchange.xl", argv[1], argv[2], argv[3]);
    checkModule("test/cases/xlir_product_owner.xl", NULL, NULL, NULL);
    assert(exchanges >= 4 && mutations == exchanges * 8);
    printf("storage exchange: %llu explicit cell exchanges, %llu type/shape/ownership mutations rejected before C/header output\n",
        (unsigned long long)exchanges, (unsigned long long)mutations);
    return 0;
}
