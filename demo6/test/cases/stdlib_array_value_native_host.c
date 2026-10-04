#include "src/runtime/xl6_array_value.h"
#include "src/runtime/xl6_string_type.h"
#include "src/runtime/xl6_nullable_string.h"
#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEXT_ARGS UINT64_C(701),UINT64_C(801),2,sizeof(xstrview),xl6StringTypeV1()
#define OPTIONAL_TEXT_ARGS UINT64_C(702),UINT64_C(802),2,sizeof(xstrview),xl6NullableStringTypeV1()
#define REF_ARGS UINT64_C(703),UINT64_C(803),3,sizeof(xvalue*),xrtTypeValue()
#define I64_ARGS UINT64_C(704),UINT64_C(804),1,sizeof(int64_t),xrtTypeInt64()

static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount &&
        before->DoubleFreeCount==after.DoubleFreeCount && before->UseAfterFreeCount==after.UseAfterFreeCount);
    assert(!xrtGetError());
}
static void error(xerrkind kind)
{ assert(xrtErrorKind(xrtGetError())==kind); xrtClearError(); }
static void text(xstrview actual, const char* expected, size_t size)
{ assert(actual.Data && actual.Size==size && !memcmp(actual.Data,expected,size)); }
static xvalue* texts(void)
{
    xvalue* value=xrtValueArray(); assert(value);
    assert(xrtValueArrayAppendNew(value,xrtValueString((xstrview){"",0})));
    assert(xrtValueArrayAppendNew(value,xrtValueString((xstrview){"a\0b",3})));
    assert(xrtValueArrayAppendNew(value,xrtValueString((xstrview){"\xe4\xbd\xa0",3})));
    return value;
}
static void textVectors(void)
{
    const xrttype* type=xl6StringTypeV1();
    assert(xrtTypeValidate(type) && xl6StringTypeAbiV1(type));
    assert(type->Id==xrtTypeId((xstrview){XL6_STRING_ABI,sizeof(XL6_STRING_ABI)-1}));
    assert(!xl6NullableStringTypeAbiV1(type) && !xl6StringTypeAbiV1(xl6NullableStringTypeV1()));
    xrttype forged=*type; forged.Flags|=XRT_TYPE_FLAG_NULLABLE;
    assert(!xl6StringTypeAbiV1(&forged));
    xstrview initial={0}; assert(xrtTypeInitValue(type,&initial)); text(initial,"",0);
    xstrview duplicate={0}; assert(xrtTypeCopyValue(type,&duplicate,&initial));
    assert(duplicate.Data && duplicate.Data!=initial.Data);
    xstrview moved={0}; assert(xrtTypeMoveValue(type,&moved,&duplicate));
    text(moved,"",0); assert(!duplicate.Data && !duplicate.Size);
    xstrview absent={0}; assert(!xrtTypeCopyValue(type,&initial,&absent)); error(XERR_TYPE); text(initial,"",0);
    const xvalueconverter* converter=xl6StringConverterV1();
    assert(!converter->ToTyped(xrtValueNull(),type,&initial,NULL)); error(XERR_TYPE); text(initial,"",0);
    assert(!converter->FromTyped(type,&absent,NULL)); error(XERR_TYPE);
    assert(!converter->FromTyped(&forged,&initial,NULL)); error(XERR_TYPE);
    xrtTypeDropValue(type,&moved); xrtTypeDropValue(type,&initial);

    xvalue* value=xl6ArrayCreateV1(TEXT_ARGS); assert(value && xl6ArrayResizeV1(value,TEXT_ARGS,3));
    for (int i=0;i<3;i++) { xl6ArrayReadV1(value,TEXT_ARGS,i,&initial); text(initial,"",0); xrtTypeDropValue(type,&initial); }
    xvalue* snapshot=xl6ArrayToValueV1(value,TEXT_ARGS,converter); assert(snapshot && xrtValueCount(snapshot)==3);
    for (int i=0;i<3;i++) { xstrview view; assert(xrtValueGetString(xrtValueArrayAt(snapshot,i),&view)); text(view,"",0); }
    xstrview input={"a\0b",3}; assert(xl6ArraySetV1(value,TEXT_ARGS,1,&input));
    xl6ArrayTakeV1(value,TEXT_ARGS,1,&initial); text(initial,"a\0b",3); xrtTypeDropValue(type,&initial);
    xrtValueRelease(snapshot); xrtValueRelease(value);

    xvalue* source=texts(); value=xl6ArrayFromValueV1(source,TEXT_ARGS,converter); assert(value);
    snapshot=xl6ArrayToValueV1(value,TEXT_ARGS,converter); assert(snapshot && xrtValueEqual(source,snapshot));
    xstrview replacement={"other",5}; assert(xl6ArraySetV1(value,TEXT_ARGS,1,&replacement));
    assert(xrtValueEqual(source,snapshot));
    xrtValueRelease(value); xrtValueRelease(source); xrtValueRelease(snapshot);

    source=texts();
    assert(!xl6ArrayFromValueV1(source,TEXT_ARGS,NULL)); error(XERR_UNSUPPORTED);
    xvalueconverter incomplete=*converter; incomplete.FromTyped=NULL;
    assert(!xl6ArrayFromValueV1(source,TEXT_ARGS,&incomplete)); error(XERR_ARGUMENT);
    assert(!xl6ArrayFromValueV1(xrtValueNull(),TEXT_ARGS,converter)); error(XERR_TYPE);
    assert(!xl6ArrayToValueV1(source,TEXT_ARGS,converter)); error(XERR_ARGUMENT);
    xrtValueRelease(source);

    source=xrtValueArray(); assert(source && xrtValueArrayAppendNew(source,xrtValueNull()));
    assert(!xl6ArrayFromValueV1(source,TEXT_ARGS,converter)); error(XERR_TYPE);
    value=xl6ArrayFromValueV1(source,OPTIONAL_TEXT_ARGS,xl6NullableStringConverterV1()); assert(value);
    snapshot=xl6ArrayToValueV1(value,OPTIONAL_TEXT_ARGS,xl6NullableStringConverterV1());
    assert(snapshot && xrtValueType(xrtValueArrayAt(snapshot,0))==XVALUE_NULL);
    xrtValueRelease(value); xrtValueRelease(source); xrtValueRelease(snapshot);
}

