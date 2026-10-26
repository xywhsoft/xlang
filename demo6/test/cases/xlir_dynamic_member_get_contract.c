#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t mutations;
static void reject(const XL6_IrModule* ir)
{
    XL6_DiagList diags = {0}; XL6_Output out; char* text;
    assert(!xl6IrVerifyModule(ir, &diags) && diags.count);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out, ir, "BAD_MEMBER", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    size_t i, reads = 0, namedReads = 0, getters = 0, inherited = 0, unsupported = 0; XL6_SymbolId pending = 0;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile(argc == 3 ? argv[1] : "test/cases/xlir_dynamic_member_get.xl", &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "dynamic_member_get"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    if (!xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    if (argc == 3) {
        assert(xl6OutputOpenFile(&out, argv[2]));
        if (!xl6IrCgenEmitC(&out, &ir, &parse.diags)) goto failed;
        xl6OutputClose(&out); xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
    }
    for (i = 0; i < ir.dynamicCallSiteCount; ++i) {
        XL6_IrDynamicCallSite* site = &ir.dynamicCallSites[i]; XL6_IrDynamicCallSite saved = *site;
        if (site->kind != XL6_IR_DYNAMIC_CALL_MEMBER_GET) continue;
        assert(!site->argumentCount && site->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE);
        assert(site->resultAbi == XL6_IR_DYNAMIC_CALL_RESULT_OWNED_FIRST_VALUE);
        site->kind = XL6_IR_DYNAMIC_CALL_METHOD; reject(&ir); *site = saved;
        site->errorAbi = XL6_IR_ERROR_ABI_NO_RAISE; reject(&ir); *site = saved;
        site->resultAbi = XL6_IR_DYNAMIC_CALL_RESULT_INVALID; reject(&ir); *site = saved;
        site->memberName = NULL; reject(&ir); *site = saved;
    }
    for (i = 0; i < ir.dynamicMethodCount; ++i) {
        XL6_IrDynamicMethod* row = &ir.dynamicMethods[i]; XL6_IrDynamicMethod saved = *row;
        const XL6_IrCallTarget* target = xl6IrCallTargetGet(&ir, row->targetSymbol);
        if (row->kind != XL6_IR_DYNAMIC_CALL_MEMBER_GET || strcmp(row->name, "label") != 0) continue;
        if (row->adapterAbi == XL6_IR_DYNAMIC_METHOD_ADAPTER_UNSUPPORTED) {
            ++unsupported;
            row->adapterAbi = XL6_IR_DYNAMIC_METHOD_ADAPTER_VERIFIED; reject(&ir); *row = saved;
            continue;
        }
        ++getters;
        assert(target && row->adapterAbi == XL6_IR_DYNAMIC_METHOD_ADAPTER_VERIFIED);
        if (target->parameterTypes[0] != row->ownerType) ++inherited;
        row->kind = XL6_IR_DYNAMIC_CALL_METHOD; reject(&ir); *row = saved;
        row->runtimeTypeTag = 0; reject(&ir); *row = saved;
        row->targetErrorAbi = XL6_IR_ERROR_ABI_INVALID; reject(&ir); *row = saved;
    }
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* call = &ir.instructions[i]; XL6_IrInstruction saved;
        if (call->kind == XL6_IR_INSTR_RUNTIME_CALL &&
            call->runtimeOp == XL6_IR_RUNTIME_NAMED_READ_LITERAL_COPY) {
            /* `inner` has no method/property row. Its verified named-read
             * plan replaces the former sixth erased getter dispatch. */
            size_t site;
            for (site = 0; site < ir.dynamicCallSiteCount; ++site)
                if (ir.dynamicCallSites[site].origin == call->origin) break;
            assert(site < ir.dynamicCallSiteCount &&
                !strcmp(ir.dynamicCallSites[site].memberName, "inner") &&
                xl6IrDynamicMemberNamedOnly(&ir, &ir.dynamicCallSites[site]));
            assert(call->operandCount == 1 &&
                xl6IrValueOwnershipGet(&ir, call->result) == XL6_IR_OWNERSHIP_MOVE);
            assert(i + 1 < ir.instructionCount && ir.instructions[i + 1].kind == XL6_IR_INSTR_ERROR_TEST);
            ++namedReads;
            saved = *call; call->runtimeOp = XL6_IR_RUNTIME_XVALUE_DYNAMIC_METHOD_CALL;
            reject(&ir); *call = saved;
            continue;
        }
        if (call->kind != XL6_IR_INSTR_RUNTIME_CALL || call->runtimeOp != XL6_IR_RUNTIME_XVALUE_DYNAMIC_METHOD_CALL)
            continue;
        assert(call->dynamicCallSite < ir.dynamicCallSiteCount);
        assert(ir.dynamicCallSites[call->dynamicCallSite].kind == XL6_IR_DYNAMIC_CALL_MEMBER_GET);
        assert(call->operandCount == 1 && xl6IrValueOwnershipGet(&ir, call->result) == XL6_IR_OWNERSHIP_MOVE);
        assert(i + 1 < ir.instructionCount && ir.instructions[i + 1].kind == XL6_IR_INSTR_ERROR_TEST);
        ++reads;
        saved = ir.instructions[i + 1]; ir.instructions[i + 1].kind = XL6_IR_INSTR_DISCARD;
        reject(&ir); ir.instructions[i + 1] = saved;
        saved = *call; call->dynamicCallSite = SIZE_MAX; reject(&ir); *call = saved;
        call->dynamicCallResultAbi = XL6_IR_DYNAMIC_CALL_RESULT_INVALID; reject(&ir); *call = saved;
        call->runtimeOp = XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_GET_LITERAL; reject(&ir); *call = saved;
    }
    assert(reads == 5 && namedReads == 1 && getters == 4 && inherited == 1 && unsupported == 1 && mutations >= 50);
    assert(xl6IrVerifyModule(&ir, &parse.diags));
    assert(argc == 4 && xl6OutputOpenFile(&out, argv[1]));
    if (!xl6IrCgenEmitC(&out, &ir, &parse.diags)) goto failed;
    xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]));
    assert(xl6IrCgenEmitHeader(&out, &ir, "XLIR_DYNAMIC_MEMBER_GET", &parse.diags)); xl6OutputClose(&out);
    for (i = 0; i < ir.storageCount; ++i) {
        const XL6_IrStorage* storage = &ir.storages[i];
        const XL6_IrType* type = xl6IrTypeGet(&ir.types, storage->type);
        if (storage->kind == XL6_IR_STORAGE_GLOBAL && type->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR) {
            assert(!pending); pending = storage->symbol;
        }
    }
    assert(pending && xl6OutputOpenFile(&out, argv[3]));
    xl6OutputPrintf(&out, "#define MEMBER_PENDING s%u\n", (unsigned int)pending); xl6OutputClose(&out);
    printf("dynamic member: %llu owned reads, %llu getter rows, %llu inherited rows, %llu rejected mutations\n",
        (unsigned long long)reads, (unsigned long long)getters, (unsigned long long)inherited, (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
    return 1;
}
