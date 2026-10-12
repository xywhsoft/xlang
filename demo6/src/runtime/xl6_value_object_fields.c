#include "xl6_value_object_fields.h"
#include "xl6_dynamic_fields.h"
#include "src/api/xl_foreign_error.h"

static bool open_fields(const xvalue* source,xrtdynamicfields** output)
{
    XL6_ClassDynamicFieldPathV3 path={0};
    int kind=xl6ClassDynamicFieldPathV3(source,&path);
    if (kind<0) return false;
    *output=NULL;
    if (!kind) return true;
    const xvalue* storage=xrtValueObjectGet(source,(xstrview){path.storageName,path.storageSize});
    const xvalue* boxed=storage ? xrtValueObjectGet(storage,(xstrview){path.handleName,path.handleSize}) : NULL;
    ptr raw=NULL,user=NULL; const xvaluehandleops* ops=NULL;
    /* Never dereference a foreign payload or execute its policy callbacks. */
    if (!boxed || !xrtValueGetHandle(boxed,&raw,&ops,&user) || !raw || user ||
        ops!=xl6DynamicFieldsHandlePolicyV1Get()->Ops) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_TYPE);
        return false;
    }
    *output=raw; return true;
}
xvalue* xl6ValueObjectFieldFindV1(const xvalue* source,const char* name,size_t size)
{
    xstrview key={name,size};
    xvalue* declared=xrtValueObjectGet(source,key);
    if (declared || xrtGetError()) return declared;
    xrtdynamicfields* fields=NULL;
    if (!open_fields(source,&fields)) return NULL;
    return fields ? (xvalue*)xrtDynamicFieldsGet(fields,key) : NULL;
}
bool xl6ValueObjectFieldSetTakeV1(xvalue* source,const char* name,size_t size,xvalue* item)
{
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) {
        xerror* primary=xrtTakeError();
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorRef(primary)});
        xrtClearError(); xrtValueRelease(item); xrtClearError(); xrtSetErrorTake(primary);
        return false;
    }
    bool success=false; xvalue* receiverPin=NULL; xvalue* retiredPin=NULL;
    xstrview key={name,size}; xrtdynamicfields* fields=NULL;
    if (!source || !item || (!name && size)) xrtSetErrorKind(XERR_ARGUMENT);
    else {
        receiverPin=xrtValueRetain(source);
        if (receiverPin) {
            const xvalue* old=xrtValueObjectGet(source,key);
            bool ready=!xrtGetError() && (old || open_fields(source,&fields));
            if (ready && !old && fields)
                old=xrtDynamicFieldsGet(fields,key);
            if (ready && !xrtGetError() && old) retiredPin=xrtValueRetain(old);
            if (ready && !xrtGetError() && (!old || retiredPin))
                /* DynamicFields SetTake isolates by cloning, unlike the
                 * language's shared field assignment. SetRef installs one
                 * real owner; consume our input below, outside mutation. */
                success=fields ? xrtDynamicFieldsSetRef(fields,key,item) :
                    xrtValueObjectSetTake(source,key,&item);
        }
    }
    if (!success || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    xrtValueRelease(item);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    xrtValueRelease(retiredPin);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    xrtValueRelease(receiverPin);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return outcome.ok;
}
