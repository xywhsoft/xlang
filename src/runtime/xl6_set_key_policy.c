#include "src/runtime/xl6_set_key_policy.h"
#include "src/api/xl_foreign_error.h"
#include "xl6_native_leaf_value.h"

/* xset's payload-only callback ABI observes the native diagnostic slot. A
 * failed checked callback can have no allocated diagnostic: first propagate
 * that exact outcome to the parent, THEN supply the allocation-free native
 * refusal. A legitimate zero hash/false equality is never a failure. */
static void callbackFailure(xl6_error_result result)
{
    xl6ErrorScopePropagateFailure(result);
    if (!xrtGetError()) xrtSetErrorKind(XERR_STATE);
}
static bool finish(xl6_error_scope* scope)
{
    if (scope->failed || xrtGetError()) xl6ErrorScopeCaptureFailure(scope);
    xl6_error_result result=xl6ErrorScopeEnd(scope);
    if (!result.ok) callbackFailure(result);
    return result.ok;
}
typedef struct ReferenceKey {
    xvaluetype tag;
    uint64 type;
    const void* object;
    const xvaluehandleops* ops;
    const void* context;
    bool reference;
} ReferenceKey;
/* Container shells are the stable mutable instance, not their COW backing.
 * Native handles may have several shells around one owner: use the exact
 * owner/producer/context domain, without calling its semantic callbacks.
 * Readability failures (including XRT callback BUSY) remain checked errors.
 * Neither this record nor the hash is a portable/persistent object ID. */
static bool referenceKey(const xvalue* value,ReferenceKey* output)
{
    xvaluetype tag=xrtValueType(value);
    if (tag==XVALUE_INVALID) return false;
    *output=(ReferenceKey){.tag=tag,.type=xrtValueTypeId(value)};
    switch (tag) {
    case XVALUE_ARRAY: case XVALUE_INT_MAP: case XVALUE_SET: case XVALUE_OBJECT:
        output->reference=true; output->object=value; return true;
    case XVALUE_HANDLE: {
        /* Only the authenticated immutable native-value producer grants
         * content semantics. A matching type ID or foreign hash callback does
         * not grant it. No key-specific library is linked by this policy. */
        if (xl6NativeLeafValueIsV1(value)) return true;
        ptr object=NULL,context=NULL; const xvaluehandleops* ops=NULL;
        if (!xrtValueGetHandle(value,&object,&ops,&context)) return false;
        output->reference=true; output->object=object; output->ops=ops; output->context=context; return true;
    }
    default: return true;
    }
}
static uint64 keyHash(const ReferenceKey* key)
{
    /* An explicit scalar array avoids struct padding and function-pointer
     * representations. All converted pointers denote data, not executable code. */
    const uint64 fields[5]={(uint64)key->tag,key->type,(uint64)(uintptr_t)key->object,
        (uint64)(uintptr_t)key->ops,(uint64)(uintptr_t)key->context};
    return xrtHash64(fields,sizeof(fields));
}
static uint64 referenceHash(const void* slot,ptr context)
{
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) {
        callbackFailure((xl6_error_result){false,xrtTakeError()}); return 0;
    }
    uint64 hash=0;
    if (!slot || context) xrtSetErrorKind(XERR_ARGUMENT);
    else {
        const xvalue* value=*(xvalue* const*)slot;
        value=value ? value : xrtValueNull(); ReferenceKey key={0};
        if (!referenceKey(value,&key)) xl6ErrorScopeCaptureFailure(&scope);
        else if (key.reference) hash=keyHash(&key);
        else if (!xrtValueHash(value,&hash)) xl6ErrorScopeCaptureFailure(&scope);
    }
    return finish(&scope) ? hash : 0;
}
static bool referenceEqual(const void* left,const void* right,ptr context)
{
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) {
        callbackFailure((xl6_error_result){false,xrtTakeError()}); return false;
    }
    bool equal=false;
    if (!left || !right || context) xrtSetErrorKind(XERR_ARGUMENT);
    else {
        const xvalue* a=*(xvalue* const*)left,*b=*(xvalue* const*)right;
        a=a ? a : xrtValueNull(); b=b ? b : xrtValueNull(); ReferenceKey ka={0},kb={0};
        if (!referenceKey(a,&ka) || !referenceKey(b,&kb)) xl6ErrorScopeCaptureFailure(&scope);
        else if (ka.reference || kb.reference)
            equal=ka.reference==kb.reference && ka.tag==kb.tag && ka.type==kb.type && ka.object==kb.object && ka.ops==kb.ops && ka.context==kb.context;
        else equal=xrtValueEqual(a,b);
    }
    return finish(&scope) && equal;
}
static uint64 futureHash(const void* slot,ptr context)
{
    if (!slot || context) { xrtSetErrorKind(XERR_ARGUMENT); return 0; }
    uintptr_t identity=(uintptr_t)*(xfuture* const*)slot;
    return xrtHash64(&identity,sizeof(identity));
}
static bool futureEqual(const void* left,const void* right,ptr context)
{
    if (!left || !right || context) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    return *(xfuture* const*)left==*(xfuture* const*)right;
}
static const xtypedsetkeypolicy referencePolicy={referenceHash,referenceEqual,NULL};
static const xtypedsetkeypolicy futurePolicy={futureHash,futureEqual,NULL};
const xtypedsetkeypolicy* xl6SetReferenceKeyPolicyV1(void) { return &referencePolicy; }
const xtypedsetkeypolicy* xl6SetFutureKeyPolicyV1(void) { return &futurePolicy; }
