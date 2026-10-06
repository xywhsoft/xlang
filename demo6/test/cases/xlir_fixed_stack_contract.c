#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned mutations;
static void rejected(const XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir, NULL));
    for (unsigned header = 0; header < 2; ++header) {
        XL6_Output output; XL6_DiagList diagnostics; size_t size = SIZE_MAX;
        xl6DiagInit(&diagnostics); assert(xl6OutputOpenMemory(&output));
        assert(!(header ? xl6IrCgenEmitHeader(&output, ir, "REFUSED_GENERIC_ABI", &diagnostics)
                        : xl6IrCgenEmitC(&output, ir, &diagnostics)));
        char* text = xl6OutputTakeMemoryText(&output, &size);
        assert(size == 0); xrtFree(text);
        xl6OutputDiscard(&output); xl6DiagFree(&diagnostics);
    }
    ++mutations;
}

int main(int argc, char** argv)
{
    assert(argc == 3);
    int ok = 0;
    unsigned dependent = 0, concrete = 0, voidCalls = 0, nullable = 0, products = 0;
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output output;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile(argv[1], &parse) || !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "generic_contract") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags)) goto done;
    assert(xl6IrVerifyModule(&ir, NULL));
    for (size_t i = 0; i < ir.intrinsicCallCount; ++i) {
        XL6_IrIntrinsicCall* item = &ir.intrinsicCalls[i];
        if (item->kind != XL6_IR_INTRINSIC_RUNTIME_RESOURCE) continue;
        XL6_IrIntrinsicAdmission admission = item->admission;
        assert(admission == XL6_IR_INTRINSIC_ADMISSION_VERIFIED ||
               admission == XL6_IR_INTRINSIC_ADMISSION_GENERIC_DEPENDENT);
        if (admission == XL6_IR_INTRINSIC_ADMISSION_GENERIC_DEPENDENT) {
            ++dependent;
            item->admission = XL6_IR_INTRINSIC_ADMISSION_VERIFIED; rejected(&ir);
        } else {
            ++concrete;
            item->admission = XL6_IR_INTRINSIC_ADMISSION_GENERIC_DEPENDENT; rejected(&ir);
        }
        item->admission = admission;
        XL6_IrRuntimeResourcePayloadAbi payload = item->runtimeResourcePayloadAbi;
        item->runtimeResourcePayloadAbi = payload == XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID
            ? XL6_IR_RUNTIME_RESOURCE_PAYLOAD_TRIVIAL_COPY : XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID;
        rejected(&ir); item->runtimeResourcePayloadAbi = payload;
        XL6_IrIntrinsicResultAbi result = item->resultAbi;
        item->resultAbi = result == XL6_IR_INTRINSIC_RESULT_VOID
            ? XL6_IR_INTRINSIC_RESULT_VALUE : XL6_IR_INTRINSIC_RESULT_VOID;
        rejected(&ir); item->resultAbi = result;
        XL6_IrErrorAbi error = item->errorAbi;
        item->errorAbi = XL6_IR_ERROR_ABI_NO_RAISE;
        rejected(&ir); item->errorAbi = error;
        if (result == XL6_IR_INTRINSIC_RESULT_VOID) {
            assert(item->resultType == XL6_IR_TYPE_INVALID);
            ++voidCalls;
        }
        assert(xl6IrVerifyModule(&ir, NULL));
    }
    /* A duplicated structural row is invalid even if its frontend ABI marker
     * differs. Borrow the extra row only for verification; never free it. */
    if (ir.types.capacity == ir.types.count) {
        void* grown = xrtRealloc(ir.types.items, (ir.types.count + 1) * sizeof(*ir.types.items));
        assert(grown); ir.types.items = grown; ++ir.types.capacity;
    }
    for (size_t i = 0; i < ir.types.count; ++i) {
        XL6_IrType* type = &ir.types.items[i];
        if (type->kind != XL6_IR_TYPE_NULLABLE && type->kind != XL6_IR_TYPE_TUPLE) continue;
        if (type->kind == XL6_IR_TYPE_NULLABLE) ++nullable; else ++products;
        XL6_IrType* duplicate = &ir.types.items[ir.types.count++];
        *duplicate = *type;
        duplicate->abiId ^= UINT64_C(0xABCDEF1234);
        rejected(&ir);
        --ir.types.count; memset(duplicate, 0, sizeof(*duplicate));
        if (type->kind == XL6_IR_TYPE_NULLABLE) {
            XL6_IrNullAbi nullAbi = type->nullAbi;
            type->nullAbi = XL6_IR_NULL_ABI_NONE;
            rejected(&ir); type->nullAbi = nullAbi;
        }
        assert(xl6IrVerifyModule(&ir, NULL));
    }
    assert(nullable && products && mutations);
    if (strstr(argv[1], "fixed_stack")) assert(dependent && concrete && voidCalls);
    for (size_t i = 0; i < ir.instructionCount; ++i) {
        const XL6_IrInstruction* instruction = &ir.instructions[i];
        if (instruction->origin != XL6_IR_NODE_INVALID) {
            const XL6_IrIntrinsicCall* intrinsic = xl6IrIntrinsicCallGet(&ir, instruction->origin);
            if (intrinsic != NULL) assert(intrinsic->admission == XL6_IR_INTRINSIC_ADMISSION_VERIFIED);
        }
    }
    if (!xl6OutputOpenFile(&output, argv[2])) goto done;
    ok = xl6IrCgenEmitC(&output, &ir, &parse.diags); xl6OutputClose(&output);
    printf("generic resource contract: %u dependent; %u concrete; %u void; %u nullable; %u products; %u mutations refused before C/header output\n",
        dependent, concrete, voidCalls, nullable, products, mutations);
done:
    for (size_t i = 0; i < parse.diags.count; ++i)
        fprintf(stderr, "%d:%d: %s\n", parse.diags.items[i].line,
            parse.diags.items[i].column, parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return !ok;
}
