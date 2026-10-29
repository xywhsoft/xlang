#include "src/runtime/xl6_value_object_fields.h"
#include "src/runtime/xl6_value_index_read.h"
#include "src/runtime/xl6_value_index_write.h"
#include "src/runtime/xl6_dynamic_fields.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Deliberately NOT the builtin spellings: the runtime must use IR metadata,
 * including exact key bytes, independently of any emitting module's layouts. */
static const char storage_key[]={'s',0,'t'},handle_key[]={'h',0,'d'};
static const XL6_ClassIdentitySchemaV3 schema={sizeof(schema),0x4578616374506174,71,
    XL6_CLASS_IDENTITY_REFERENCE_V3,XL6_CLASS_IDENTITY_REFERENCE_V3,NULL,0,
    {storage_key,sizeof(storage_key),handle_key,sizeof(handle_key)}};
static unsigned finalizers,drops,reentries;
static bool finalize(xvalue* value,void* context)
{ assert(value && !context); ++finalizers; return true; }
static xvalue* make(bool commit)
{
    const XL6_ClassRuntimeV3* api=xl6ClassRuntimeV3Get();
    xvalue* source=api->Create(NULL,finalize,&schema); assert(source);
    assert(xrtValueTypeIdBind(source,schema.identity) && api->BindIdentityTake(source)==source);
    void* raw=xl6_xrt_dynamic_fields_create(); assert(raw);
    xvalue* boxed=xl6NativeHandleTakeV1(&raw,xl6DynamicFieldsHandlePolicyV1Get());
    xvalue* storage=xrtValueObject(); assert(boxed && !raw && storage);
    assert(xrtValueObjectSetNew(storage,(xstrview){handle_key,sizeof(handle_key)},boxed));
    assert(xrtValueObjectSetNew(source,(xstrview){storage_key,sizeof(storage_key)},storage));
    assert(xrtValueObjectSetNew(source,XRT_STR_LITERAL("declared"),xrtValueInt(3)));
    if (commit) assert(api->CommitTake(source)==source);
    return source;
}
static int64_t integer(const xvalue* value)
{
    int64_t result=0;
    if (!value || !xrtValueGetInt(value,&result))
        fprintf(stderr,"object-field integer projection: value=%p type=%d error=%d\n",
            (const void*)value,(int)xrtValueType(value),(int)xrtErrorKind(xrtGetError()));
    assert(value && xrtValueGetInt(value,&result)); return result;
}
static bool leaf(const xvalue* value,xrtownershipvisitor visit,void* context)
{ (void)value; (void)context; return visit!=NULL; }
static void silent_drop(void* raw,void* user)
{ assert(raw==(void*)1 && !user); ++drops; xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL}); }
static xvalue* silent_value(void)
{
    static const xvaluehandleops ops={NULL,silent_drop,NULL,NULL};
    void* raw=(void*)1; xvalue* value=xrtValueHandleTake(&raw,&ops,NULL);
    assert(value && !raw && xrtValueHandleOwnershipBindPhased(value,leaf)); return value;
}
static void forged_drop(void* raw,void* user)
{ assert(raw==(void*)1 && !user); }
static xvalue* reenter_source;
static void reenter_drop(void* raw,void* user)
{
    assert(raw==(void*)2 && !user);
    assert(integer(xl6ValueObjectFieldFindV1(reenter_source,"retired",7))==42);
    ++reentries; xrtValueRelease(reenter_source); reenter_source=NULL;
}
static void routing(void)
{
    xvalue* source=make(false); /* Prepared finalizer is NOT collector-ready. */
    assert(xl6ValueObjectFieldSetTakeV1(source,"a\0b",3,xrtValueInt(7)));
    assert(integer(xl6ValueObjectFieldFindV1(source,"a\0b",3))==7);
    assert(xl6ClassRuntimeV3Get()->CommitTake(source)==source);
    assert(xl6ValueObjectFieldSetTakeV1(source,"a",1,xrtValueInt(8)));
    assert(integer(xl6ValueObjectFieldFindV1(source,"a",1))==8);
    assert(integer(xl6ValueObjectFieldFindV1(source,"a\0b",3))==7);
    assert(xl6ValueObjectFieldSetTakeV1(source,"declared",8,xrtValueInt(4)));
    assert(integer(xrtValueObjectGet(source,XRT_STR_LITERAL("declared")))==4);
#if !defined(XL6_OBJECT_FIELDS_NO_INDEX)
    xvalue* key=xrtValueString(XRT_STR_LITERAL("a\0b")); assert(key);
    xvalue* owned=xl6ValueIndexReadCopyV1(source,key); assert(integer(owned)==7);
    assert(xl6ValueIndexSetTakeV1(source,key,xrtValueInt(9)));
    assert(integer(owned)==7 && integer(xl6ValueObjectFieldFindV1(source,"a\0b",3))==9);
    xrtValueRelease(owned); xrtValueRelease(key);
#endif
    assert(!xl6ValueObjectFieldFindV1(source,"missing",7) && !xrtGetError());
    assert(xl6ValueObjectFieldSetTakeV1(source,"null",4,xrtValueRetain(xrtValueNull())));
    assert(xl6ValueObjectFieldFindV1(source,"null",4)==xrtValueNull());
    static const xvaluehandleops ops={NULL,reenter_drop,NULL,NULL};
    void* raw=(void*)2; xvalue* child=xrtValueHandleTake(&raw,&ops,NULL);
    assert(child && xrtValueHandleOwnershipBindPhased(child,leaf));
    reenter_source=xrtValueRetain(source); assert(reenter_source);
    assert(xl6ValueObjectFieldSetTakeV1(source,"retired",7,child));
    assert(xl6ValueObjectFieldSetTakeV1(source,"retired",7,xrtValueInt(42)) && reentries==1);
    assert(xl6ValueObjectFieldSetTakeV1(source,"silent",6,silent_value()));
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
    assert(!xl6ValueObjectFieldSetTakeV1(source,"silent",6,xrtValueInt(43)));
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(!outcome.ok && !outcome.error && drops==1);
    assert(integer(xl6ValueObjectFieldFindV1(source,"silent",6))==43);
    xrtValueRelease(source); assert(finalizers==1 && !xrtGetError());
}
static void forged(void)
{
    xvalue* source=make(true);
    xvalue* storage=xrtValueObjectGet(source,(xstrview){storage_key,sizeof(storage_key)}); assert(storage);
    /* Different exact policy, invalid address. Rejection must precede payload
     * access. Its silent input Drop must not replace the primary TYPE error. */
    static const xvaluehandleops ops={NULL,forged_drop,NULL,NULL}; void* raw=(void*)1;
    assert(xrtValueObjectSetNew(storage,(xstrview){handle_key,sizeof(handle_key)},
        xrtValueHandleTake(&raw,&ops,NULL)));
    assert(!xl6ValueObjectFieldFindV1(source,"missing",7) && xrtErrorKind(xrtGetError())==XERR_TYPE);
    xrtClearError(); xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
    assert(!xl6ValueObjectFieldSetTakeV1(source,"missing",7,silent_value()));
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(!outcome.ok && outcome.error && xrtErrorKind(outcome.error)==XERR_TYPE && drops==2);
    xrtErrorFree(outcome.error); xrtClearError();
    assert(!xrtValueObjectGet(source,XRT_STR_LITERAL("missing")));
    xrtValueRelease(source); assert(!xrtGetError());
    XL6_ClassIdentitySchemaV3 malformed=schema; malformed.dynamicFields.handleSize=0;
    assert(!xl6ClassRuntimeV3Get()->Create(NULL,NULL,&malformed) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT);
    xrtClearError();
    XL6_ClassIdentitySchemaV3 custom=schema;
    custom.hashMode=custom.equalMode=XL6_CLASS_IDENTITY_NONE_V3;
    source=xl6ClassRuntimeV3Get()->Create(NULL,NULL,&custom); assert(source);
    assert(xrtValueTypeIdBind(source,custom.identity));
    XL6_ClassDynamicFieldPathV3 path={0};
    assert(xl6ClassDynamicFieldPathV3(source,&path)==1 && path.storageSize==sizeof(storage_key));
    assert(xl6ClassRuntimeV3Get()->CommitTake(source)==source);
    xrtValueRelease(source);
    source=xrtValueObject(); assert(source);
    assert(xl6ValueObjectFieldSetTakeV1(source,"plain\0key",9,xrtValueInt(44)));
    assert(integer(xl6ValueObjectFieldFindV1(source,"plain\0key",9))==44);
    xrtValueRelease(source);
}
static void prefixes(void)
{
    size_t prefix;
    for(prefix=0;prefix<1000;++prefix) {
        xl6ProductMemoryBegin(SIZE_MAX);
        xvalue* source=make(true); xvalue* item=xrtValueString(XRT_STR_LITERAL("owned")); assert(item);
        xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
        assert(xrtMemDebugFailAfter(prefix));
        bool success=xl6ValueObjectFieldSetTakeV1(source,"new\0key",7,item);
        bool hit=xl6ProductMemoryEnd(); xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
        assert(hit ? !success && !outcome.ok : success && outcome.ok);
        if(hit) assert(outcome.error && xrtErrorKind(outcome.error)==XERR_MEMORY);
        xrtErrorFree(outcome.error); xrtClearError(); xrtValueRelease(source);
        xl6ProductMemoryVerify();
        if(!hit) break;
    }
    assert(prefix<1000); printf("%zu checked object-field SetTake OOM prefixes balanced\n",prefix+1);
}
int main(void)
{
    assert(xrtMemDebugEnable(true)); xl6_error_scope warm={0}; assert(xl6ErrorScopeBegin(&warm));
    assert(xl6ErrorScopeEnd(&warm).ok);
    xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
    routing(); forged(); prefixes(); xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
        before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount);
    puts("resident object fields: prepared/exact-path/index/owned/forged/reentry/first-error balanced");
    return 0;
}
