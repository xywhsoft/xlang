#include "src/runtime/xl6_list_import_chain.h"
#include "src/runtime/xl6_list_schema.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_array_input.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <float.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX=65 };
#define OWNER(i) (UINT64_C(0xF100)+(i))
#define DOMAIN(i) (UINT64_C(0xF200)+(i))
#define LEAF UINT64_C(0xF300)
#define TUPLE(t) (t).identity,(t).elementIdentity,(t).transfer,(t).size,(t).type
extern void xl6TestListGraphBoundaryFailureV1(int,int);
static unsigned rule(unsigned profile)
{
    return profile==0 || profile==5 ? XL6_VALUE_ADMIT_I8 : profile==2 ? XL6_VALUE_ADMIT_BYTES :
        profile==3 ? XL6_VALUE_ADMIT_CHAR : profile==6 ? XL6_VALUE_ADMIT_F32 :
        profile==7 ? XL6_VALUE_ADMIT_U64 : profile==8 ? XL6_VALUE_ADMIT_BOOL : XL6_VALUE_ADMIT_STRING;
}
static bool nullable(unsigned profile) { return profile==4 || profile==5; }
static xvalue* seeds(xvalue** values,const XL6_CollectionImportProviderV1** providers,size_t count,unsigned profile)
{
    const XL6_CollectionImportProviderV1* list=xl6ListImportProviderV1();
    for (size_t at=count;at>0;--at) {
        size_t i=at-1; bool isList=(i%2)==0;
        providers[i]=isList ? list : NULL;
        uint64_t id=isList ? DOMAIN(i) : OWNER(i); xvalue* array;
        if (i+1<count) {
            array=providers[i+1] ? xl6SequenceListCreateTakeV1(1,id,OWNER(i+1),xrtValueRetain(values[i+1]),false) :
                xl6SequenceNestedCreateTakeV2(1,id,OWNER(i+1),xrtValueRetain(values[i+1]),false);
        } else {
            const xrttype* type=profile==0 ? xrtTypeInt64() : profile==3 ? xl6CharTypeV1() :
                profile==4 ? xl6NullableStringTypeV1() : profile==5 ? xl6NullableI64TypeV1() :
                profile==6 ? xrtTypeFloat32() : profile==7 ? xrtTypeUInt64() :
                profile==8 ? xrtTypeBool() : xl6StringTypeV1();
            unsigned transfer=profile==1 || profile==4 ? 2 : 1;
            array=profile==2 ? xl6SequenceReferenceCreateV1(1,id,LEAF,rule(profile),false,LEAF) :
                xl6SequencePrimitiveCreateV1(1,id,LEAF,transfer,type->Size,type,rule(profile),nullable(profile),LEAF);
        }
        assert(array);
        XL6_SequenceRoleV1 role; XL6_SequenceLayoutV1 layout={0}; XL6_SequenceAdmissionV3 policy={0};
        assert(xl6SequenceSchemaCopyV1(array,&role,&layout,&policy)); xl6SequenceAdmissionDropV3(&policy);
        const xvalueconverter* codec=i+1<count || profile==2 ? NULL : profile==1 ? xl6StringConverterV1() :
            profile==3 ? xl6CharConverterV1() : profile==4 ? xl6NullableStringConverterV1() :
            profile==5 ? xl6NullableI64ConverterV1() : NULL;
        if (isList) {
            XL6_ListLayoutV1 t={OWNER(i),layout};
            values[i]=xl6ListProjectionTakeV1(xl6ListCreateTakeV1(&t,array),&t,codec);
        } else values[i]=xl6ArrayProjectionTakeV1(array,TUPLE(layout),codec);
        assert(values[i]);
    }
    return values[0];
}
static XL6_CollectionImportPlanV1 plan(const XL6_CollectionImportProviderV1* const* providers,
    unsigned profile,bool restore,bool json)
{
    return (XL6_CollectionImportPlanV1){sizeof(XL6_CollectionImportPlanV1),providers,
        restore ? XL6_COLLECTION_IMPORT_RESTORE_V1 | (json ? XL6_COLLECTION_IMPORT_JSON_V1 : 0) : 0,
        restore ? rule(profile) : 0,restore ? LEAF : 0,restore && nullable(profile)};
}
static xvalue* leaf(unsigned profile,bool json,bool native)
{
    xvalue* value=profile==0 || profile==5 ? xrtValueInt(7) : profile==2 ?
        xrtValueBytes((xbytesview){(const uint8_t*)"a\0b",3}) : profile==3 ?
        (json ? xrtValueUInt(0x4F60) : xrtValueChar(0x4F60)) : profile==6 ? xrtValueFloat(0.1) :
        profile==7 ? xrtValueUInt(UINT64_MAX) : profile==8 ? xrtValueBool(true) :
        xrtValueString((xstrview){"a\0\xE4\xBD\xA0",5});
    assert(value);
    if (native && profile!=8) { assert(profile!=6); assert(xrtValueTypeIdBind(value,LEAF)); }
    return value;
}
static xvalue* wireLeaf(unsigned profile,bool json,bool native)
{
    xvalue* value=xrtValueArray(); assert(value);
    assert(xrtValueArrayAppendNew(value,leaf(profile,json,native)));
    if (nullable(profile)) assert(xrtValueArrayAppend(value,xrtValueNull()));
    return value;
}
static xvalue* wire(size_t count,unsigned profile,bool json,bool native,size_t width)
{
    xvalue* value=wireLeaf(profile,json,native);
    for (size_t i=1;i<count;++i) {
        xvalue* parent=xrtValueArray(); assert(parent);
        assert(xrtValueArrayAppend(parent,value) && xrtValueArrayAppend(parent,value));
        xrtValueRelease(value); value=parent;
    }
    if (width>1) {
        assert(count==3); xrtValueRelease(value); value=xrtValueArray(); assert(value);
        for (size_t i=0;i<width;++i) {
            xvalue* middle=wire(2,profile,json,native,1);
            assert(xrtValueArrayAppend(value,middle) && xrtValueArrayAppend(value,middle)); xrtValueRelease(middle);
        }
    }
    return value;
}
static xvalue* item(const xvalue* value,size_t depth,int64_t key)
{
    if (depth%2) return xl6ArrayErasedReadV1(value,key);
    xvalue* result=NULL; bool found=false;
    assert(xl6ListErasedReadV1(value,key,false,&result,&found) && found); return result;
}
static void observed(const xvalue* value,size_t count,unsigned profile)
{
    xvalue* current=xrtValueRetain(value); assert(current);
    for (size_t i=0;i+1<count;++i) {
        xvalue* a=item(current,i,0),*b=item(current,i,1); assert(a && a==b && a!=current);
        xrtValueRelease(current); xrtValueRelease(b); current=a;
    }
    xvalue* actual=item(current,count-1,0); assert(actual);
    if (profile==0 || profile==5) { int64_t n=0; assert(xrtValueGetInt(actual,&n) && n==7); }
    else if (profile==2) { xbytesview b={0}; assert(xrtValueGetBytes(actual,&b) && b.Size==3 && !memcmp(b.Data,"a\0b",3)); }
    else if (profile==3) { uint32 n=0; assert(xrtValueGetChar(actual,&n) && n==0x4F60); }
    else if (profile==6) { double n=0; assert(xrtValueGetFloat(actual,&n) && n==(double)(float)0.1); }
    else if (profile==7) { uint64 n=0; assert(xrtValueGetUInt(actual,&n) && n==UINT64_MAX); }
    else if (profile==8) { bool b=false; assert(xrtValueGetBool(actual,&b) && b); }
    else { xstrview text={0}; assert(xrtValueGetString(actual,&text) && text.Size==5 && !memcmp(text.Data,"a\0\xE4\xBD\xA0",5)); }
    assert(profile==8 || xrtValueTypeId(actual)==LEAF); xrtValueRelease(actual);
    if (nullable(profile)) { actual=item(current,count-1,1); assert(xrtValueType(actual)==XVALUE_NULL); xrtValueRelease(actual); }
    xrtValueRelease(current);
}
static void profiles(void)
{
    for (unsigned profile=0;profile<9;++profile) for (unsigned json=0;json<2;++json) {
        if (json && profile==2) continue;
        xvalue* values[MAX]={0}; const XL6_CollectionImportProviderV1* providers[MAX]={0};
        seeds(values,providers,1,profile); xvalue* source=wire(1,profile,json,false,1);
        XL6_CollectionImportPlanV1 p=plan(providers,profile,true,json);
        xrtSetErrorKind(XERR_TIMEOUT); const xerror* prior=xrtGetError();
        xvalue* result=xl6CollectionImportChainTakeV1(values,1,source,&p); assert(result && !values[0] && xrtGetError()==prior); xrtClearError();
        observed(result,1,profile); assert(xrtValueType(source)==XVALUE_ARRAY);
        assert(profile==8 || xrtValueTypeId(xrtValueArrayAt(source,0))==0);
        xrtValueRelease(result); xrtValueRelease(source);
    }
}
static void roundtrip(void)
{
    for (unsigned profile=0;profile<9;++profile) for (unsigned json=0;json<2;++json) {
        if (json && profile==2) continue;
        xvalue* source=wire(1,profile,json,false,1); size_t size=0;
        char* encoded=json ? xrtJsonStringify(source,false,&size) : xrtXlonStringify(source,false,&size);
        assert(encoded && size);
        xvalue* decoded=json ? xrtJsonParse((xstrview){encoded,size}) : xrtXlonParse((xstrview){encoded,size});
        assert(decoded); xrtFree(encoded);
        xvalue* values[MAX]={0}; const XL6_CollectionImportProviderV1* providers[MAX]={0};
        seeds(values,providers,1,profile); XL6_CollectionImportPlanV1 p=plan(providers,profile,true,json);
        xvalue* result=xl6CollectionImportChainTakeV1(values,1,decoded,&p); assert(result);
        observed(result,1,profile); xrtValueRelease(result); xrtValueRelease(decoded); xrtValueRelease(source);
    }
}
static void mixed(void)
{
    for (size_t count=2;count<=4;++count) {
        xvalue* values[MAX]={0}; const XL6_CollectionImportProviderV1* providers[MAX]={0};
        seeds(values,providers,count,1); xvalue* source=wire(count,1,false,false,1);
        XL6_CollectionImportPlanV1 p=plan(providers,1,true,false);
        xvalue* result=xl6CollectionImportChainTakeV1(values,count,source,&p); assert(result);
        observed(result,count,1); for (size_t i=0;i<count;++i) assert(!values[i]);
        xrtValueRelease(result); xrtValueRelease(source);
    }
    xvalue* values[MAX]={0}; const XL6_CollectionImportProviderV1* providers[MAX]={0};
    seeds(values,providers,3,1); xvalue* source=wire(3,1,false,false,70);
    XL6_CollectionImportPlanV1 p=plan(providers,1,true,false);
    xvalue* result=xl6CollectionImportChainTakeV1(values,3,source,&p); assert(result);
    xvalue* prior=NULL;
    for (int64_t i=0;i<70;++i) {
        xvalue* a=item(result,0,i*2),*b=item(result,0,i*2+1);
        assert(a && a==b && a!=prior); xrtValueRelease(prior); xrtValueRelease(b); prior=a;
    }
    xrtValueRelease(prior); xrtValueRelease(result); xrtValueRelease(source);
    /* Matching actual native children retain their original identity and
     * owned metadata; same physical tuple with another policy is refused. */
    for (unsigned mismatch=0;mismatch<2;++mismatch) {
        xvalue* original[MAX]={0}; const XL6_CollectionImportProviderV1* originalProviders[MAX]={0};
        seeds(original,originalProviders,3,mismatch ? 0 : 1);
        xvalue* nativeChild=original[2]; original[2]=NULL;
        xvalue* scalar=leaf(mismatch ? 0 : 1,false,true);
        assert(xl6ListErasedSetV1(nativeChild,INT64_MIN,scalar)); xrtValueRelease(scalar);
        xrtValueRelease(original[0]); xrtValueRelease(original[1]);
        xvalue* middle=xrtValueArray(),*root=xrtValueArray(); assert(middle && root);
        assert(xrtValueArrayAppend(middle,nativeChild) && xrtValueArrayAppend(middle,nativeChild));
        assert(xrtValueArrayAppend(root,middle) && xrtValueArrayAppend(root,middle)); xrtValueRelease(middle);
        seeds(values,providers,3,1); p=plan(providers,1,true,false);
        result=xl6CollectionImportChainTakeV1(values,3,root,&p);
        if (mismatch) { assert(!result && xrtGetError() && xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError(); }
        else {
            assert(result); middle=item(result,0,0); xvalue* kept=item(middle,1,0);
            assert(kept==nativeChild); xrtValueRelease(kept); xrtValueRelease(middle);
        }
        xrtValueRelease(result); xrtValueRelease(root); xrtValueRelease(nativeChild);
    }
}
static void rejectedValue(unsigned profile,xvalue* invalid,bool json)
{
    xvalue* values[MAX]={0}; const XL6_CollectionImportProviderV1* providers[MAX]={0};
    seeds(values,providers,1,profile); xvalue* source=xrtValueArray(); assert(source && invalid);
    uint64_t sourceIdentity=xrtValueTypeId(invalid);
    assert(xrtValueArrayAppendNew(source,invalid)); XL6_CollectionImportPlanV1 p=plan(providers,profile,true,json);
    assert(!xl6CollectionImportChainTakeV1(values,1,source,&p) && !values[0]);
    assert(xrtGetError() && xrtErrorKind(xrtGetError())==XERR_TYPE);
    assert(xrtValueTypeId(xrtValueArrayAt(source,0))==sourceIdentity); xrtClearError(); xrtValueRelease(source);
}
static void domains(void)
{
    rejectedValue(0,xrtValueInt(128),true); rejectedValue(0,xrtValueFloat(7),true);
    rejectedValue(0,xrtValueString((xstrview)XRT_STR_INIT("7")),true);
    rejectedValue(0,xrtValueNull(),true); rejectedValue(3,xrtValueUInt(0xD800),true);
    rejectedValue(3,xrtValueInt(0x4F60),false); rejectedValue(2,leaf(2,false,false),true);
    rejectedValue(6,xrtValueFloat(DBL_MAX),false); rejectedValue(7,xrtValueInt(-1),false);
    rejectedValue(8,xrtValueInt(1),true);
    xvalue* nominal=xrtValueString(XRT_STR_LITERAL("nominal")); assert(nominal && xrtValueTypeIdBind(nominal,LEAF+100));
    rejectedValue(1,nominal,false);
    nominal=xrtValueInt(7); assert(nominal && xrtValueTypeIdBind(nominal,LEAF+100)); rejectedValue(0,nominal,true);
    xvalue* values[MAX]={0}; const XL6_CollectionImportProviderV1* providers[MAX]={0};
    seeds(values,providers,1,2); xvalue* source=xrtValueArray(); assert(source);
    XL6_CollectionImportPlanV1 p=plan(providers,2,true,true);
    assert(!xl6CollectionImportChainTakeV1(values,1,source,&p) && !values[0]);
    assert(xrtGetError() && xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError(); xrtValueRelease(source);
}
static void rejected(void)
{
    for (unsigned variant=0;variant<6;++variant) {
        xvalue* values[MAX]={0}; const XL6_CollectionImportProviderV1* providers[MAX]={0};
        seeds(values,providers,3,1); xvalue* source=wire(3,1,false,false,1);
        XL6_CollectionImportPlanV1 p=plan(providers,1,true,false);
        if (variant==0) p.size--;
        else if (variant==1) p.flags|=128;
        else if (variant==2) p.leafRule=XL6_VALUE_ADMIT_NONE;
        else if (variant==3) p.leafIdentity=0;
        else if (variant==4) providers[0]=NULL;
        else { xrtValueRelease(source); source=xrtValueObject(); assert(source); }
        assert(!xl6CollectionImportChainTakeV1(values,3,source,&p));
        for (size_t i=0;i<3;++i) assert(!values[i]);
        assert(xrtGetError()); xrtClearError(); xrtValueRelease(source);
    }
    /* The actual wire-container boundary itself refuses direct self insertion;
     * do not fabricate a test-only backing graph to bypass that invariant. */
    xvalue* source=xrtValueArray(); assert(source && !xrtValueArrayAppend(source,source));
    assert(xrtGetError() && !xrtValueCount(source)); xrtClearError(); xrtValueRelease(source);
}
static void depth(void)
{
    xvalue* values[MAX]={0}; const XL6_CollectionImportProviderV1* providers[MAX]={0};
    seeds(values,providers,MAX,0); xvalue* source=wire(MAX,0,false,false,1);
    XL6_CollectionImportPlanV1 p=plan(providers,0,true,false);
    xvalue* result=xl6CollectionImportChainTakeV1(values,MAX,source,&p); assert(result);
    observed(result,MAX,0); xrtValueRelease(result); xrtValueRelease(source);
}
static unsigned drops;
static void failingDrop(ptr raw,ptr user)
{ (void)raw; (void)user; ++drops; xrtSetErrorKind(XERR_VALUE); xl6ForeignCallbackFail(); }
static const xvaluehandleops failingOps={.Drop=failingDrop};
static xvalue* failingSource(void)
{
    ptr raw=&drops; xvalue* value=xrtValueHandleTake(&raw,&failingOps,NULL),*source=xrtValueArray();
    assert(value && source && xrtValueArrayAppendNew(source,value)); return source;
}
static void entryErrors(void)
{
    for (int injection=-1;injection<=1;injection+=2) for (int cleanup=-1;cleanup<=1;++cleanup) {
        xvalue* values[MAX]={0}; const XL6_CollectionImportProviderV1* providers[MAX]={0};
        seeds(values,providers,1,1); xvalue* source=failingSource(); XL6_CollectionImportPlanV1 p=plan(providers,1,true,false);
        xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope)); unsigned before=drops;
        xl6TestListGraphBoundaryFailureV1(injection,cleanup);
        assert(!xl6CollectionImportChainSourceTakeV1(values,1,source,&p) && !values[0] && drops==before+1);
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
        assert(!outcome.ok && (injection<0 ? !outcome.error : outcome.error && xrtErrorKind(outcome.error)==XERR_MEMORY));
        xrtErrorFree(outcome.error); xrtClearError(); xl6TestListGraphBoundaryFailureV1(0,0);
    }
    xvalue* values[1]={NULL}; xvalue* source=failingSource(); unsigned before=drops;
    XL6_CollectionImportPlanV1 p=plan(NULL,1,true,false); xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
    xl6ErrorScopeCaptureFailure(&scope);
    assert(!xl6CollectionImportChainSourceTakeV1(values,1,source,&p) && drops==before+1);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope); assert(!outcome.ok && !outcome.error); xrtClearError();
}
static void cleanupErrors(void)
{
    for (int injection=-1;injection<=1;++injection) for (unsigned diagnostic=0;diagnostic<2;++diagnostic) {
        xvalue* source=failingSource(); XL6_ArrayInputV1 input={0};
        assert(xl6ArrayInputAcquireV1(source,&input) && input.count==1); xrtValueRelease(source);
        unsigned before=drops; xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
        if (diagnostic) xrtSetErrorKind(XERR_TIMEOUT); else xl6ErrorScopeCaptureFailure(&scope);
        xl6TestListGraphBoundaryFailureV1(0,injection); xl6ArrayInputReleaseV1(&input);
        assert(!input.items && !input.count && drops==before+1);
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
        assert(!outcome.ok && (diagnostic ? outcome.error && xrtErrorKind(outcome.error)==XERR_TIMEOUT : !outcome.error));
        xrtErrorFree(outcome.error); xrtClearError(); xl6TestListGraphBoundaryFailureV1(0,0);
    }
}
static unsigned providerFailure;
static bool badBegin(const xvalue* seed,ptr* output)
{
    bool ok=xl6ListImportProviderV1()->Begin(seed,output);
    if (ok && providerFailure==1) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    return ok;
}
static xvalue* badEmpty(ptr opaque)
{
    xvalue* value=xl6ListImportProviderV1()->Empty(opaque);
    if (value && providerFailure==2) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    return value;
}
static bool badAppend(xvalue* target,int64_t key,const xvalue* item)
{
    bool ok=xl6ListImportProviderV1()->Append(target,key,item);
    if (ok && providerFailure==3) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    return ok;
}
static void providerErrors(void)
{
    const XL6_CollectionImportProviderV1* real=xl6ListImportProviderV1();
    XL6_CollectionImportProviderV1 delegate={badBegin,real->End,badEmpty,real->Admits,badAppend};
    for (providerFailure=1;providerFailure<=3;++providerFailure) {
        xvalue* values[MAX]={0}; const XL6_CollectionImportProviderV1* providers[MAX]={0};
        seeds(values,providers,1,1); providers[0]=&delegate; xvalue* source=wire(1,1,false,false,1);
        XL6_CollectionImportPlanV1 p=plan(providers,1,true,false); xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
        assert(!xl6CollectionImportChainTakeV1(values,1,source,&p) && !values[0]);
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope); assert(!outcome.ok && !outcome.error);
        assert(xrtValueTypeId(xrtValueArrayAt(source,0))==0); xrtValueRelease(source); xrtClearError();
    }
    providerFailure=0;
}
void xl6TestListRestoreVectorsV1(const char* name)
{
    if (!strcmp(name,"restore-profiles")) profiles();
    else if (!strcmp(name,"restore-mixed")) mixed();
    else if (!strcmp(name,"restore-domains")) domains();
    else if (!strcmp(name,"restore-rejected")) rejected();
    else if (!strcmp(name,"restore-depth")) depth();
    else if (!strcmp(name,"restore-entry-errors")) entryErrors();
    else if (!strcmp(name,"restore-provider-errors")) providerErrors();
    else if (!strcmp(name,"restore-format-roundtrip")) roundtrip();
    else if (!strcmp(name,"restore-cleanup-errors")) cleanupErrors();
    else assert(0);
}
bool xl6TestListRestoreFaultV1(const char* name,size_t point)
{
    if (strncmp(name,"restore-",8)) return false;
    bool native=!strcmp(name,"restore-native"),json=!strcmp(name,"restore-json"),
        wide=!strcmp(name,"restore-wide"),take=!strcmp(name,"restore-source-take");
    assert(native || json || wide || take || !strcmp(name,"restore-xlon"));
    unsigned profile=json ? 3 : !strcmp(name,"restore-xlon") ? 2 : 1;
    xvalue* values[MAX]={0}; const XL6_CollectionImportProviderV1* providers[MAX]={0};
    seeds(values,providers,3,profile); xvalue* source=wire(3,profile,json,native,wide ? 70 : 1);
    XL6_CollectionImportPlanV1 p=plan(providers,profile,!native,json); xvalue* owned=take ? xrtValueRetain(source) : NULL;
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope)); assert(xrtMemDebugFailAfter(point));
    xvalue* result=take ? xl6CollectionImportChainSourceTakeV1(values,3,owned,&p) :
        xl6CollectionImportChainTakeV1(values,3,source,&p);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(hit ? !result && !outcome.ok : result && outcome.ok);
    for (size_t i=0;i<3;++i) assert(!values[i]);
    assert(xrtValueCount(source)==(wide ? 140 : 2));
    if (!hit && !wide) observed(result,3,profile);
    xrtErrorFree(outcome.error); xrtValueRelease(result); xrtValueRelease(source); xrtClearError();
    printf("native List %s point=%zu hit=%d complete-prefix balanced\n",name,point,hit); return true;
}
