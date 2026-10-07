#define _GNU_SOURCE 1
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_THREAD
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_code_lease.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
typedef HMODULE Library;
#else
#include <dlfcn.h>
typedef void* Library;
#endif
typedef struct Owner {
    XL6_CodeOwnerGraph capability;
    Library library;
    const XL6_ClassIdentitySchemaV3* schema;
    unsigned pins,acquires,drops,unmapped;
    bool closed;
} Owner;
static const XL6_ClassRuntimeV3* api;
static unsigned finalizers;
static bool pending_finalizer(xvalue* value,void* context)
{ (void)value; assert(!context); ++finalizers; return true; }
static int acquire(void* data)
{ Owner* owner=data; if (owner->closed) return false; ++owner->pins; ++owner->acquires; return true; }
static void close_library(Owner* owner)
{
    assert(owner->closed && !owner->pins && !owner->unmapped);
    const void* address=owner->schema;
#if defined(_WIN32)
    MEMORY_BASIC_INFORMATION info;
    assert(FreeLibrary(owner->library));
    assert(VirtualQuery(address,&info,sizeof(info))==sizeof(info) && info.State==MEM_FREE);
#else
    Dl_info info;
    assert(dlclose(owner->library)==0 && !dladdr(address,&info));
#endif
    owner->library=NULL; owner->schema=NULL; ++owner->unmapped;
}
static void release(void* data)
{ Owner* owner=data; assert(owner->pins); --owner->pins; ++owner->drops; if (owner->closed && !owner->pins) close_library(owner); }
static bool owner_count(const void* data,size_t* count)
{ const Owner* owner=data; if (owner->unmapped) return false; *count=owner->pins+(owner->closed ? 0 : 1); return *count!=0; }
static bool empty(const void* data,xrtownershipvisitor visit,void* context)
{ (void)data; (void)visit; (void)context; return true; }
static const xrtownershipops owner_ops={owner_count,empty};
static void load(Owner* owner,const char* path)
{
    const XL6_ClassIdentitySchemaV3* (*get)(void);
#if defined(_WIN32)
    owner->library=LoadLibraryA(path); assert(owner->library);
    FARPROC symbol=GetProcAddress(owner->library,"class_identity_schema_get"); assert(symbol); memcpy(&get,&symbol,sizeof(get));
#else
    owner->library=dlopen(path,RTLD_NOW|RTLD_LOCAL); assert(owner->library);
    void* symbol=dlsym(owner->library,"class_identity_schema_get"); assert(symbol); memcpy(&get,&symbol,sizeof(get));
#endif
    owner->schema=get(); assert(owner->schema);
    owner->capability=(XL6_CodeOwnerGraph){{sizeof(XL6_CodeOwnerGraph),owner,acquire,release},UINT64_C(0x584C36434F574E31),&owner_ops};
}
static void close_owner(Owner* owner)
{ owner->closed=true; if (!owner->pins) close_library(owner); }
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtClearError(); xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static const char binary_key[]={'b',0,'s'};
static xvalue* make_schema(const XL6_CodeOwner* owner,const XL6_ClassIdentitySchemaV3* schema)
{
    xvalue* value=api->Create(owner,NULL,schema); assert(value);
    assert(xrtValueTypeIdBind(value,schema->identity) &&
        xrtValueObjectSetNew(value,XRT_STR_LITERAL("$xlir.class.runtime-type"),xrtValueInt(schema->runtimeTypeTag)) &&
        xrtValueObjectSetNew(value,(xstrview){binary_key,sizeof(binary_key)},xrtValueString((xstrview){"a\0你",5})) &&
        xrtValueObjectSetNew(value,XRT_STR_LITERAL("own"),xrtValueInt(42)) &&
        xrtValueObjectSetNew(value,XRT_STR_LITERAL("skip"),xrtValueInt(777)));
    return value;
}
static xvalue* make(Owner* owner)
{
    xvalue* value=make_schema(&owner->capability.base,owner->schema);
    assert(api->BindIdentityTake(value)==value && api->CommitTake(value)==value); return value;
}
static bool anchor(xrtownershipref ref,void* data)
{ assert(ref.Data && ref.Ops && xl6ClassOwnershipAdapterV1(ref)); ++*(unsigned*)data; return true; }
static void lifetimes(const char* first,const char* second)
{
    for (unsigned round=0;round<16;++round) {
        xmemdebugsnapshot before; xrtMemDebugSnapshot(&before); Owner a={0},b={0}; load(&a,first); load(&b,second);
        xvalue *left=make(&a),*right=make(&b); bool equal=false; uint64 h1=0,h2=1;
        assert(a.schema!=b.schema && a.schema->runtimeTypeTag!=b.schema->runtimeTypeTag);
        assert(api->Equal(left,right,&equal) && equal && xrtValueEqual(left,right) &&
            xrtValueHash(left,&h1) && xrtValueHash(right,&h2) && h1==h2);
        assert(xrtValueObjectSetNew(right,XRT_STR_LITERAL("skip"),xrtValueInt(123)) && api->Equal(left,right,&equal) && equal);
        xvalue *cow=xrtValueClone(left),*deep=xrtValueDeepClone(left); assert(cow && deep);
        assert(xrtValueObjectSetNew(cow,XRT_STR_LITERAL("skip"),xrtValueInt(456)));
        xrtownershipscope freeze={0}; unsigned anchors=0;
        assert(xrtOwnershipFreezeTryBegin(&freeze) && xl6ClassOwnershipDiscoverV1(&a.capability.base,anchor,&anchors) &&
            anchors==3 && xrtOwnershipScopeEnd(&freeze));
        close_owner(&a); close_owner(&b); assert(a.pins==1 && b.pins==1 && !a.unmapped && !b.unmapped);
        xrtValueRelease(left);
        assert(api->Equal(cow,right,&equal) && equal && xrtValueHash(deep,&h1) && h1==h2);
        xvalueiter cursor={0}; assert(xrtValueIterBegin(deep,&cursor)); xrtValueRelease(deep); xrtValueRelease(cow);
        assert(!a.unmapped && a.pins==1); xrtValueIterEnd(&cursor); xrtValueRelease(right);
        assert(a.unmapped==1 && b.unmapped==1 && a.acquires==1 && b.acquires==1 && a.drops==1 && b.drops==1);
        balanced(&before);
    }
}
static const XL6_ClassIdentityFieldV3 local_fields[]={ {binary_key,sizeof(binary_key),0x21},{"own",3,0x22} };
static const XL6_ClassIdentitySchemaV3 local_schema={sizeof(local_schema),UINT64_C(0x434c415353),17,
    XL6_CLASS_IDENTITY_DERIVED_V3,XL6_CLASS_IDENTITY_DERIVED_V3,local_fields,2};
