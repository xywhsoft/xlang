#include "src/runtime/xl6_string_type.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>

/* Borrowed inputs, independent output and exact byte length, including NUL.
 * Capture failures before every temporary is retired: allocator/drop hooks
 * may report another error, but must not replace the operation's first one. */
XL6_TEXT_API xvalue* xl6ValueStringConcatV1(const xvalue* left, const xvalue* right)
{
    xerror* incoming = xrtTakeError();
    xl6_error_scope operation = {0};
    if (!xl6ErrorScopeBegin(&operation)) abort();
    xvalue* a = NULL; xvalue* b = NULL; xvalue* result = NULL;
    char* joined = NULL; xstrview av = {0}, bv = {0};
    if (!left || !right || xrtValueType(left) != XVALUE_STRING ||
        xrtValueType(right) != XVALUE_STRING) {
        xrtSetErrorKind(XERR_TYPE); goto done;
    }
    a = xrtValueRetain(left); b = xrtValueRetain(right);
    if (!a || !b || !xrtValueGetString(a, &av) || !xrtValueGetString(b, &bv)) goto done;
    if (av.Size > SIZE_MAX - bv.Size || av.Size + bv.Size == SIZE_MAX) {
        xrtSetErrorKind(XERR_RANGE); goto done;
    }
    joined = xrtStrConcat(av, bv);
    if (joined) result = xrtValueString((xstrview){joined, av.Size + bv.Size});
done:
    if (!result || xrtGetError()) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
        xl6ErrorScopeCaptureFailure(&operation);
    }
    xrtFree(joined);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&operation);
    xrtValueRelease(b); xrtValueRelease(a);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&operation);
    if (operation.failed) { xrtValueRelease(result); result = NULL; }
    xl6_error_result outcome = xl6ErrorScopeEnd(&operation);
    if (!outcome.ok) { xrtErrorFree(incoming); xl6ErrorScopePropagateFailure(outcome); }
    else xrtSetErrorTake(incoming);
    return result;
}

/* Projection is a separate resident island from storage-only operations. */
static bool toTyped(const xvalue* source, const xrttype* type, ptr target, ptr context)
{
    (void)context;
    if (!source || !target) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (type != xl6StringTypeV1()) { xrtSetErrorKind(XERR_TYPE); return false; }
    xstrview view={0};
    if (!xrtValueGetString(source,&view)) return false;
    return xrtTypeCopyValue(type,target,&view);
}
static xvalue* fromTyped(const xrttype* type, const void* source, ptr context)
{
    (void)context;
    if (!source) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    if (type != xl6StringTypeV1()) { xrtSetErrorKind(XERR_TYPE); return NULL; }
    const xstrview* view=source;
    if (!view->Data) { xrtSetErrorKind(XERR_TYPE); return NULL; }
    return xrtValueString(*view);
}
XL6_TEXT_API const xvalueconverter* xl6StringConverterV1(void)
{
    static const xvalueconverter converter={.ToTyped=toTyped,.FromTyped=fromTyped};
    return &converter;
}
