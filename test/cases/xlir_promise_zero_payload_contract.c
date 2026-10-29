#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t refusals;
static void reject(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module, NULL));
    for (int header = 0; header < 2; ++header) {
        XL6_Output output;
        XL6_DiagList errors;
        size_t size = SIZE_MAX;
        xl6DiagInit(&errors);
        assert(xl6OutputOpenMemory(&output));
        assert(!(header ? xl6IrCgenEmitHeader(&output, module, "BAD_EMPTY_PAYLOAD", &errors)
                       : xl6IrCgenEmitC(&output, module, &errors)));
        char* text = xl6OutputTakeMemoryText(&output, &size);
        assert(size == 0);
        xrtFree(text);
        xl6OutputDiscard(&output);
        xl6DiagFree(&errors);
    }
    ++refusals;
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    XL6_CompileUnit unit;
    xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit, argv[1], 1));
    assert(xl6IrLowerModule(&unit.ir, &unit.parse.diags));
    XL6_IrModule* module = &unit.ir;
    assert(xl6IrVerifyModule(module, NULL));
    size_t targets = 0, producers = 0;
    for (size_t t = 0; t < module->callTargetCount; ++t) {
        XL6_IrCallTarget* target = &module->callTargets[t];
        for (size_t p = 0; p < target->parameterCount; ++p) {
            if (target->parameterPhysicalAbis[p] != XL6_IR_CALL_PARAMETER_PHYSICAL_GENERIC_ZERO_PAYLOAD)
                continue;
            assert(xl6IrTypeIsZeroPayload(module, target->parameterTypes[p]));
            assert(target->parameterAbis[p] == XL6_IR_PARAMETER_ABI_VALUE);
            assert(xl6IrRuntimeDirectBridgeTargetMatches(module, target));
            target->parameterPhysicalAbis[p] = XL6_IR_CALL_PARAMETER_PHYSICAL_GENERIC_VALUE_ADDRESS_BORROW;
            reject(module);
            target->parameterPhysicalAbis[p] = XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;
            reject(module);
            target->parameterPhysicalAbis[p] = XL6_IR_CALL_PARAMETER_PHYSICAL_GENERIC_ZERO_PAYLOAD;
            XL6_IrAsyncResultAbi saved = target->nativeContextPayloadAbi;
            target->nativeContextPayloadAbi = XL6_IR_ASYNC_RESULT_COPY_I64;
            reject(module);
            target->nativeContextPayloadAbi = saved;
            ++targets;
        }
    }
    for (size_t i = 0; i < module->instructionCount; ++i) {
        XL6_IrInstruction* instruction = &module->instructions[i];
        if (!xl6IrInstructionIsZeroPayload(module, instruction)) continue;
        XL6_IrInstruction saved = *instruction;
        instruction->kind = XL6_IR_INSTR_VALUE;
        reject(module);
        *instruction = saved;
        instruction->targetValue = instruction->result;
        reject(module);
        *instruction = saved;
        instruction->origin = module->root;
        reject(module);
        *instruction = saved;
        instruction->runtimeOp = XL6_IR_RUNTIME_XVALUE_NULL;
        reject(module);
        *instruction = saved;
        XL6_IrType* type = (XL6_IrType*)xl6IrTypeGet(&module->types, instruction->type);
        XL6_IrOwnershipAbi ownership = type->ownershipAbi;
        type->ownershipAbi = XL6_IR_OWNERSHIP_ABI_MANAGED;
        reject(module);
        type->ownershipAbi = ownership;
        ++producers;
    }
    assert(targets && producers >= 5 && xl6IrVerifyModule(module, NULL));
    size_t captures = module->callbackCaptureCount;
    module->callbackCaptureCount = module->callbackCaptureCapacity + 1;
    reject(module);
    module->callbackCaptureCount = captures;
    size_t instances = module->instantiationCount;
    module->instantiationCount = module->instantiationCapacity + 1;
    reject(module);
    module->instantiationCount = instances;
    assert(xl6IrVerifyModule(module, NULL));
    printf("empty payload: %zu exact native targets, %zu zero-storage producers, %zu zero-C/header refusals\n",
        targets, producers, refusals);
    xl6CompileUnitFree(&unit);
    return 0;
}
