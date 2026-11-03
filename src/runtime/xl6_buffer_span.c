#include "src/runtime/xl6_buffer_internal.h"
#include "xl6_native_handle.h"

bool xl6BufferSpanBeginV1(xvalue* value,bool readOnly,XL6_BufferSpanV1* scope)
{
    if(!scope || (uintptr_t)scope % _Alignof(XL6_BufferSpanV1) || scope->data || scope->bytes) {
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    if(!xl6NativeHandleAccessBeginV1(value,xl6BufferHandlePolicyV1Get(),readOnly ?
        XL6_NATIVE_HANDLE_ACCESS_READ_SPAN_V1 : XL6_NATIVE_HANDLE_ACCESS_WRITE_SPAN_V1,&scope->access))return false;
    xbuffer* buffer=xl6BufferChecked(scope->access.raw);
    if(!buffer) {
        xerror* error=xrtTakeError();
        (void)xl6NativeHandleAccessEndV1(&scope->access);xrtClearError();xrtSetErrorTake(error);return false;
    }
    scope->data=buffer->Data;scope->bytes=buffer->Size;return true;
}
bool xl6BufferSpanEndV1(XL6_BufferSpanV1* scope)
{
    if(!scope || (uintptr_t)scope % _Alignof(XL6_BufferSpanV1)) {xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(!scope->access.self || !xl6ForeignContextCallContainsV1(&scope->access.execution)) {
        xrtSetErrorKind(XERR_STATE);return false;
    }
    scope->data=NULL;scope->bytes=0;
    return xl6NativeHandleAccessEndV1(&scope->access);
}