typedef struct Profile { const char* name; const xrttype* type; const xvalueconverter* converter; } Profile;
static Profile profiles[7];
static void profileInit(void)
{
    profiles[0]=(Profile){"bool",xl6NullableBoolTypeV1(),xl6NullableBoolConverterV1()};
    profiles[1]=(Profile){"i64",xl6NullableI64TypeV1(),xl6NullableI64ConverterV1()};
    profiles[2]=(Profile){"u64",xl6NullableU64TypeV1(),xl6NullableU64ConverterV1()};
    profiles[3]=(Profile){"f32",xl6NullableF32TypeV1(),xl6NullableF32ConverterV1()};
    profiles[4]=(Profile){"f64",xl6NullableF64TypeV1(),xl6NullableF64ConverterV1()};
    profiles[5]=(Profile){"time",xl6NullableTimeTypeV1(),xl6NullableTimeConverterV1()};
    profiles[6]=(Profile){"char",xl6NullableCharTypeV1(),xl6NullableCharConverterV1()};
}
#define PROFILE_ARGS UINT64_C(705),UINT64_C(805),1,profile->type->Size,profile->type
static xvalue* profileSource(size_t index)
{
    xvalue* value=xrtValueArray(); assert(value && xrtValueArrayAppendNew(value,xrtValueNull()));
    xvalue* zero=NULL,*edge=NULL;
    switch(index) {
    case 0: zero=xrtValueBool(false); edge=xrtValueBool(true); break;
    case 1: zero=xrtValueInt(INT64_MIN); edge=xrtValueInt(INT64_MAX); break;
    case 2: zero=xrtValueUInt(0); edge=xrtValueUInt(UINT64_MAX); break;
    case 3: case 4: zero=xrtValueFloat(-0.0); edge=xrtValueFloat(INFINITY); break;
    case 5: zero=xrtValueTime(INT64_MIN); edge=xrtValueTime(INT64_MAX); break;
    case 6: zero=xrtValueChar(0); edge=xrtValueChar(0x10ffff); break;
    default: abort();
    }
    assert(zero && edge && xrtValueArrayAppendNew(value,zero) && xrtValueArrayAppendNew(value,edge));
    return value;
}
static void scalarVectors(void)
{
    for(size_t i=0;i<7;i++) {
        const Profile* profile=&profiles[i]; xvalue* source=profileSource(i);
        xvalue* value=xl6ArrayFromValueV1(source,PROFILE_ARGS,profile->converter); assert(value);
        xvalue* snapshot=xl6ArrayToValueV1(value,PROFILE_ARGS,profile->converter);
        assert(snapshot && xrtValueEqual(source,snapshot));
        assert(xrtValueType(xrtValueArrayAt(snapshot,0))==XVALUE_NULL);
        assert(xrtValueType(xrtValueArrayAt(source,1))==xrtValueType(xrtValueArrayAt(snapshot,1)));
        if(i==3 || i==4) { double number; assert(xrtValueGetFloat(xrtValueArrayAt(snapshot,1),&number) && signbit(number)); }
        xrtValueRelease(value); xrtValueRelease(snapshot); xrtValueRelease(source);
    }
    /* Canonical builtins intentionally need no custom converter. */
    xvalue* source=xrtValueArray(); assert(source && xrtValueArrayAppendNew(source,xrtValueInt(INT64_MIN)));
    xvalue* value=xl6ArrayFromValueV1(source,I64_ARGS,NULL); assert(value);
    xvalue* snapshot=xl6ArrayToValueV1(value,I64_ARGS,NULL); assert(snapshot && xrtValueEqual(source,snapshot));
    xrtValueRelease(value); xrtValueRelease(snapshot); xrtValueRelease(source);
    const Profile* profile=&profiles[6]; source=xrtValueArray();
    assert(source && xrtValueArrayAppendNew(source,xrtValueUInt(65)));
    assert(!xl6ArrayFromValueV1(source,PROFILE_ARGS,profile->converter)); error(XERR_TYPE); xrtValueRelease(source);
    source=xrtValueArray(); assert(source && xrtValueArrayAppendNew(source,xrtValueUInt(UINT64_MAX)));
    assert(!xl6ArrayFromValueV1(source,I64_ARGS,NULL)); error(XERR_RANGE); xrtValueRelease(source);
    profile=&profiles[3]; source=xrtValueArray(); assert(source && xrtValueArrayAppendNew(source,xrtValueFloat(0.1)));
    assert(!xl6ArrayFromValueV1(source,PROFILE_ARGS,profile->converter)); error(XERR_RANGE); xrtValueRelease(source);
}

