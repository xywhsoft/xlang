#include "xl6_dynamic_fields.h"
#include "xl6_array_graph.h"
#include "src/api/xl_foreign_error.h"

/* XRT's ordinary handle Clone may retain a reference-class carrier. The
 * existing language graph copier knows native Array admission and layout.
 * Collect only borrowed references first, then copy through ONE XRT memo.
 * This optional provider is not linked for core-only field operations. */
static xvalue* finishValue(xl6_error_scope* scope, xvalue* result)
{
    if (!result || xrtGetError()) xl6ErrorScopeCaptureFailure(scope);
    if (scope->failed) { xrtValueRelease(result); result = NULL; }
    xl6_error_result outcome = xl6ErrorScopeEnd(scope);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return result;
}
static bool appendRef(xvalue* array, const xvalue* value)
{
    xvalue* owned = xrtValueRetain(value);
    bool ok = owned && xrtValueArrayAppendTake(array, &owned);
    xrtValueRelease(owned); return ok;
}
static bool collectRef(xstrview name, const xvalue* borrowed, ptr context)
{
    xvalue* owned = xrtValueRetain(borrowed);
    bool ok = owned && xrtValueObjectSetTake(context, name, &owned);
    xrtValueRelease(owned); return ok;
}
xvalue* xl6_xrt_dynamic_fields_to_value(void* payload)
{
    xl6_error_scope operation = {0};
    if (!xl6ErrorScopeBegin(&operation)) return NULL;
    xvalue* borrowed = xrtValueObject();
    xvalue* result = borrowed && xrtDynamicFieldsVisitV1(payload, collectRef, borrowed)
        ? xl6ValueDeepCloneV1(borrowed) : NULL;
    if (!result) xl6ErrorScopeCaptureFailure(&operation);
    xrtValueRelease(borrowed);
    return finishValue(&operation, result);
}
xvalue* xl6_xrt_dynamic_fields_copy(void* payload, xstrview name)
{
    xl6_error_scope operation = {0};
    if (!xl6ErrorScopeBegin(&operation)) return NULL;
    xvalue* borrowed = xrtDynamicFieldsGetRef(payload, name);
    xvalue* result = borrowed ? xl6ValueDeepCloneV1(borrowed)
        : xrtGetError() ? NULL : xrtValueNull();
    if (!result) xl6ErrorScopeCaptureFailure(&operation);
    xrtValueRelease(borrowed);
    return finishValue(&operation, result);
}
/* The graph has already been isolated. Store its actual owning edges; using
 * FromValue here would perform another per-field deep clone and lose memo
 * relationships. Partial construction remains private until it succeeds. */
static void* fromCopiedObject(const xvalue* copy)
{
    void* result = xl6_xrt_dynamic_fields_create();
    size_t count = xrtValueCount(copy);
    bool ok = result && xrtDynamicFieldsReserve(result, count);
    for (size_t i = 0; ok && i < count; ++i) {
        xstrview name;
        const xvalue* value = xrtValueObjectAt(copy, i, &name);
        ok = value && xrtDynamicFieldsSetRef(result, name, value);
    }
    if (!ok) { xl6_xrt_dynamic_fields_destroy(result); return NULL; }
    return result;
}
static void* finishFields(xl6_error_scope* scope, void* result)
{
    if (!result || xrtGetError()) xl6ErrorScopeCaptureFailure(scope);
    if (scope->failed) { xl6_xrt_dynamic_fields_destroy(result); result = NULL; }
    xl6_error_result outcome = xl6ErrorScopeEnd(scope);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return result;
}
void* xl6_xrt_dynamic_fields_from_value(const xvalue* source)
{
    xl6_error_scope operation = {0};
    if (!xl6ErrorScopeBegin(&operation)) return NULL;
    if (xrtValueType(source) != XVALUE_OBJECT) xrtSetErrorKind(XERR_TYPE);
    xvalue* copy = xrtGetError() ? NULL : xl6ValueDeepCloneV1(source);
    void* result = copy ? fromCopiedObject(copy) : NULL;
    if (!result) xl6ErrorScopeCaptureFailure(&operation);
    xrtValueRelease(copy);
    return finishFields(&operation, result);
}
void* xl6_xrt_dynamic_fields_clone(void* source)
{
    xl6_error_scope operation = {0};
    if (!xl6ErrorScopeBegin(&operation)) return NULL;
    xvalue* copy = xl6_xrt_dynamic_fields_to_value(source);
    void* result = copy ? fromCopiedObject(copy) : NULL;
    if (!result) xl6ErrorScopeCaptureFailure(&operation);
    xrtValueRelease(copy);
    return finishFields(&operation, result);
}
static xvalue* collect(void* source, bool pairs)
{
    xl6_error_scope operation = {0};
    if (!xl6ErrorScopeBegin(&operation)) return NULL;
    xvalue* copy = xl6_xrt_dynamic_fields_to_value(source);
    xvalue* result = copy ? xrtValueArray() : NULL;
    bool ok = result && xrtValueReserve(result, xrtValueCount(copy));
    for (size_t i = 0; ok && i < xrtValueCount(copy); ++i) {
        xstrview name; const xvalue* value = xrtValueObjectAt(copy, i, &name);
        if (!pairs) ok = appendRef(result, value);
        else {
            xvalue* pair = xrtValueArray();
            xvalue* key = pair ? xrtValueString(name) : NULL;
            ok = key && xrtValueArrayAppendTake(pair, &key) && appendRef(pair, value)
                && xrtValueArrayAppendTake(result, &pair);
            if (!ok) xl6ErrorScopeCaptureFailure(&operation);
            xrtValueRelease(key); xrtValueRelease(pair);
        }
    }
    if (!ok) xl6ErrorScopeCaptureFailure(&operation);
    xrtValueRelease(copy);
    return finishValue(&operation, result);
}
xvalue* xl6_xrt_dynamic_fields_values(void* source) { return collect(source, false); }
xvalue* xl6_xrt_dynamic_fields_items(void* source) { return collect(source, true); }
bool xl6_xrt_dynamic_fields_merge(void* target, void* source, bool replace)
{
    xl6_error_scope operation = {0};
    if (!xl6ErrorScopeBegin(&operation)) return false;
    xvalue* copy = xl6_xrt_dynamic_fields_to_value(source);
    void* fields = copy ? fromCopiedObject(copy) : NULL;
    /* Preserve XRT's transactional merge, including replacement order and
     * rollback. Its downstream handle clone retains the isolated carriers,
     * not the original language collections. */
    bool ok = fields && xrtDynamicFieldsMerge(target, fields, replace);
    if (!ok) xl6ErrorScopeCaptureFailure(&operation);
    xl6_xrt_dynamic_fields_destroy(fields); xrtValueRelease(copy);
    xl6_error_result outcome = xl6ErrorScopeEnd(&operation);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return ok && outcome.ok;
}
