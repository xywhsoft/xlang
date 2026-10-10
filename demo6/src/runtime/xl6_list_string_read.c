#include "src/runtime/xl6_list_typed_read.h"
#include "src/runtime/xl6_list_typed_read_internal.h"
#include "src/runtime/xl6_string_type.h"

xstrview xl6ListReadStringV1(const xvalue* source,uint64_t identity,uint64_t schema,
    uint64_t element,int64_t key)
{
    xstrview result={0},view={0}; xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return result;
    }
    const xvalue* wire=NULL; xvalue* pin=NULL;
    int status=xl6ListTypedReadSlot(source,identity,schema,element,key,2,sizeof(result),
        xl6StringTypeV1(),&result,&wire);
    bool success=status==1;
    if (!status) {
        /* The wire item must survive reentrant allocation of the copy. */
        pin=xrtValueRetain(wire);
        if (pin && xrtValueGetString(pin,&view) && view.Data) {
            result.Data=xrtStrDupN(view.Data,view.Size);
            if (result.Data) { result.Size=view.Size; success=true; }
            else if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
        } else if (!xrtGetError()) xrtSetErrorKind(XERR_TYPE);
    }
    if (!success || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    xrtValueRelease(pin);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    if (scope.failed) { xrtFree((void*)result.Data); result=(xstrview){0}; }
    (void)xl6ListTypedReadFinish(&scope,success);
    return result;
}