static void referenceVectors(void)
{
    xvalue* item=xrtValueArray(); assert(item && xrtValueArrayAppendNew(item,xrtValueInt(19)));
    assert(xrtValueTypeIdBind(item,901));
    xvalue* source=xrtValueArray(); assert(source && xrtValueArrayAppend(source,item));
    assert(xrtValueArrayAppendNew(source,xrtValueNull()));
    xvalue* value=xl6ArrayFromValueV1(source,REF_ARGS,NULL); assert(value);
    xvalue* output=NULL; xl6ArrayReadV1(value,REF_ARGS,0,&output); assert(output==item); xrtValueRelease(output);
    xvalue* snapshot=xl6ArrayToValueV1(value,REF_ARGS,NULL); assert(snapshot);
    assert(xrtValueArrayAt(snapshot,0)==item && xrtValueTypeId(xrtValueArrayAt(snapshot,0))==901);
    assert(xrtValueArrayAppendNew(item,xrtValueInt(20)) && xrtValueCount(xrtValueArrayAt(snapshot,0))==2);
    xl6ArrayClearV1(value,REF_ARGS); assert(xrtValueCount(snapshot)==2 && xrtValueCount(source)==2);
    assert(!xl6ArrayFromValueV1(source,REF_ARGS,xl6StringConverterV1())); error(XERR_ARGUMENT);
    assert(!xl6ArrayToValueV1(value,REF_ARGS,xl6StringConverterV1())); error(XERR_ARGUMENT);
    output=NULL; assert(xl6ArrayPushV1(value,REF_ARGS,&output));
    xvalue* absent=xl6ArrayToValueV1(value,REF_ARGS,NULL); assert(absent && xrtValueType(xrtValueArrayAt(absent,0))==XVALUE_NULL);
    xrtValueRelease(absent); xrtValueRelease(value); xrtValueRelease(snapshot); xrtValueRelease(source); xrtValueRelease(item);
}

