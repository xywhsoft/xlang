#include "src/runtime/xl6_sequence_projection_schema.h"
#include "src/api/xl_foreign_error.h"
#include <string.h>

static bool enter(xl6_error_scope* scope)
{
    if (xl6ErrorScopeBegin(scope)) return true;
    xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
    return false;
}
static bool end(xl6_error_scope* scope, bool ok)
{
    if (!ok || xrtGetError()) xl6ErrorScopeCaptureFailure(scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(scope);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return outcome.ok;
}
static bool empty(const XL6_SequenceProjectionSchemaV1* schema)
{
    return schema && !schema->role && !schema->layout.identity &&
        !schema->layout.elementIdentity && !schema->layout.transfer &&
        !schema->layout.size && !schema->layout.type && !schema->admission.ops &&
        !schema->admission.data && !schema->admission.elementIdentity &&
        !schema->admission.transfer && !schema->admission.size &&
        !schema->admission.type && !schema->admission.boxedIdentity && !schema->admission.scalarDomain;
}
static bool valid(const XL6_SequenceProjectionSchemaV1* schema)
{
    return schema && (schema->role==XL6_SEQUENCE_ARRAY_V1 || schema->role==XL6_SEQUENCE_STACK_V1) &&
        schema->admission.ops && schema->admission.data &&
        schema->admission.elementIdentity==schema->layout.elementIdentity &&
        schema->admission.transfer==schema->layout.transfer &&
        schema->admission.size==schema->layout.size && schema->admission.type==schema->layout.type &&
        xl6SequenceTupleV1(schema->layout.identity,schema->layout.elementIdentity,
            schema->layout.transfer,schema->layout.size,schema->layout.type);
}
bool xl6SequenceProjectionPrepareV1(const xvalue* witness,XL6_SequenceRoleV1 role,
    XL6_SequenceProjectionSchemaV1* output)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return false;
    XL6_SequenceProjectionSchemaV1 value={0}; bool ok=false;
    if (!empty(output) || (role!=XL6_SEQUENCE_ARRAY_V1 && role!=XL6_SEQUENCE_STACK_V1))
        xrtSetErrorKind(XERR_ARGUMENT);
    else {
        ok=xl6SequenceSchemaCopyV1(witness,&value.role,&value.layout,&value.admission);
        if (!ok) xl6ErrorScopeCaptureFailure(&scope);
        else if (value.role!=role || !valid(&value)) { ok=false; xrtSetErrorKind(XERR_TYPE); }
    }
    if (!ok || xrtGetError() || scope.failed) {
        xl6ErrorScopeCaptureFailure(&scope);
        xl6SequenceAdmissionDropV3(&value.admission);
        return end(&scope,false);
    }
    if (!end(&scope,true)) {
        xerror* primary=xrtTakeError();
        xl6SequenceProjectionDisposeV1(&value);
        xrtClearError(); xl6ErrorScopePropagateFailure((xl6_error_result){false,primary});
        return false;
    }
    *output=value; return true;
}
bool xl6SequenceProjectionApplyV1(const XL6_SequenceProjectionSchemaV1* schema,
    const xvalue* source,bool nullable,xvalue** output)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return false;
    xvalue* result=NULL;
    if (!output || *output || !valid(schema)) xrtSetErrorKind(XERR_ARGUMENT);
    else if (!source) xrtSetErrorKind(XERR_TYPE);
    else if (xl6SequenceAdmitsSchemaV1(source,schema->role,&schema->layout,&schema->admission))
        result=xrtValueRetain(source);
    else if (nullable) result=xrtValueRetain(xrtValueNull());
    else xrtSetErrorKind(XERR_TYPE);
    if (!result || xrtGetError() || scope.failed) {
        xl6ErrorScopeCaptureFailure(&scope);
        xrtValueRelease(result);
        return end(&scope,false);
    }
    if (!end(&scope,true)) {
        xerror* primary=xrtTakeError(); xrtValueRelease(result);
        xrtClearError(); xl6ErrorScopePropagateFailure((xl6_error_result){false,primary});
        return false;
    }
    *output=result; return true;
}
bool xl6SequenceProjectionDisposeV1(XL6_SequenceProjectionSchemaV1* schema)
{
    if (!schema) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    XL6_SequenceProjectionSchemaV1 owned=*schema;
    *schema=(XL6_SequenceProjectionSchemaV1){0};
    xl6_error_scope cleanup={0};
    if (!xl6ErrorScopeBeginCleanup(&cleanup,false)) {
        xerror* primary=xrtTakeError();
        xl6SequenceAdmissionDropV3(&owned.admission);
        xrtClearError(); xl6ErrorScopePropagateFailure((xl6_error_result){false,primary});
        return false;
    }
    xl6SequenceAdmissionDropV3(&owned.admission);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&cleanup);
    xl6_error_result result=xl6ErrorScopeEnd(&cleanup);
    if (!result.ok) xl6ErrorScopePropagateFailure(result);
    return result.ok;
}

bool xl6SequenceProjectionWitnessTakeV1(xvalue* witness,XL6_SequenceRoleV1 role,
    const xvalue* source,bool nullable,xvalue** output)
{
    xl6_error_scope scope={0};
    if (!enter(&scope)) {
        xerror* primary=xrtTakeError(); xrtValueRelease(witness);
        xrtClearError(); xl6ErrorScopePropagateFailure((xl6_error_result){false,primary});
        return false;
    }
    XL6_SequenceProjectionSchemaV1 schema={0}; xvalue* result=NULL; bool prepared=false;
    if (!output || *output || source==witness) xrtSetErrorKind(XERR_ARGUMENT);
    else prepared=xl6SequenceProjectionPrepareV1(witness,role,&schema);
    if (!prepared || xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
    xrtValueRelease(witness);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    if (prepared && !scope.failed && !xl6SequenceProjectionApplyV1(&schema,source,nullable,&result))
        xl6ErrorScopeCaptureFailure(&scope);
    if (!xl6SequenceProjectionDisposeV1(&schema)) xl6ErrorScopeCaptureFailure(&scope);
    if (!result || scope.failed || xrtGetError()) {
        xl6ErrorScopeCaptureFailure(&scope); xrtValueRelease(result);
        return end(&scope,false);
    }
    if (!end(&scope,true)) {
        xerror* primary=xrtTakeError(); xrtValueRelease(result);
        xrtClearError(); xl6ErrorScopePropagateFailure((xl6_error_result){false,primary});
        return false;
    }
    *output=result; return true;
}
