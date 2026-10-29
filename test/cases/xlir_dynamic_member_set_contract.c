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
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out, ir, "BAD_SETTER", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    size_t i, writes = 0, setters = 0, inherited = 0, unsupported = 0;
    XL6_SymbolId pending = 0;
    size_t amountSite = SIZE_MAX;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile(argc == 3 ? argv[1] : "test/cases/xlir_dynamic_member_set.xl", &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "dynamic_member_set"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    if (!xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    if (argc == 3) {
        assert(xl6OutputOpenFile(&out, argv[2]));
        if (!xl6IrCgenEmitC(&out, &ir, &parse.diags)) goto failed;
        xl6OutputClose(&out); xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
    }
    for (i = 0; i < ir.dynamicCallSiteCount; ++i) {
        XL6_IrDynamicCallSite* site = &ir.dynamicCallSites[i]; XL6_IrDynamicCallSite saved = *site;
        XL6_IrDynamicCallArgument* argument; XL6_IrDynamicCallArgument savedArgument;
        if (site->kind != XL6_IR_DYNAMIC_CALL_MEMBER_SET) continue;
        if (amountSite == SIZE_MAX && strcmp(site->memberName, "amount") == 0) amountSite = i;
        assert(site->argumentCount == 1 && site->positionalCount == 1 && !site->keywordCount);
        assert(site->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE);
        assert(site->resultAbi == XL6_IR_DYNAMIC_CALL_RESULT_OWNED_UNIT_VALUE);
        assert(site->evaluationOrder == XL6_IR_DYNAMIC_CALL_ARGUMENTS_FIRST);
        site->kind = XL6_IR_DYNAMIC_CALL_METHOD; reject(&ir); *site = saved;
        site->kind = XL6_IR_DYNAMIC_CALL_MEMBER_GET; reject(&ir); *site = saved;
        site->evaluationOrder = XL6_IR_DYNAMIC_CALL_SUBJECT_FIRST; reject(&ir); *site = saved;
        site->errorAbi = XL6_IR_ERROR_ABI_NO_RAISE; reject(&ir); *site = saved;
        site->resultAbi = XL6_IR_DYNAMIC_CALL_RESULT_OWNED_FIRST_VALUE; reject(&ir); *site = saved;
        site->memberBindingAbi = XL6_IR_DYNAMIC_MEMBER_BINDING_RETAIN_RECEIVER; reject(&ir); *site = saved;
        site->memberName = NULL; reject(&ir); *site = saved;
        site->subject = 0; reject(&ir); *site = saved;
        site->argumentCount = 0; reject(&ir); *site = saved;
        argument = &ir.dynamicCallArguments[site->firstArgument]; savedArgument = *argument;
        argument->kind = XL6_IR_DYNAMIC_CALL_ARGUMENT_KEYWORD; reject(&ir); *argument = savedArgument;
        argument->expression = site->subject; reject(&ir); *argument = savedArgument;
        argument->type = XL6_IR_TYPE_INVALID; reject(&ir); *argument = savedArgument;
        argument->admission = XL6_IR_DYNAMIC_CALL_ARGUMENT_ADMISSION_UNSUPPORTED_BOX_ABI;
        reject(&ir); *argument = savedArgument;
    }
    for (i = 0; i < ir.dynamicMethodCount; ++i) {
        XL6_IrDynamicMethod* row = &ir.dynamicMethods[i]; XL6_IrDynamicMethod saved = *row;
        const XL6_IrCallTarget* target = xl6IrCallTargetGet(&ir, row->targetSymbol);
        if (row->kind != XL6_IR_DYNAMIC_CALL_MEMBER_SET) continue;
        if (row->adapterAbi == XL6_IR_DYNAMIC_METHOD_ADAPTER_UNSUPPORTED) {
            ++unsupported; row->adapterAbi = XL6_IR_DYNAMIC_METHOD_ADAPTER_VERIFIED; reject(&ir); *row = saved;
            continue;
        }
        ++setters; assert(target && row->returnCount == 0 && row->parameterCount == 1);
        if (target->parameterTypes[0] != row->ownerType) ++inherited;
        row->kind = XL6_IR_DYNAMIC_CALL_METHOD; reject(&ir); *row = saved;
        row->runtimeTypeTag = 0; reject(&ir); *row = saved;
        row->returnCount = 1; reject(&ir); *row = saved;
        row->targetErrorAbi = XL6_IR_ERROR_ABI_INVALID; reject(&ir); *row = saved;
    }
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* call = &ir.instructions[i]; XL6_IrInstruction saved;
        if (call->kind != XL6_IR_INSTR_RUNTIME_CALL || call->runtimeOp != XL6_IR_RUNTIME_XVALUE_DYNAMIC_METHOD_CALL ||
            ir.dynamicCallSites[call->dynamicCallSite].kind != XL6_IR_DYNAMIC_CALL_MEMBER_SET) continue;
        assert(call->operandCount == 2 && xl6IrValueOwnershipGet(&ir, call->result) == XL6_IR_OWNERSHIP_MOVE);
        assert(i + 1 < ir.instructionCount && ir.instructions[i + 1].kind == XL6_IR_INSTR_ERROR_TEST);
        ++writes;
        saved = ir.instructions[i + 1]; ir.instructions[i + 1].kind = XL6_IR_INSTR_DISCARD;
        reject(&ir); ir.instructions[i + 1] = saved;
        saved = *call; call->dynamicCallSite = SIZE_MAX; reject(&ir); *call = saved;
        call->dynamicCallResultAbi = XL6_IR_DYNAMIC_CALL_RESULT_OWNED_FIRST_VALUE; reject(&ir); *call = saved;
        call->runtimeOp = XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_SET_LITERAL_NEW; reject(&ir); *call = saved;
    }
    assert(writes >= 8 && setters >= 4 && inherited >= 1 && unsupported >= 1 && mutations >= 100);
    assert(xl6IrVerifyModule(&ir, &parse.diags));
    assert(argc == 4 && xl6OutputOpenFile(&out, argv[1]));
    if (!xl6IrCgenEmitC(&out, &ir, &parse.diags)) goto failed;
    xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]));
    assert(xl6IrCgenEmitHeader(&out, &ir, "XLIR_DYNAMIC_MEMBER_SET", &parse.diags)); xl6OutputClose(&out);
    for (i = 0; i < ir.storageCount; ++i) {
        const XL6_IrStorage* storage = &ir.storages[i];
        const XL6_IrType* type = xl6IrTypeGet(&ir.types, storage->type);
        if (storage->kind == XL6_IR_STORAGE_GLOBAL && type->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR) {
            assert(!pending); pending = storage->symbol;
        }
    }
    assert(pending && amountSite != SIZE_MAX && xl6OutputOpenFile(&out, argv[3]));
    xl6OutputPrintf(&out, "#define SETTER_PENDING s%u\n#define SETTER_AMOUNT_DISPATCH xlir_dynamic_method_call_d%lu\n",
        (unsigned int)pending, (unsigned long)amountSite); xl6OutputClose(&out);
    printf("dynamic setter: %llu owned writes, %llu setter rows, %llu inherited rows, %llu rejected mutations\n",
        (unsigned long long)writes, (unsigned long long)setters, (unsigned long long)inherited, (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
    return 1;
}