static xvalue* reentrantOwner;
static bool callback(bool releaseOwner)
{
    xstrview item={"blocked",7};
    assert(!xl6ArrayPushV1(reentrantOwner,TEXT_ARGS,&item)); error(XERR_STATE);
    if(releaseOwner) { xrtValueRelease(reentrantOwner); reentrantOwner=NULL; }
    return true;
}
static xvalue* reentrantFrom(const xrttype* type, const void* source, ptr context)
{
    (void)context;
    if(reentrantOwner) callback(true);
    return xl6StringConverterV1()->FromTyped(type,source,NULL);
}
static bool mutateTo(const xvalue* source, const xrttype* type, ptr target, ptr context)
{
    xvalue* original=context;
    assert(xrtValueArrayAppendNew(original,xrtValueString((xstrview){"extra",5})));
    return xl6StringConverterV1()->ToTyped(source,type,target,NULL);
}
static xvalue* failingFrom(const xrttype* type, const void* source, ptr context)
{ (void)type; (void)source; (void)context; xrtSetErrorKind(XERR_IO); return NULL; }
static void callbackVectors(void)
{
    xvalue* source=texts(); reentrantOwner=xl6ArrayFromValueV1(source,TEXT_ARGS,xl6StringConverterV1());
    assert(reentrantOwner); xvalue* weak=xrtValueWeakRef(reentrantOwner); assert(weak);
    xvalueconverter converter=*xl6StringConverterV1(); converter.FromTyped=reentrantFrom;
    xvalue* snapshot=xl6ArrayToValueV1(reentrantOwner,TEXT_ARGS,&converter);
    assert(snapshot && !reentrantOwner && xrtValueWeakRefExpired(weak) && xrtValueEqual(source,snapshot));
    xrtValueRelease(weak); xrtValueRelease(snapshot);
    converter=*xl6StringConverterV1(); converter.ToTyped=mutateTo; converter.Context=source;
    xvalue* value=xl6ArrayFromValueV1(source,TEXT_ARGS,&converter);
    assert(value && xl6ArraySizeV1(value,TEXT_ARGS,false)==3 && xrtValueCount(source)==6);
    converter=*xl6StringConverterV1(); converter.FromTyped=failingFrom;
    assert(!xl6ArrayToValueV1(value,TEXT_ARGS,&converter)); error(XERR_IO);
    assert(xl6ArraySizeV1(value,TEXT_ARGS,false)==3); xrtValueRelease(value); xrtValueRelease(source);
}

static const xrtownershipadapterv1* resolve(xrtownershipref ref, void* context)
{
    (void)context; const xrtownershipadapterv1* adapter=xl6SequenceOwnershipAdapterV1(ref);
    return adapter ? adapter : xrtValueOwnershipAdapterV1(ref);
}
static xvalue* cycle(xvalue** weak)
{
    xvalue* value=xl6ArrayCreateV1(REF_ARGS); assert(value);
    *weak=xrtValueWeakRef(value); assert(*weak);
    assert(xl6ArrayPushV1(value,REF_ARGS,&value));
    xvalue* snapshot=xl6ArrayToValueV1(value,REF_ARGS,NULL); assert(snapshot);
    xvalue* imported=xl6ArrayFromValueV1(snapshot,REF_ARGS,NULL); assert(imported);
    xvalue* item=NULL; xl6ArrayReadV1(imported,REF_ARGS,0,&item); assert(item==value); xrtValueRelease(item);
    assert(xl6ArrayPushV1(value,REF_ARGS,&snapshot) && xl6ArrayPushV1(value,REF_ARGS,&imported));
    xrtValueRelease(snapshot); xrtValueRelease(imported); xrtValueRelease(value);
    return value; /* Real closed owning cycle, not a borrowed dangling shell. */
}
static void graphFault(const char* operation, size_t point, bool inject)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* weak=NULL; xvalue* value=cycle(&weak); XL6_OwnershipPlan* plan=NULL;
    bool create=!strcmp(operation,"graph-create"), commit=!strcmp(operation,"graph-commit");
    assert(create || commit || !strcmp(operation,"graph-refresh"));
    if(!create) assert(xl6OwnershipPlanTryCreate(xrtValueOwnership(value),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_READY);
    if(inject) assert(xrtMemDebugFailAfter(point));
    XL6_OwnershipPlanResult outcome=create ? xl6OwnershipPlanTryCreate(xrtValueOwnership(value),resolve,NULL,&plan) :
        commit ? xl6OwnershipPlanTryCommit(plan) : xl6OwnershipPlanTryRefresh(plan);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    assert(hit ? outcome==XL6_OWNERSHIP_PLAN_ERROR : outcome==XL6_OWNERSHIP_PLAN_READY);
    if(hit) {
        error(XERR_MEMORY); assert(xl6ArraySizeV1(value,REF_ARGS,false)==3);
        if(plan) assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
        plan=NULL; assert(xl6OwnershipPlanTryCreate(xrtValueOwnership(value),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_READY);
    }
    if(!commit || hit) assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanFinish(plan) && xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xrtValueWeakRefExpired(weak)); xrtValueRelease(weak); balanced(&before);
    printf("array-value %s point=%zu hit=%d balanced\n",operation,point,hit);
}