static unsigned hash_calls,equal_calls,drops;
static xvalue* receiver;
static uint64 evil_hash(void* data,void* user)
{
    (void)data; (void)user; ++hash_calls;
    assert(!xrtValueObjectRemove(receiver,XRT_STR_LITERAL("own"))); return 123;
}
static bool evil_equal(void* left,void* right,void* user)
{
    (void)left; (void)right; (void)user; ++equal_calls;
    xrtValueRelease(receiver); return true;
}
static void drop(void* data,void* user) { (void)data; (void)user; ++drops; }
static const xvaluehandleops evil={NULL,drop,evil_hash,evil_equal};
static xvalue* handle(void* address) { void* raw=address; xvalue* value=xrtValueHandleTake(&raw,&evil,NULL); assert(value && !raw); return value; }
static void invalid_schemas(void)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    for (unsigned mode=0;mode<9;++mode) {
        XL6_ClassIdentitySchemaV3 schema=local_schema; XL6_ClassIdentityFieldV3 field=local_fields[0];
        switch (mode) {
        case 0: schema.size=0; break;
        case 1: schema.identity=0; break;
        case 2: schema.runtimeTypeTag=0; break;
        case 3: schema.hashMode=(XL6_ClassIdentityModeV3)-1; break;
        case 4: schema.equalMode=(XL6_ClassIdentityModeV3)3; break;
        case 5: schema.fields=NULL; break;
        case 6: schema.fieldCount=SIZE_MAX; break;
        case 7: field.name=NULL; schema.fields=&field; schema.fieldCount=1; break;
        case 8: field.identity=0; schema.fields=&field; schema.fieldCount=1; break;
        }
        assert(xrtMemDebugFailAfter(0) && !api->Create(NULL,NULL,&schema) &&
            !xrtMemDebugFailTriggered() && xrtErrorKind(xrtGetError())==XERR_ARGUMENT);
        xrtMemDebugFailClear(); xrtClearError();
    }
    assert(!api->Create(NULL,NULL,NULL) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT); xrtClearError();
    xvalue* value=api->Create(NULL,NULL,&local_schema); assert(value);
    assert(!api->BindIdentityTake(value) && xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError();
    value=make_schema(NULL,&local_schema);
    assert(xrtValueObjectSetNew(value,XRT_STR_LITERAL("$xlir.class.runtime-type"),xrtValueInt(99)));
    assert(api->BindIdentityTake(value)==value); uint64 output=567;
    assert(!xrtValueHash(value,&output) && output==567 && xrtErrorKind(xrtGetError())==XERR_STATE); xrtClearError(); xrtValueRelease(value);
    value=api->Create(NULL,NULL,&local_schema); assert(value && xrtValueTypeIdBind(value,local_schema.identity));
    assert(xrtMemDebugFailAfter(0) && api->BindIdentityTake(value)==value && !xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    /* Real IR birth order: metadata binds before the runtime-tag/field slots. */
    assert(!xrtValueHash(value,&output) && output==567 && xrtErrorKind(xrtGetError())==XERR_STATE); xrtClearError();
    assert(xrtValueObjectSetNew(value,XRT_STR_LITERAL("$xlir.class.runtime-type"),xrtValueInt(17)) &&
        xrtValueObjectSetNew(value,(xstrview){binary_key,sizeof(binary_key)},xrtValueString((xstrview){"a\0你",5})) &&
        xrtValueObjectSetNew(value,XRT_STR_LITERAL("own"),xrtValueInt(42)) && api->CommitTake(value)==value && xrtValueHash(value,&output));
    xrtValueRelease(value);
    value=xl6ClassRuntimeV2Get()->Create(NULL,NULL); assert(value);
    assert(!api->BindIdentityTake(value) && xrtErrorKind(xrtGetError())==XERR_STATE); xrtClearError();
    value=make_schema(NULL,&local_schema);
    assert(!api->Equal(value,value,NULL) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT); xrtClearError();
    bool equal=true;
    assert(!api->Equal(value,NULL,&equal) && equal && xrtErrorKind(xrtGetError())==XERR_ARGUMENT); xrtClearError();
    XL6_ClassIdentitySchemaV3 separate=local_schema; XL6_ClassIdentityFieldV3 fields[2]={local_fields[0],local_fields[1]};
    fields[1].identity=0x23; separate.fields=fields;
    xvalue* other=make_schema(NULL,&separate);
    assert(api->Equal(value,other,&equal) && !equal);
    xrtValueRelease(other); xrtValueRelease(value); balanced(&before);
}
static void failures(void)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue *left=make_schema(NULL,&local_schema),*right=make_schema(NULL,&local_schema);
    assert(api->BindIdentityTake(left)==left && api->BindIdentityTake(right)==right && api->CommitTake(left)==left && api->CommitTake(right)==right);
    bool result=true; uint64 hash=UINT64_C(0xabcdef);
    assert(xrtValueObjectRemove(left,XRT_STR_LITERAL("own")));
    xrtSetErrorKind(XERR_ARGUMENT);
    assert(!api->Equal(left,right,&result) && result && xrtErrorKind(xrtGetError())==XERR_STATE); xrtClearError();
    assert(!xrtValueHash(left,&hash) && hash==UINT64_C(0xabcdef) && xrtErrorKind(xrtGetError())==XERR_STATE); xrtClearError();
    assert(xrtValueObjectSetNew(left,XRT_STR_LITERAL("own"),xrtValueInt(42)));
    xrtSetErrorKind(XERR_ARGUMENT); const xerror* prior=xrtGetError();
    assert(api->Equal(left,right,&result) && result && xrtGetError()==prior); xrtClearError();
    assert(xrtMemDebugFailAfter(0) && api->Equal(left,right,&result) && result && !xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    receiver=left;
    assert(xrtValueObjectSetNew(left,XRT_STR_LITERAL("own"),handle((void*)(uintptr_t)1)) &&
        xrtValueObjectSetNew(right,XRT_STR_LITERAL("own"),handle((void*)(uintptr_t)2)));
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
    result=false; assert(!api->Equal(left,right,&result) && !result && equal_calls==1 && xrtErrorKind(xrtGetError())==XERR_STATE);
    xl6ErrorScopeCaptureFailure(&scope); xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(!outcome.ok && xrtErrorKind(outcome.error)==XERR_STATE); xrtErrorFree(outcome.error);
    assert(xl6ErrorScopeBegin(&scope) && !xrtValueHash(left,&hash) && hash==UINT64_C(0xabcdef) && hash_calls==1);
    outcome=xl6ErrorScopeEnd(&scope); assert(!outcome.ok && xrtErrorKind(outcome.error)==XERR_STATE); xrtErrorFree(outcome.error);
    assert(xrtValueCount(left)==4 && xrtValueCount(right)==4);
    xrtValueRelease(left); xrtValueRelease(right); assert(drops==2); receiver=NULL; balanced(&before);
    XL6_ClassIdentitySchemaV3 reference=local_schema; reference.hashMode=reference.equalMode=XL6_CLASS_IDENTITY_REFERENCE_V3;
    left=make_schema(NULL,&reference); right=make_schema(NULL,&reference);
    assert(api->BindIdentityTake(left)==left && api->BindIdentityTake(right)==right && api->CommitTake(left)==left && api->CommitTake(right)==right);
    assert(api->Equal(left,right,&result) && !result && api->Equal(left,left,&result) && result);
    xrtValueRelease(left); xrtValueRelease(right);
    XL6_ClassIdentitySchemaV3 equality_only=local_schema; equality_only.hashMode=XL6_CLASS_IDENTITY_NONE_V3;
    left=make_schema(NULL,&equality_only); right=make_schema(NULL,&equality_only);
    assert(api->Equal(left,right,&result) && result);
    assert(!api->BindIdentityTake(left) && xrtErrorKind(xrtGetError())==XERR_UNSUPPORTED); xrtClearError();
    xrtValueRelease(right); balanced(&before);
}
static void factory_oom(const char* path)
{
    for (unsigned finalize=0;finalize<2;++finalize) {
        bool terminal=false;
        for (size_t point=0;point<64;++point) {
            xmemdebugsnapshot before; xrtMemDebugSnapshot(&before); Owner owner={0}; load(&owner,path);
            assert(xrtMemDebugFailAfter(point));
            unsigned calls=finalizers;
            xvalue* value=api->Create(&owner.capability.base,finalize ? pending_finalizer : NULL,owner.schema);
            bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
            assert(hit ? !value && xrtErrorKind(xrtGetError())==XERR_MEMORY : value!=NULL);
            xrtClearError(); close_owner(&owner); xrtValueRelease(value);
            assert(owner.unmapped==1 && !owner.pins && owner.acquires==owner.drops && finalizers==calls); balanced(&before);
            if (!hit) { terminal=true; break; }
        }
        assert(terminal);
    }
}
static int32 worker(void* data)
{
    size_t rounds=(size_t)(uintptr_t)data;
    for (size_t i=0;i<rounds;++i) {
        xvalue *left=make_schema(NULL,&local_schema),*right=make_schema(NULL,&local_schema); uint64 hash=0; bool equal=false;
        assert(api->BindIdentityTake(left)==left && api->BindIdentityTake(right)==right &&
            api->CommitTake(left)==left && api->CommitTake(right)==right && xrtValueHash(left,&hash) &&
            api->Equal(left,right,&equal) && equal);
        xrtValueRelease(left); xrtValueRelease(right); assert(!xrtGetError());
    }
    return 0;
}
static void concurrent(void)
{
    /* The production xlang snapshot's relative waits are MICROSECONDS. */
    const uint64 tenSeconds=UINT64_C(10000000);
    xthread* warm=xrtThreadCreate(worker,NULL,0); assert(warm && xrtThreadWaitFor(warm,tenSeconds)==XWAIT_OK); xrtThreadDestroy(warm);
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before); xthread* threads[4];
    for (unsigned i=0;i<4;++i) { threads[i]=xrtThreadCreate(worker,(void*)(uintptr_t)100,0); assert(threads[i]); }
    for (unsigned i=0;i<4;++i) {
        xwaitresult wait=xrtThreadWaitFor(threads[i],tenSeconds);
        if (wait!=XWAIT_OK) fprintf(stderr,"class worker join=%d error=%d system=%d thread=%u\n",wait,
            xrtErrorKind(xrtGetError()),xrtErrorSystemCode(xrtGetError()),i);
        assert(wait==XWAIT_OK); xrtThreadDestroy(threads[i]);
    }
    balanced(&before);
}
static void depth_limit(void)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before); xvalue* chain=NULL;
    for (unsigned i=0;i<XL6_CLASS_IDENTITY_DEPTH_MAX_V3+1;++i) {
        xvalue* value=make_schema(NULL,&local_schema);
        if (chain) assert(xrtValueObjectSetNew(value,XRT_STR_LITERAL("own"),chain));
        assert(api->BindIdentityTake(value)==value && api->CommitTake(value)==value); chain=value;
    }
    uint64 output=789;
    assert(!xrtValueHash(chain,&output) && output==789 && xrtErrorKind(xrtGetError())==XERR_RANGE); xrtClearError();
    xrtValueRelease(chain);
    xvalue *a=make_schema(NULL,&local_schema),*b=make_schema(NULL,&local_schema); bool equal=false;
    assert(api->Equal(a,b,&equal) && equal); xrtValueRelease(a); xrtValueRelease(b); balanced(&before);
}
int main(int argc,char** argv)
{
    assert(argc==3 && xrtMemDebugEnable(true)); api=xl6ClassRuntimeV3Get();
    lifetimes(argv[1],argv[2]); invalid_schemas(); failures(); factory_oom(argv[1]); depth_limit(); concurrent();
    puts("Resident class identity V3: 32 real module lifetimes/unmaps, cross-module schemas/local tags, COW/deep/cursor discovery, exact NUL/UTF8 fields, checked failures/reentry/output/first-error, full factory OOM prefix, bounded recursion, 400 independent-thread rounds and allocator balance passed");
    assert(xrtMemDebugReset()); return 0;
}
