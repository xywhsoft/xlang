#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t mutations;
static const XL6_IrInstruction* definition(const XL6_IrModule* ir, XL6_IrValueId value)
{
    size_t i;
    for (i = 0; i < ir->instructionCount; ++i) if (ir->instructions[i].result == value) return &ir->instructions[i];
    return NULL;
}
static void reject(const XL6_IrModule* ir)
{
    XL6_DiagList diags = {0}; XL6_Output out; char* text;
    assert(!xl6IrVerifyModule(ir, &diags) && diags.count);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out, ir, "BAD_PROPERTY", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
static const XL6_IrNode* accessorTarget(const XL6_IrModule* ir, const XL6_IrInstruction* call)
{
    const XL6_IrCallSite* site;
    XL6_IrPropertyAccessorSignature signature;
    if (call->kind != XL6_IR_INSTR_CALL && call->kind != XL6_IR_INSTR_VIRTUAL_CALL &&
        call->kind != XL6_IR_INSTR_PROTOCOL_CALL) return NULL;
    site = xl6IrCallSiteGet(ir, call->origin);
    if (!site || (site->dispatchKind == XL6_IR_CALL_DISPATCH_DIRECT
        ? call->kind != XL6_IR_INSTR_CALL || call->targetSymbol != site->targetSymbol
        : site->dispatchKind == XL6_IR_CALL_DISPATCH_CLASS
            ? call->kind != XL6_IR_INSTR_VIRTUAL_CALL || call->dispatchSlot != site->dispatchSlot
            : site->dispatchKind == XL6_IR_CALL_DISPATCH_PROTOCOL
                ? call->kind != XL6_IR_INSTR_PROTOCOL_CALL || call->dispatchSlot != site->dispatchSlot
                : 1)) return NULL;
    return site && xl6IrGetPropertyAccessorSignature(ir, site, &signature)
        ? xl6IrNodeGet(ir, signature.declaration) : NULL;
}
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    size_t i, j, calls = 0, raising = 0, paired = 0, templates = 0;
    XL6_SymbolId pending = 0;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/xlir_property_update.xl", &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "property_transaction"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    if (!xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) {
        for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
        return 1;
    }
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* call = &ir.instructions[i];
        const XL6_IrNode* target;
        const XL6_IrInstruction* receiver;
        target = accessorTarget(&ir, call);
        if (!target) continue;
        ++calls;
        if (xl6IrCallInstructionErrorAbi(&ir, call) == XL6_IR_ERROR_ABI_MAY_RAISE) {
            XL6_IrInstruction* test = &ir.instructions[i + 1]; XL6_IrInstruction saved = *test;
            assert(call->result && test->kind == XL6_IR_INSTR_ERROR_TEST && test->operandCount == 1 &&
                ir.operands[test->firstOperand] == call->result && test->trueTarget != test->falseTarget);
            test->kind = XL6_IR_INSTR_DISCARD; test->trueTarget = test->falseTarget = 0;
            reject(&ir); *test = saved; ++raising;
        }
        receiver = definition(&ir, ir.operands[call->firstOperand]);
        assert(receiver && receiver->kind == XL6_IR_INSTR_LOAD);
        for (j = 0; j < i; ++j) {
            XL6_IrInstruction* store = &ir.instructions[j];
            if (store->kind == XL6_IR_INSTR_STORE && store->targetSymbol == receiver->targetSymbol) {
                XL6_IrInstruction saved = *store;
                store->kind = XL6_IR_INSTR_DISCARD; store->targetSymbol = 0;
                reject(&ir); *store = saved; break;
            }
        }
        assert(j < i);
    }
    for (i = 0; i < ir.nodeCount; ++i) {
        const XL6_IrNode* node = &ir.nodes[i];
        const XL6_IrNode* lhs;
        XL6_IrNodeId getterOrigin;
        const XL6_IrInstruction *get = NULL, *set = NULL;
        size_t getIndex = SIZE_MAX, setIndex = SIZE_MAX;
        XL6_IrExitPlan* exitPlan = NULL;
        if (!xl6IrNodeIsPropertyUpdate(node) ||
            node->opcode != XL6_IR_OPCODE_ADD) continue;
        getterOrigin = ir.children[node->firstChild]; lhs = xl6IrNodeGet(&ir, getterOrigin);
        assert(lhs && xl6IrCallSiteGet(&ir, getterOrigin));
        for (j = 0; j < ir.instructionCount; ++j) {
            const XL6_IrInstruction* call = &ir.instructions[j];
            if (!accessorTarget(&ir, call)) continue;
            if (call->origin == getterOrigin) { assert(!get); get = call; getIndex = j; }
            if (call->origin == i + 1u) { assert(!set); set = call; setIndex = j; }
        }
        if (!get && !set) {
            const XL6_IrCallSite* site = xl6IrCallSiteGet(&ir, getterOrigin);
            const XL6_IrNode* receiver = xl6IrNodeGet(&ir, site->receiver);
            const XL6_IrType* owner = xl6IrTypeGet(&ir.types, receiver->storageType);
            assert(owner && owner->kind == XL6_IR_TYPE_CLASS && owner->paramCount == 1 &&
                xl6IrTypeGet(&ir.types, owner->params[0])->kind == XL6_IR_TYPE_TYPE_PARAM);
            ++templates; continue;
        }
        assert(get && set && getIndex < setIndex && get->operandCount + 1 == set->operandCount);
        /* Borrowed access operands share the same SSA. Consuming keys use
         * separate retained copies, while the canonical bindings remain shared. */
        for (j = 0; j < get->operandCount; ++j) {
            XL6_IrPropertyAccessorSignature signature;
            assert(xl6IrGetPropertyAccessorSignature(&ir, xl6IrCallSiteGet(&ir, getterOrigin), &signature));
            if (j == 0 || signature.parameterAbis[j - 1] != XL6_IR_PARAMETER_ABI_OWNED)
                assert(ir.operands[get->firstOperand + j] == ir.operands[set->firstOperand + j]);
        }
        for (j = 0; j < ir.exitPlanCount; ++j)
            if (ir.exitPlans[j].origin == i + 1u && ir.exitPlans[j].point == XL6_IR_EXIT_POINT_BEFORE_NODE) {
                assert(!exitPlan); exitPlan = &ir.exitPlans[j];
            }
        assert(exitPlan && exitPlan->kind == XL6_SEM_EXIT_THROW);
        {
            XL6_SemExitKind saved = exitPlan->kind;
            exitPlan->kind = XL6_SEM_EXIT_RETURN; reject(&ir); exitPlan->kind = saved;
        }
        ++paired;
    }
    printf("property transaction: %llu accessor calls, %llu checked outcomes, %llu shared getter/setter pairs, %llu rejected mutations\n",
        (unsigned long long)calls, (unsigned long long)raising, (unsigned long long)paired, (unsigned long long)mutations);
    fflush(stdout);
    assert(calls == 43 && raising == 27 && paired == 14 && templates == 1 && mutations == 84);
    assert(ir.classDispatchSlotCount == 2);
    for (i = 0; i < ir.classDispatchSlotCount; ++i) {
        XL6_IrClassDispatchSlot* slot = &ir.classDispatchSlots[i];
        const XL6_IrCallTarget* base = xl6IrCallTargetGet(&ir, slot->declarationSymbol);
        XL6_IrErrorAbi effect = slot->errorAbi;
        XL6_IrCallErrorPhysicalAbi physical = slot->errorPhysicalAbi;
        XL6_IrParameterAbi parameter = slot->parameterAbis[0];
        XL6_IrParameterAbi* parameters = slot->parameterAbis;
        assert(base && base->errorAbi == XL6_IR_ERROR_ABI_NO_RAISE && effect == XL6_IR_ERROR_ABI_MAY_RAISE);
        assert(physical == XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER && parameter == XL6_IR_PARAMETER_ABI_BORROW);
        slot->errorAbi = XL6_IR_ERROR_ABI_NO_RAISE; reject(&ir); slot->errorAbi = effect;
        slot->errorPhysicalAbi = XL6_IR_CALL_ERROR_PHYSICAL_NONE; reject(&ir); slot->errorPhysicalAbi = physical;
        slot->parameterAbis[0] = XL6_IR_PARAMETER_ABI_VALUE; reject(&ir); slot->parameterAbis[0] = parameter;
        slot->parameterAbis = NULL; reject(&ir); slot->parameterAbis = parameters;
    }
    printf("property virtual slots: 2 joined outcomes, borrowed keys, %llu total rejected mutations\n", (unsigned long long)mutations);
    assert(mutations == 92);
    {
        size_t updates = 0, protocols = 0;
        for (i = 0; i < ir.callSiteCount; ++i) {
            XL6_IrCallSite* site = &ir.callSites[i];
            XL6_IrCallSite saved = *site;
            XL6_IrCallArgument* value;
            XL6_IrCallArgument savedValue;
            const XL6_IrNode* update;
            if (!site->preparedGetter) continue;
            update = xl6IrNodeGet(&ir, site->origin);
            value = &ir.callArguments[site->firstArgument + site->argumentCount - 1];
            savedValue = *value;
            assert(value->source == XL6_IR_CALL_ARGUMENT_PROPERTY_UPDATE &&
                value->expression == site->origin && site->receiverEvaluationIndex == 0);
            ++updates;
            if (site->dispatchKind == XL6_IR_CALL_DISPATCH_PROTOCOL) ++protocols;
            site->preparedGetter = 0; reject(&ir); *site = saved;
            site->preparedGetter = site->receiver; reject(&ir); *site = saved;
            site->receiverEvaluationIndex = 1; reject(&ir); *site = saved;
            value->source = XL6_IR_CALL_ARGUMENT_EXPLICIT; reject(&ir); *value = savedValue;
            value->expression = ir.children[update->firstChild + 1]; reject(&ir); *value = savedValue;
            if (site->argumentCount > 1) {
                XL6_IrCallArgument* key = &ir.callArguments[site->firstArgument];
                XL6_IrCallArgument savedKey = *key;
                key->expression = site->receiver; reject(&ir); *key = savedKey;
            }
        }
        assert(updates == 15 && protocols == 12);
        printf("property update: %llu protocol pairs, %llu total rejected mutations\n",
            (unsigned long long)protocols, (unsigned long long)mutations);
    }
    assert(xl6IrVerifyModule(&ir, &parse.diags));
    assert(argc == 4 && xl6OutputOpenFile(&out, argv[1]));
    if (!xl6IrCgenEmitC(&out, &ir, &parse.diags)) {
        for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
        return 1;
    }
    xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]));
    assert(xl6IrCgenEmitHeader(&out, &ir, "XLIR_PROPERTY_TRANSACTION", &parse.diags)); xl6OutputClose(&out);
    for (i = 0; i < ir.storageCount; ++i) {
        const XL6_IrStorage* storage = &ir.storages[i];
        const XL6_IrType* type = xl6IrTypeGet(&ir.types, storage->type);
        if (storage->kind == XL6_IR_STORAGE_GLOBAL && type->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR) {
            assert(!pending); pending = storage->symbol;
        }
    }
    assert(pending && xl6OutputOpenFile(&out, argv[3]));
    xl6OutputPrintf(&out, "#define PROPERTY_PENDING s%u\n", (unsigned int)pending); xl6OutputClose(&out);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
