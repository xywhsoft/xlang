/* Reuse genuine Dict fixtures/codecs, not a mirrored container implementation. */
#define main xl6ExistingDictFoundationMain
#define __wrap_xl6ErrorScopeBegin xl6ExistingDictScopeBegin
#include "stdlib_dict_native_host.c"
#undef __wrap_xl6ErrorScopeBegin
#undef main
#include "src/runtime/xl6_dict_graph_access.h"
#include "src/runtime/xl6_value_index_read.h"
#include "src/runtime/xl6_value_index_write.h"

static size_t indexPoint,indexSeen;
static bool indexArmed,indexProviders;
static bool indexFailure(void)
{
    if (indexArmed && indexSeen++==indexPoint) { xrtClearError(); return true; }
    return false;
}
bool __wrap_xl6ErrorScopeBegin(xl6_error_scope* scope)
{ return !indexProviders && indexFailure() ? false : xl6ExistingDictScopeBegin(scope); }
extern bool __real_xl6DictErasedReadV1(const xvalue*,xstrview,bool,xvalue**,bool*);
bool __wrap_xl6DictErasedReadV1(const xvalue* source,xstrview key,bool take,xvalue** out,bool* found)
{ return indexProviders && indexFailure() ? false : __real_xl6DictErasedReadV1(source,key,take,out,found); }
extern bool __real_xl6DictErasedSetV1(const xvalue*,xstrview,const xvalue*);
bool __wrap_xl6DictErasedSetV1(const xvalue* source,xstrview key,const xvalue* item)
{ return indexProviders && indexFailure() ? false : __real_xl6DictErasedSetV1(source,key,item); }

