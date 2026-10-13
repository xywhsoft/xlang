#include "src/runtime/xl6_char_checked.h"
#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_value_domain.h"
#include "src/api/xl_foreign_error.h"

bool xl6CharValueCheckedV1(const xvalue* source,uint64_t expected,bool strict,uint32_t* output)
{
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return false;
    }
    uint64_t code=0; uint32_t character=0; int64_t integer=0; bool decoded=false;
    if (!expected || !output) xrtSetErrorKind(XERR_ARGUMENT);
    else if (!source) xrtSetErrorKind(XERR_TYPE);
    else {
        xvaluetype tag=xrtValueType(source);
        if (tag==XVALUE_CHAR && (!strict || !xrtValueTypeId(source) || xrtValueTypeId(source)==expected)) {
            decoded=xrtValueGetChar(source,&character); code=character;
            if (!decoded) xl6ErrorScopeCaptureFailure(&scope);
        } else if (!strict && tag==XVALUE_INT) {
            decoded=xrtValueGetInt(source,&integer);
            if (!decoded) xl6ErrorScopeCaptureFailure(&scope);
            else if (integer<0) { decoded=false; xrtSetErrorKind(XERR_RANGE); }
            else code=(uint64_t)integer;
        } else if (!strict && tag==XVALUE_UINT) {
            decoded=xrtValueGetUInt(source,&code);
            if (!decoded) xl6ErrorScopeCaptureFailure(&scope);
        } else xrtSetErrorKind(XERR_TYPE);
        if (decoded && !scope.failed && !xrtGetError() &&
            (code>UINT32_MAX || !xl6CharDomainV1((uint32_t)code))) {
            decoded=false; xrtSetErrorKind(XERR_RANGE);
        }
    }
    if (!decoded || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) { xl6ErrorScopePropagateFailure(outcome); return false; }
    *output=(uint32_t)code; return true;
}
