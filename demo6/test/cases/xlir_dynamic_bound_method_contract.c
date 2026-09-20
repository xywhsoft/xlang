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
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out, ir, "BAD_BOUND", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    size_t i, reads = 0, bindings = 0, inherited = 0, unsupported = 0;
    XL6_SymbolId pending = 0, faultCallable = 0, faultReceiver = 0;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile(argc == 3 ? argv[1] : "test/cases/xlir_dynamic_bound_method.xl", &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    if (!xl6SemCheckModule(parse.root, &parse.diags, "dynamic_bound_method")) goto failed;
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    if (!xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    if (argc == 3) {
        assert(xl6OutputOpenFile(&out, argv[2]));
        if (!xl6IrCgenEmitC(&out, &ir, &parse.diags)) goto failed;
        xl6OutputClose(&out); xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
    }
    for (i = 0; i < ir.dynamicCallSiteCount; ++i) {
        XL6_IrDynamicCallSite* site = &ir.dynamicCallSites[i]; XL6_IrDynamicCallSite saved = *site;
        if (site->kind != XL6_IR_DYNAMIC_CALL_MEMBER_GET) {
            assert(site->memberBindingAbi == XL6_IR_DYNAMIC_MEMBER_BINDING_NONE);
            site->memberBindingAbi = XL6_IR_DYNAMIC_MEMBER_BINDING_RETAIN_RECEIVER; reject(&ir); *site = saved;
            continue;
        }
        assert(!site->argumentCount && site->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE);
        assert(site->memberBindingAbi == XL6_IR_DYNAMIC_MEMBER_BINDING_RETAIN_RECEIVER);
        assert(site->resultAbi == XL6_IR_DYNAMIC_CALL_RESULT_OWNED_FIRST_VALUE);
        ++bindings;
        site->memberBindingAbi = XL6_IR_DYNAMIC_MEMBER_BINDING_NONE; reject(&ir); *site = saved;
        site->memberBindingAbi = (XL6_IrDynamicMemberBindingAbi)99; reject(&ir); *site = saved;
        site->kind = XL6_IR_DYNAMIC_CALL_METHOD; reject(&ir); *site = saved;
        site->errorAbi = XL6_IR_ERROR_ABI_NO_RAISE; reject(&ir); *site = saved;
        site->resultAbi = XL6_IR_DYNAMIC_CALL_RESULT_INVALID; reject(&ir); *site = saved;
        site->memberName = NULL; reject(&ir); *site = saved;
    }
    for (i = 0; i < ir.dynamicMethodCount; ++i) {
        XL6_IrDynamicMethod* row = &ir.dynamicMethods[i]; XL6_IrDynamicMethod saved = *row;
        const XL6_IrCallTarget* target = xl6IrCallTargetGet(&ir, row->targetSymbol);
        size_t siteIndex;
        if (row->kind != XL6_IR_DYNAMIC_CALL_METHOD) continue;
        for (siteIndex = 0; siteIndex < ir.dynamicCallSiteCount; ++siteIndex)
            if (xl6IrDynamicMethodMatchesCallSite(&ir.dynamicCallSites[siteIndex], row)) break;
        if (siteIndex == ir.dynamicCallSiteCount) continue;
        if (row->adapterAbi == XL6_IR_DYNAMIC_METHOD_ADAPTER_UNSUPPORTED) {
            if (!strcmp(row->name, "mutate")) ++unsupported;
            row->adapterAbi = XL6_IR_DYNAMIC_METHOD_ADAPTER_VERIFIED; reject(&ir); *row = saved;
            continue;
        }
        assert(target && row->adapterAbi == XL6_IR_DYNAMIC_METHOD_ADAPTER_VERIFIED);
        if (target->parameterTypes[0] != row->ownerType) ++inherited;
        /* Method bodies are roots through member binding, not direct CALLs. */
        assert(target->reachability != XL6_IR_REACHABILITY_UNREACHABLE);
        row->kind = XL6_IR_DYNAMIC_CALL_MEMBER_GET; reject(&ir); *row = saved;
        row->runtimeTypeTag = 0; reject(&ir); *row = saved;
        row->targetErrorAbi = XL6_IR_ERROR_ABI_INVALID; reject(&ir); *row = saved;
    }
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* call = &ir.instructions[i]; XL6_IrInstruction saved;
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
    {
        size_t mutableLaunchOnly = 0;
        for (i = 0; i < ir.asyncMachineFrameSlotCount; ++i) {
            XL6_IrAsyncMachineFrameSlot* frame = &ir.asyncMachineFrameSlots[i];
            XL6_IrAsyncMachineFrameSlot saved = *frame;
            if (frame->residence != XL6_IR_ASYNC_FRAME_RESIDENCE_LAUNCH_PARAMETER ||
                frame->firstLiveState != UINT32_MAX) continue;
            ++mutableLaunchOnly;
            assert(frame->lastLiveState == UINT32_MAX && frame->parameter != SIZE_MAX);
            assert(frame->transferAbi == XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_XVALUE);
            frame->transferAbi = XL6_IR_ASYNC_FRAME_TRANSFER_NONE; reject(&ir); *frame = saved;
            frame->destroyAbi = XL6_IR_ASYNC_FRAME_DESTROY_NONE; reject(&ir); *frame = saved;
            frame->firstLiveState = 0; reject(&ir); *frame = saved;
            frame->parameter = SIZE_MAX; reject(&ir); *frame = saved;
            frame->storageSymbol = 0; reject(&ir); *frame = saved;
        }
        assert(mutableLaunchOnly == 1);
    }
    assert(reads >= 7 && bindings >= 7 && inherited >= 1 && unsupported == 1 && mutations >= 80);
    assert(xl6IrVerifyModule(&ir, &parse.diags));
    assert(argc == 4 && xl6OutputOpenFile(&out, argv[1]));
    if (!xl6IrCgenEmitC(&out, &ir, &parse.diags)) goto failed;
    xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]));
    assert(xl6IrCgenEmitHeader(&out, &ir, "XLIR_DYNAMIC_BOUND_METHOD", &parse.diags)); xl6OutputClose(&out);
    for (i = 0; i < ir.storageCount; ++i) {
        const XL6_IrStorage* storage = &ir.storages[i];
        const XL6_IrType* type = xl6IrTypeGet(&ir.types, storage->type);
        const XL6_IrNode* declaration = xl6IrNodeGet(&ir, storage->declaration);
        if (storage->kind == XL6_IR_STORAGE_GLOBAL && declaration && declaration->text &&
            strcmp(declaration->text, "boundFaultCallable") == 0) faultCallable = storage->symbol;
        if (storage->kind == XL6_IR_STORAGE_GLOBAL && declaration && declaration->text &&
            strcmp(declaration->text, "boundFaultReceiver") == 0) faultReceiver = storage->symbol;
        if (storage->kind == XL6_IR_STORAGE_GLOBAL && type->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR) {
            assert(!pending); pending = storage->symbol;
        }
    }
    assert(pending && faultCallable && faultReceiver && xl6OutputOpenFile(&out, argv[3]));
    xl6OutputPrintf(&out, "#define BOUND_PENDING s%u\n#define BOUND_FAULT_CALLABLE s%u\n#define BOUND_FAULT_RECEIVER s%u\n",
        (unsigned int)pending, (unsigned int)faultCallable, (unsigned int)faultReceiver); xl6OutputClose(&out);
    printf("dynamic bound method: %llu owned reads, %llu binding sites, %llu inherited rows, %llu rejected mutations\n",
        (unsigned long long)reads, (unsigned long long)bindings, (unsigned long long)inherited, (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
    return 1;
}