static void fault(const char* operation, size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* source=NULL,*value=NULL,*result=NULL; xstrview output={0};
    const xrttype* type=xl6StringTypeV1(); const Profile* profile=NULL;
    bool from=strstr(operation,"-from")!=NULL, to=strstr(operation,"-to")!=NULL;
    bool reference=!strncmp(operation,"ref-",4);
    if(!strncmp(operation,"profile-",8)) {
        for(size_t i=0;i<7;i++) {
            if(!strncmp(operation+8,profiles[i].name,strlen(profiles[i].name)) && operation[8+strlen(profiles[i].name)]=='-') {
                profile=&profiles[i]; source=profileSource(i); break;
            }
        }
        assert(profile && (from || to));
    } else if(reference) {
        source=xrtValueArray(); xvalue* item=xrtValueArray();
        assert(source && item && xrtValueArrayAppendNew(item,xrtValueInt(19)));
        assert(xrtValueArrayAppendNew(source,item) && xrtValueArrayAppendNew(source,xrtValueNull()));
    } else source=texts();
    if(to) {
        value=profile ? xl6ArrayFromValueV1(source,PROFILE_ARGS,profile->converter) : reference ?
            xl6ArrayFromValueV1(source,REF_ARGS,NULL) : xl6ArrayFromValueV1(source,TEXT_ARGS,xl6StringConverterV1());
        assert(value);
    } else if(!strcmp(operation,"string-resize")) {
        value=xl6ArrayCreateV1(TEXT_ARGS); assert(value && xl6ArrayResizeV1(value,TEXT_ARGS,1));
    }
    if(!strcmp(operation,"string-copy")) assert(xrtTypeInitValue(type,&output));
    assert(xrtMemDebugFailAfter(point)); bool ok=false;
    if(!strcmp(operation,"string-init")) ok=xrtTypeInitValue(type,&output);
    else if(!strcmp(operation,"string-copy")) { xstrview input={"a\0b",3}; ok=xrtTypeCopyValue(type,&output,&input); }
    else if(!strcmp(operation,"string-resize")) ok=xl6ArrayResizeV1(value,TEXT_ARGS,4);
    else if(from) {
        result=profile ? xl6ArrayFromValueV1(source,PROFILE_ARGS,profile->converter) : reference ?
            xl6ArrayFromValueV1(source,REF_ARGS,NULL) : xl6ArrayFromValueV1(source,TEXT_ARGS,xl6StringConverterV1());
        ok=result!=NULL;
    } else if(to) {
        result=profile ? xl6ArrayToValueV1(value,PROFILE_ARGS,profile->converter) : reference ?
            xl6ArrayToValueV1(value,REF_ARGS,NULL) : xl6ArrayToValueV1(value,TEXT_ARGS,xl6StringConverterV1());
        ok=result!=NULL;
    } else abort();
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    assert(hit ? !ok && xrtErrorKind(xrtGetError())==XERR_MEMORY : ok && !xrtGetError());
    xerror* failure=xrtTakeError();
    assert(xrtValueCount(source)==(reference ? 2u : 3u));
    if(!strcmp(operation,"string-copy")) text(output,hit ? "" : "a\0b",hit ? 0u : 3u);
    if(!strcmp(operation,"string-resize")) {
        assert(xl6ArraySizeV1(value,TEXT_ARGS,false)==(hit ? 1 : 4));
        xstrview check={0}; xl6ArrayReadV1(value,TEXT_ARGS,0,&check); text(check,"",0); xrtTypeDropValue(type,&check);
    }
    if(to) {
        assert((profile ? xl6ArraySizeV1(value,PROFILE_ARGS,false) : reference ?
            xl6ArraySizeV1(value,REF_ARGS,false) : xl6ArraySizeV1(value,TEXT_ARGS,false))==(reference ? 2 : 3));
        if(ok) assert(xrtValueEqual(result,source));
    }
    xrtSetErrorTake(failure); const xerror* saved=xrtGetError();
    xrtTypeDropValue(type,&output); xrtValueRelease(result); xrtValueRelease(value); xrtValueRelease(source);
    assert(xrtGetError()==saved); xrtClearError(); balanced(&before);
    printf("array-value %s point=%zu hit=%d balanced\n",operation,point,hit);
}
int main(int argc, char** argv)
{
    assert(xrtMemDebugEnable(true)); profileInit();
    if(argc==3) {
        size_t point=(size_t)strtoull(argv[2],NULL,10);
        if(!strncmp(argv[1],"graph-",6)) graphFault(argv[1],point,true); else fault(argv[1],point);
        return 0;
    }
    assert(argc==1); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    textVectors(); scalarVectors(); referenceVectors(); callbackVectors(); graphFault("graph-create",0,false); balanced(&before);
    puts("array-value native vectors balanced"); return 0;
}
