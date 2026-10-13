#include "src/runtime/xl6_stack_erased_methods.h"
#include "src/runtime/xl6_stack_batch.h"
#include "src/runtime/xl6_stack.h"
#include "src/api/xl_foreign_error.h"

static bool integer(const xvalue* value,int64_t* output)
{
    if (!value || xrtValueType(value)!=XVALUE_INT) { xrtSetErrorKind(XERR_TYPE); return false; }
    return xrtValueGetInt(value,output);
}
xvalue* xl6StackErasedMethodV1(const xvalue* value,unsigned method,const xvalue* arg0,const xvalue* arg1)
{
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return NULL;
    }
    XL6_Sequence* pin=NULL; XL6_SequenceLayoutV1 layout={0};
    const xvalueconverter* converter=NULL; XL6_SequenceAdmissionV3 admission={0};
    xvalue* result=NULL; uint64_t arrayIdentity=0; int64_t depth=0,maximum=0; bool equal=false;
    if (method<XL6_STACK_ERASED_PUSH_BATCH || method>XL6_STACK_ERASED_EQUALS || !arg0 ||
        ((method==XL6_STACK_ERASED_PEEK_BATCH_AT)!=(arg1!=NULL))) { xrtSetErrorKind(XERR_ARGUMENT); goto done; }
    int status=xl6SequenceBeginErasedRoleV1(value,XL6_SEQUENCE_STACK_V1,&pin,&layout,&converter);
    if (status!=1) { if(!status)xrtSetErrorKind(XERR_TYPE); goto done; }
    if (method==XL6_STACK_ERASED_EQUALS) {
        if (!xl6SequenceAdmissionCopyV3(pin,&admission)) goto done;
        if (!xl6SequenceAdmitsSchemaV1(arg0,XL6_SEQUENCE_STACK_V1,&layout,&admission)) {
            xrtSetErrorKind(XERR_TYPE); goto done;
        }
        xl6SequenceEndV1(pin); pin=NULL;
        equal=xl6StackEqualsV1(value,arg0,layout.identity,layout.elementIdentity,layout.transfer,layout.size,layout.type);
    } else {
        arrayIdentity=xl6SequenceStackResultArrayV1(pin);
        if (!arrayIdentity) goto done;
        xl6SequenceEndV1(pin); pin=NULL;
        if (method==XL6_STACK_ERASED_PUSH_BATCH) {
            if (xrtValueTypeId(arg0)!=arrayIdentity) { xrtSetErrorKind(XERR_TYPE); goto done; }
            equal=xl6StackPushBatchV1(value,layout.identity,layout.elementIdentity,layout.transfer,layout.size,layout.type,arg0,arrayIdentity);
        } else {
            if (method==XL6_STACK_ERASED_PEEK_BATCH_AT) {
                if (!integer(arg0,&depth) || !integer(arg1,&maximum)) goto done;
            } else if (!integer(arg0,&maximum)) goto done;
            result=xl6StackExtractArrayV1(value,layout.identity,layout.elementIdentity,layout.transfer,layout.size,layout.type,
                arrayIdentity,depth,maximum,method==XL6_STACK_ERASED_POP_BATCH,converter);
        }
    }
    if ((method==XL6_STACK_ERASED_EQUALS || method==XL6_STACK_ERASED_PUSH_BATCH) && !scope.failed && !xrtGetError()) {
        if (method==XL6_STACK_ERASED_PUSH_BATCH && !equal) goto done;
        result=xrtValueBool(equal);
    }
done:
    if (!result || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    if (pin) xl6SequenceEndV1(pin);
    xl6SequenceAdmissionDropV3(&admission);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    if (scope.failed) { xrtValueRelease(result); result=NULL; }
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return result;
}
