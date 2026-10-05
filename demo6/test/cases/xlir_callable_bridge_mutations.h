#ifndef XLIR_CALLABLE_BRIDGE_MUTATIONS_H
#define XLIR_CALLABLE_BRIDGE_MUTATIONS_H

/* Exercise the complete canonical plan, not only the bridge used by one call. */
static void xlirCallableBridgeMutations(
    XL6_IrModule* ir, void (*reject)(const XL6_IrModule*))
{
    size_t i;
    for (i = 0; i < ir->callableBridgeCount; ++i) {
        XL6_IrCallableBridge* b = &ir->callableBridges[i], saved = *b;
        b->signatureType = XL6_IR_TYPE_INVALID; reject(ir); *b = saved;
        b->firstParameter = SIZE_MAX; reject(ir); *b = saved;
        ++b->parameterCount; reject(ir); *b = saved;
        ++b->fixedParameterCount; reject(ir); *b = saved;
        b->firstResult = SIZE_MAX; reject(ir); *b = saved;
        ++b->resultCount; reject(ir); *b = saved;
        b->resultType = saved.resultType ? XL6_IR_TYPE_INVALID : 1; reject(ir); *b = saved;
        b->resultCommitAbi = XL6_IR_CALLABLE_RESULT_COMMIT_INVALID; reject(ir); *b = saved;
        b->frameCleanupAbi = XL6_IR_CALLABLE_FRAME_CLEANUP_INVALID; reject(ir); *b = saved;
        b->forwardErrorBoundaryAbi = XL6_IR_CALLABLE_ERROR_BOUNDARY_ISOLATED_STATUS; reject(ir); *b = saved;
        b->reverseErrorBoundaryAbi = XL6_IR_CALLABLE_ERROR_BOUNDARY_NATIVE_CALLBACK; reject(ir); *b = saved;
        b->erasureAbi = XL6_IR_CALLABLE_ERASURE_INVALID; reject(ir); *b = saved;
        b->cleanupAbi = XL6_IR_CALLBACK_CLEANUP_INVALID; reject(ir); *b = saved;
        b->errorAbi = XL6_IR_CALLBACK_ERROR_ASYNC_CAUSE; reject(ir); *b = saved;
        b->reverseSupported = !b->reverseSupported; reject(ir); *b = saved;
    }
    for (i = 0; i < ir->callableBridgeParameterCount; ++i) {
        XL6_IrCallableBridgeParameter* p = &ir->callableBridgeParameters[i], saved = *p;
        p->parameter = SIZE_MAX; reject(ir); *p = saved;
        p->type = XL6_IR_TYPE_INVALID; reject(ir); *p = saved;
        p->decodeAbi = XL6_IR_CALLABLE_BRIDGE_INVALID; reject(ir); *p = saved;
        p->encodeAbi = XL6_IR_CALLABLE_BRIDGE_INVALID; reject(ir); *p = saved;
        p->integerDomain.rule = saved.integerDomain.rule == XL6_VALUE_ADMIT_NONE ? XL6_VALUE_ADMIT_I8 : XL6_VALUE_ADMIT_NONE;
        reject(ir); *p = saved;
        p->integerDomain.identity ^= UINT64_C(1); reject(ir); *p = saved;
        p->integerDomain.nominal = !saved.integerDomain.nominal; reject(ir); *p = saved;
        p->nativeScalarProfile = saved.nativeScalarProfile == XL6_NATIVE_SCALAR_NONE ? XL6_NATIVE_SCALAR_CHAR : XL6_NATIVE_SCALAR_NONE;
        reject(ir); *p = saved;
        p->nativeScalarProfile = saved.nativeScalarProfile == XL6_NATIVE_SCALAR_INT ? XL6_NATIVE_SCALAR_UINT : XL6_NATIVE_SCALAR_INT;
        reject(ir); *p = saved;
        p->invokeAbi = saved.invokeAbi == XL6_IR_CALLBACK_INVOKE_TAKE
            ? XL6_IR_CALLBACK_INVOKE_BORROW : XL6_IR_CALLBACK_INVOKE_TAKE;
        reject(ir); *p = saved;
        p->decodedDestroyAbi = saved.decodedDestroyAbi == XL6_IR_ASYNC_FRAME_DESTROY_NONE
            ? XL6_IR_ASYNC_FRAME_DESTROY_FREE_STRING : XL6_IR_ASYNC_FRAME_DESTROY_NONE;
        reject(ir); *p = saved;
        p->inputDestroyAbi = saved.inputDestroyAbi == XL6_IR_ASYNC_FRAME_DESTROY_NONE
            ? XL6_IR_ASYNC_FRAME_DESTROY_FREE_STRING : XL6_IR_ASYNC_FRAME_DESTROY_NONE;
        reject(ir); *p = saved;
    }
    for (i = 0; i < ir->callableBridgeResultCount; ++i) {
        XL6_IrCallableBridgeResult* r = &ir->callableBridgeResults[i], saved = *r;
        r->result = SIZE_MAX; reject(ir); *r = saved;
        r->type = XL6_IR_TYPE_INVALID; reject(ir); *r = saved;
        r->decodeAbi = XL6_IR_CALLABLE_BRIDGE_INVALID; reject(ir); *r = saved;
        r->encodeAbi = XL6_IR_CALLABLE_BRIDGE_INVALID; reject(ir); *r = saved;
        r->integerDomain.rule = saved.integerDomain.rule == XL6_VALUE_ADMIT_NONE ? XL6_VALUE_ADMIT_I8 : XL6_VALUE_ADMIT_NONE;
        reject(ir); *r = saved;
        r->integerDomain.identity ^= UINT64_C(1); reject(ir); *r = saved;
        r->integerDomain.nominal = !saved.integerDomain.nominal; reject(ir); *r = saved;
        r->nativeScalarProfile = saved.nativeScalarProfile == XL6_NATIVE_SCALAR_NONE ? XL6_NATIVE_SCALAR_CHAR : XL6_NATIVE_SCALAR_NONE;
        reject(ir); *r = saved;
        r->nativeScalarProfile = saved.nativeScalarProfile == XL6_NATIVE_SCALAR_INT ? XL6_NATIVE_SCALAR_UINT : XL6_NATIVE_SCALAR_INT;
        reject(ir); *r = saved;
        r->destroyAbi = saved.destroyAbi == XL6_IR_ASYNC_FRAME_DESTROY_NONE
            ? XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_XVALUE : XL6_IR_ASYNC_FRAME_DESTROY_NONE;
        reject(ir); *r = saved;
    }
    {
        size_t count = ir->callableBridgeResultCount;
        size_t capacity = ir->callableBridgeResultCapacity;
        XL6_IrCallableBridgeResult* results = ir->callableBridgeResults;
        assert(count); ir->callableBridgeResultCount = 0; reject(ir); ir->callableBridgeResultCount = count;
        ir->callableBridgeResultCapacity = 0; reject(ir); ir->callableBridgeResultCapacity = capacity;
        ir->callableBridgeResults = NULL; reject(ir); ir->callableBridgeResults = results;
        if (count > 1) {
            XL6_IrCallableBridgeResult saved = results[0];
            results[0] = results[1]; results[0].result = SIZE_MAX; reject(ir); results[0] = saved;
        }
    }
    {
        size_t count = ir->callableBridgeCount;
        assert(count); ir->callableBridgeCount = 0; reject(ir); ir->callableBridgeCount = count;
        count = ir->callableBridgeParameterCount;
        ir->callableBridgeParameterCount = count ? 0 : 1;
        reject(ir); ir->callableBridgeParameterCount = count;
    }
    {
        size_t capacity = ir->callableBridgeCapacity;
        XL6_IrCallableBridge* bridges = ir->callableBridges;
        ir->callableBridgeCapacity = 0; reject(ir); ir->callableBridgeCapacity = capacity;
        ir->callableBridges = NULL; reject(ir); ir->callableBridges = bridges;
        capacity = ir->callableBridgeParameterCapacity;
        XL6_IrCallableBridgeParameter* parameters = ir->callableBridgeParameters;
        if (ir->callableBridgeParameterCount) {
            ir->callableBridgeParameterCapacity = 0; reject(ir); ir->callableBridgeParameterCapacity = capacity;
            ir->callableBridgeParameters = NULL; reject(ir); ir->callableBridgeParameters = parameters;
        }
    }
    if (ir->callableBridgeCount > 1) {
        XL6_IrCallableBridge saved = ir->callableBridges[0];
        ir->callableBridges[0] = ir->callableBridges[1]; reject(ir); ir->callableBridges[0] = saved;
    }
    if (ir->callableBridgeParameterCount > 1) {
        XL6_IrCallableBridgeParameter saved = ir->callableBridgeParameters[0];
        ir->callableBridgeParameters[0] = ir->callableBridgeParameters[1];
        reject(ir); ir->callableBridgeParameters[0] = saved;
    }
}
#endif