static const xstrview indexKeys[]={{"",0},{"a\0\xE4\xBD\xA0",5},{"a",1}};
static void indexText(const xvalue* value,xstrview expected)
{
    xstrview actual={0};
    assert(value && xrtValueGetString(value,&actual) && actual.Data &&
        actual.Size==expected.Size && !memcmp(actual.Data,expected.Data,actual.Size));
}
static void indexError(xerrkind kind)
{ assert(xrtGetError() && xrtErrorKind(xrtGetError())==kind); xrtClearError(); }
static xvalue* indexInput(void)
{
    XL6_DictLayoutV1 t=layout(2,xl6StringTypeV1());
    xvalue* dict=projected(&t,XL6_VALUE_ADMIT_STRING,NULL); assert(dict);
    for (size_t i=0;i<3;++i) assert(xl6DictSetV1(dict,&t,indexKeys[i],&indexKeys[i]));
    return dict;
}
static void indexVectors(void)
{
    xvalue* dict=indexInput(),*key=xrtValueString(indexKeys[1]); assert(key);
    xvalue* escaped=xl6ValueIndexReadCopyV1(dict,key); indexText(escaped,indexKeys[1]);
    assert(xl6ValueIndexSetTakeV1(dict,key,xrtValueString(XRT_STR_LITERAL("replacement"))));
    indexText(escaped,indexKeys[1]);
    xvalue* result=xl6ValueIndexReadCopyV1(dict,key); indexText(result,XRT_STR_LITERAL("replacement"));
    xrtValueRelease(result);
    for (size_t i=0;i<3;++i) {
        xvalue* selected=xrtValueString(indexKeys[i]); assert(selected);
        assert(xl6ValueIndexSetTakeV1(dict,selected,xrtValueString(indexKeys[i])));
        result=xl6ValueIndexReadCopyV1(dict,selected); indexText(result,indexKeys[i]);
        xrtValueRelease(result); xrtValueRelease(selected);
    }
    xvalue* missing=xrtValueString(XRT_STR_LITERAL("missing")); assert(missing);
    result=xl6ValueIndexReadCopyV1(dict,missing); assert(result==xrtValueNull() && !xrtGetError()); xrtValueRelease(result);
    assert(xl6ValueIndexSetTakeV1(dict,missing,xrtValueString((xstrview){"v\0z",3})));
    result=xl6ValueIndexReadCopyV1(dict,missing); indexText(result,(xstrview){"v\0z",3}); xrtValueRelease(result);
    XL6_DictLayoutV1 t=layout(2,xl6StringTypeV1()); assert(xl6DictCountV1(dict,&t)==4);
    assert(!xl6ValueIndexSetTakeV1(dict,key,xrtValueInt(1))); indexError(XERR_TYPE);
    result=xl6ValueIndexReadCopyV1(dict,key); indexText(result,indexKeys[1]); xrtValueRelease(result);
    assert(xl6DictErasedClearV1(dict)); xrtValueRelease(dict);
    indexText(escaped,indexKeys[1]); xrtValueRelease(escaped); xrtValueRelease(missing); xrtValueRelease(key);
    /* The explicit wire path remains exact and owns its result too. */
    dict=xrtValueObject(); key=xrtValueString(indexKeys[1]); assert(dict && key);
    assert(xl6ValueIndexSetTakeV1(dict,key,xrtValueInt(77)));
    result=xl6ValueIndexReadCopyV1(dict,key); int64_t n=0;
    xrtValueRelease(dict); assert(result && xrtValueGetInt(result,&n) && n==77);
    xrtValueRelease(result); xrtValueRelease(key);
}
static void indexDomains(void)
{
    xvalue* key=xrtValueString(indexKeys[1]); assert(key); int64_t number=0; uint64_t wide=0;
    XL6_DictLayoutV1 t=layout(1,xrtTypeInt64()); xvalue* dict=projected(&t,XL6_VALUE_ADMIT_U8,&scalarCodec);
    assert(xl6ValueIndexSetTakeV1(dict,key,xrtValueInt(255)));
    assert(!xl6ValueIndexSetTakeV1(dict,key,xrtValueInt(256))); indexError(XERR_TYPE);
    xvalue* result=xl6ValueIndexReadCopyV1(dict,key); assert(result && xrtValueGetInt(result,&number) && number==255);
    xrtValueRelease(result);
    for (int mode=1;mode<=5;++mode) {
        xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); codecMode=mode; codecOwner=dict;
        bool read=mode==3 || mode==4;
        if (read) assert(!xl6ValueIndexReadCopyV1(dict,key));
        else assert(!xl6ValueIndexSetTakeV1(dict,key,xrtValueInt(2)));
        codecMode=0; codecOwner=NULL; xl6_error_result outcome=xl6ErrorScopeEnd(&parent);
        assert(!outcome.ok && ((mode==2 || mode==4) ? !outcome.error : outcome.error!=NULL)); xrtErrorFree(outcome.error);
        result=xl6ValueIndexReadCopyV1(dict,key); assert(result && xrtValueGetInt(result,&number) && number==255); xrtValueRelease(result);
    }
    xrtValueRelease(dict);
    t=layout(1,xrtTypeUInt64()); dict=projected(&t,XL6_VALUE_ADMIT_U64,NULL);
    assert(xl6ValueIndexSetTakeV1(dict,key,xrtValueUInt(UINT64_MAX)));
    result=xl6ValueIndexReadCopyV1(dict,key); assert(result && xrtValueGetUInt(result,&wide) && wide==UINT64_MAX);
    xrtValueRelease(result); xrtValueRelease(dict);
    t=layout(1,xl6CharTypeV1()); dict=projected(&t,XL6_VALUE_ADMIT_CHAR,xl6CharConverterV1());
    assert(xl6ValueIndexSetTakeV1(dict,key,xrtValueChar(0x4F60)));
    assert(!xl6ValueIndexSetTakeV1(dict,key,xrtValueUInt(65))); indexError(XERR_TYPE);
    result=xl6ValueIndexReadCopyV1(dict,key); uint32_t character=0;
    assert(result && xrtValueGetChar(result,&character) && character==0x4F60); xrtValueRelease(result); xrtValueRelease(dict);
    t=layout(2,xl6NullableStringTypeV1()); ++t.value.elementIdentity;
    xvalue* witness=xl6SequencePrimitiveCreateV1(1,ARRAY_ID,t.value.elementIdentity,2,t.value.size,t.value.type,
        XL6_VALUE_ADMIT_STRING,true,ELEMENT_ID); assert(witness);
    dict=xl6DictProjectionTakeV1(xl6DictCreateTakeV1(&t,witness),&t,xl6NullableStringConverterV1()); assert(dict);
    assert(xl6ValueIndexSetTakeV1(dict,key,xrtValueRetain(xrtValueNull())));
    result=xl6ValueIndexReadCopyV1(dict,key); assert(result==xrtValueNull()); xrtValueRelease(result);
    bool found=false; assert(xl6DictErasedKeyV1(dict,indexKeys[1],false,&found) && found);
    assert(xl6ValueIndexSetTakeV1(dict,key,xrtValueString(XRT_STR_LITERAL(""))));
    result=xl6ValueIndexReadCopyV1(dict,key); indexText(result,XRT_STR_LITERAL("")); xrtValueRelease(result); xrtValueRelease(dict);
    t=layout(3,xrtTypeValue()); dict=projected(&t,XL6_VALUE_ADMIT_BYTES,NULL);
    const uint8_t raw[]={0,255,0,1}; xvalue* bytes=xrtValueBytes((xbytesview){raw,sizeof(raw)}); assert(bytes);
    assert(xl6ValueIndexSetTakeV1(dict,key,xrtValueRetain(bytes)));
    result=xl6ValueIndexReadCopyV1(dict,key); assert(result==bytes); xrtValueRelease(dict); xrtValueRelease(bytes);
    xbytesview binary={0}; assert(xrtValueGetBytes(result,&binary) && binary.Size==sizeof(raw) && !memcmp(binary.Data,raw,sizeof(raw)));
    xrtValueRelease(result); xrtValueRelease(key);
}
static unsigned indexDrops;
static void indexDrop(ptr raw,ptr context)
{
    assert(raw==(ptr)(uintptr_t)1 && !context); ++indexDrops;
    xrtSetErrorKind(XERR_ARGUMENT); xl6ForeignCallbackFail();
}
static const xvaluehandleops indexDropOps={.Drop=indexDrop};
static xvalue* indexRejected(void)
{ ptr raw=(ptr)(uintptr_t)1; xvalue* value=xrtValueHandleTake(&raw,&indexDropOps,NULL); assert(value && !raw); return value; }
static void indexRefusals(void)
{
    XL6_DictLayoutV1 t=layout(2,xl6StringTypeV1()); xvalue* dict=indexInput(),*key=xrtValueString(indexKeys[1]); assert(key);
    xvalue* cursor=xl6DictCursorCreateV1(dict,&t,CURSOR_ID,false); assert(cursor);
    assert(!xl6ValueIndexReadCopyV1(cursor,key)); indexError(XERR_TYPE);
    assert(!xl6ValueIndexSetTakeV1(cursor,key,xrtValueString(indexKeys[1]))); indexError(XERR_TYPE); xrtValueRelease(cursor);
    ptr raw=NULL,user=NULL; const xvaluehandleops* ops=NULL; assert(xrtValueGetHandle(dict,&raw,&ops,&user));
    XL6_DictGraphViewV1 view={0}; assert(xl6DictGraphBeginV1(ops,raw,user,t.identity,&view)==1);
    assert(!xl6ValueIndexReadCopyV1(dict,key)); indexError(XERR_STATE);
    assert(!xl6ValueIndexSetTakeV1(dict,key,xrtValueString(indexKeys[1]))); indexError(XERR_STATE); xl6DictGraphEndV1(&view);
    ptr malformedRaw=NULL; assert(ops->Clone(raw,&malformedRaw,user));
    xvalue* malformed=xrtValueHandleTake(&malformedRaw,ops,user); assert(malformed && !malformedRaw);
    assert(xrtValueTypeIdBind(malformed,t.identity+1));
    assert(!xl6ValueIndexReadCopyV1(malformed,key)); indexError(XERR_STATE);
    assert(!xl6ValueIndexSetTakeV1(malformed,key,xrtValueString(indexKeys[1]))); indexError(XERR_STATE);
    xrtValueRelease(malformed);
    xvalue* unbound=create(&t,XL6_VALUE_ADMIT_STRING); assert(unbound);
    assert(!xl6ValueIndexReadCopyV1(unbound,key)); indexError(XERR_STATE);
    assert(!xl6ValueIndexSetTakeV1(unbound,key,xrtValueString(indexKeys[1]))); indexError(XERR_STATE); xrtValueRelease(unbound);
    raw=(ptr)(uintptr_t)1; xvalue* foreign=xrtValueHandleTake(&raw,&foreignOps,NULL); assert(foreign && xrtValueTypeIdBind(foreign,t.identity));
    xvalue* result=xl6ValueIndexReadCopyV1(foreign,key); assert(result==xrtValueNull() && !xrtGetError()); xrtValueRelease(result);
    assert(!xl6ValueIndexSetTakeV1(foreign,key,xrtValueString(indexKeys[1]))); indexError(XERR_TYPE); xrtValueRelease(foreign);
    unsigned before=indexDrops; xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent));
    assert(!xl6ValueIndexSetTakeV1(dict,key,indexRejected())); xl6_error_result outcome=xl6ErrorScopeEnd(&parent);
    assert(!outcome.ok && xrtErrorKind(outcome.error)==XERR_TYPE && indexDrops==before+1); xrtErrorFree(outcome.error);
    for (int failure=-1;failure<=1;failure+=2) {
        xvalue* rejected=indexRejected(); before=indexDrops; parent=(xl6_error_scope){0}; assert(xl6ErrorScopeBegin(&parent)); entryFailure=failure;
        assert(!xl6ValueIndexSetTakeV1(dict,key,rejected)); outcome=xl6ErrorScopeEnd(&parent);
        assert(!outcome.ok && indexDrops==before+1 && !entryFailure);
        assert(failure<0 ? !outcome.error : xrtErrorKind(outcome.error)==XERR_MEMORY); xrtErrorFree(outcome.error);
    }
    assert(xl6DictCountV1(dict,&t)==3); xrtValueRelease(key); xrtValueRelease(dict);
}
static bool indexAttempt(unsigned lane,size_t point,const char* mode)
{
    xl6ProductMemoryBegin(SIZE_MAX); xvalue* dict=indexInput();
    xvalue* key=xrtValueString(lane==1 || lane==2 ? XRT_STR_LITERAL("new") : indexKeys[1]);
    xvalue* item=xrtValueString(XRT_STR_LITERAL("replacement")),*result=NULL; assert(key && item);
    xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent));
    indexPoint=point; indexSeen=0; indexProviders=!strcmp(mode,"providers"); indexArmed=strcmp(mode,"oom")!=0;
    if (!indexArmed) assert(xrtMemDebugFailAfter(point));
    bool success;
    if (lane<2) { result=xl6ValueIndexReadCopyV1(dict,key); success=result!=NULL; }
    else { success=xl6ValueIndexSetTakeV1(dict,key,item); item=NULL; }
    bool hit=indexArmed ? indexSeen>point : xrtMemDebugFailTriggered(); indexArmed=false; indexProviders=false;
    xrtMemDebugFailClear(); xl6_error_result outcome=xl6ErrorScopeEnd(&parent);
    assert(success==!hit && outcome.ok==!hit); if (hit) assert(!result);
    if (hit && strcmp(mode,"oom")) assert(!outcome.error);
    XL6_DictLayoutV1 t=layout(2,xl6StringTypeV1()); assert(xl6DictCountV1(dict,&t)==(lane==2 && !hit ? 4 : 3));
    xstrview actual={0}; bool found=false;
    assert(xl6DictGetV1(dict,&t,indexKeys[1],&actual,false,&found) && found);
    xstrview expected=lane==3 && !hit ? XRT_STR_LITERAL("replacement") : indexKeys[1];
    assert(actual.Size==expected.Size && !memcmp(actual.Data,expected.Data,actual.Size)); xrtTypeDropValue(t.value.type,&actual);
    if (success && lane<2) { if (lane==1) assert(result==xrtValueNull()); else indexText(result,indexKeys[1]); }
    xrtErrorFree(outcome.error); xrtValueRelease(result); xrtValueRelease(item); xrtValueRelease(key); xrtValueRelease(dict); xrtClearError();
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    printf("Dict index lane=%u mode=%s point=%zu hit=%d balanced\n",lane,mode,point,hit); return hit;
}
#ifndef XL6_TEST_DICT_INDEX_NO_MAIN
int main(int argc,char** argv)
{
    assert(argc==2);
    if (!strcmp(argv[1],"vectors")) {
        xl6ProductMemoryBegin(SIZE_MAX); indexVectors(); indexDomains(); indexRefusals();
        assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
        puts("[PASS] native Dict dynamic index exact keys, owned outputs and checked domains balanced"); return 0;
    }
    assert(!strcmp(argv[1],"oom") || !strcmp(argv[1],"scopes") || !strcmp(argv[1],"providers"));
    for (unsigned lane=0;lane<4;++lane) {
        bool closed=false;
        for (size_t point=0;point<4096;++point) if (!indexAttempt(lane,point,argv[1])) {
            printf("Dict index closed lane=%u mode=%s runs=%zu first-no-hit=%zu\n",lane,argv[1],point+1,point);
            closed=true; break;
        }
        assert(closed);
    }
    return 0;
}
#endif
