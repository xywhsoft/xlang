#include "src/runtime/xl6_array_producer.h"
#include "src/runtime/xl6_array_erased.h"
#include <limits.h>
#include <stdlib.h>

bool xl6ArrayProducerBeginV1(XL6_ArrayProducerV1* state, uint64_t identity,
    uint64_t element, unsigned int rule, const xrttype* type, const xvalueconverter* converter)
{
    *state=(XL6_ArrayProducerV1){0};
    if (!xl6ErrorScopeBegin(&state->transaction)) abort();
    if (!type || (rule!=XL6_VALUE_ADMIT_STRING && rule!=XL6_VALUE_ADMIT_CHAR && rule!=XL6_VALUE_ADMIT_I64)) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    unsigned int transfer=rule==XL6_VALUE_ADMIT_STRING ? 2u : 1u;
    state->result=xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,identity,element,
        transfer,type->Size,type,rule,false,element);
    if (!state->result) return false;
    state->result=xl6ArrayProjectionTakeV1(state->result,identity,element,transfer,type->Size,type,converter);
    if (!state->result) return false;
    state->pin=xl6SequenceBeginV1(state->result,identity,element,transfer,type->Size,type);
    return state->pin!=NULL;
}
bool xl6ArrayProducerReserveV1(XL6_ArrayProducerV1* state, size_t count)
{
    if (!state->pin) { xrtSetErrorKind(XERR_STATE); return false; }
    if (count>INT64_MAX) { xrtSetErrorKind(XERR_RANGE); return false; }
    return xrtTypedArrayReserve(xl6SequenceStorageV1(state->pin),count);
}
bool xl6ArrayProducerPushV1(XL6_ArrayProducerV1* state, const void* item)
{
    if (!state->pin) { xrtSetErrorKind(XERR_STATE); return false; }
    if (xrtTypedArrayCount(xl6SequenceStorageV1(state->pin))>=INT64_MAX) {
        xrtSetErrorKind(XERR_RANGE); return false;
    }
    return xl6SequenceAdmitSlotV1(state->pin,item) &&
        xrtTypedArrayPush(xl6SequenceStorageV1(state->pin),item);
}
xvalue* xl6ArrayProducerFinishV1(XL6_ArrayProducerV1* state, bool success)
{
    if (!success) xl6ErrorScopeCaptureFailure(&state->transaction);
    if (state->pin) { xl6SequenceEndV1(state->pin); state->pin=NULL; }
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&state->transaction);
    xvalue* result=state->result; state->result=NULL;
    if (state->transaction.failed) { xrtValueRelease(result); result=NULL; }
    xl6_error_result outcome=xl6ErrorScopeEnd(&state->transaction);
    /* NULL is the checked failure carrier. Only the language caller decides
     * whether this outcome escapes; do not fail its enclosing callback. */
    if (!outcome.ok) { xrtSetErrorTake(outcome.error); return NULL; }
    return result;
}
