#define main xl6ExistingListMethodsMain
#include "test/cases/stdlib_list_methods_native_host.c"
#undef main
#include "src/runtime/xl6_list_value_read.h"
#include "src/runtime/xl6_value_index_read.h"
#include "src/runtime/xl6_value_index_write.h"
#include "src/runtime/xl6_value_contains.h"
static void exact(const xvalue* value,const char* text,size_t size)
{ xstrview s={0}; assert(value && xrtValueGetString(value,&s) && s.Size==size && !memcmp(s.Data,text,size)); }
static unsigned indexDrops;
static void indexDrop(void* payload,void* context)
{
    assert(payload && !context); ++indexDrops;
    xrtSetErrorKind(XERR_ARGUMENT); xl6ForeignCallbackFail();
}
static const xvaluehandleops indexDropOps={.Drop=indexDrop};
static unsigned objectDrops;
static void objectDrop(void* payload,void* context)
{
    assert(payload==(void*)1 && context); ++objectDrops;
    xvalue* object=context;
    int64_t value=0;
    assert(xrtValueGetInt(xrtValueObjectGet(object,(xstrview){"a\0b",3}),&value) && value==7);
    xvalue* key=xrtValueString((xstrview){"second",6}); assert(key);
    assert(xl6ValueIndexSetTakeV1(object,key,xrtValueInt(8)));
    xrtValueRelease(key);
    /* Independently owned back-reference: a retirement callback may release
     * it, and may observe/mutate the committed receiver, outside its BUSY fence. */
    xrtValueRelease(object);
}
static const xvaluehandleops objectDropOps={.Drop=objectDrop};
static void objectRetirement(void)
{
    xvalue* object=xrtValueObject(),*key=xrtValueString((xstrview){"a\0b",3}); assert(object && key);
    xvalue* pin=xrtValueRetain(object); assert(pin);
    void* token=(void*)1; xvalue* old=xrtValueHandleTake(&token,&objectDropOps,pin); assert(old && !token);
    assert(xl6ValueIndexSetTakeV1(object,key,old)); unsigned before=objectDrops;
    assert(xl6ValueIndexSetTakeV1(object,key,xrtValueInt(7)) && objectDrops==before+1);
    int64_t value=0; assert(xrtValueGetInt(xrtValueObjectGet(object,(xstrview){"second",6}),&value) && value==8);
    xrtValueRelease(key); xrtValueRelease(object); assert(!xrtGetError());
}
static void indexProfiles(void)
{
    XL6_ListLayoutV1 t=tuple(); t.value.transfer=1; t.value.type=xrtTypeUInt64(); t.value.size=sizeof(uint64_t);
    xvalue* list=profile(&t,XL6_VALUE_ADMIT_U64,false,t.value.elementIdentity,NULL);
    uint64_t integer=UINT64_MAX,restored=0; assert(xl6ListSetV1(list,&t,INT64_MAX,&integer));
    xvalue* key=xrtValueInt(INT64_MAX),*result=xl6ValueIndexReadCopyV1(list,key); assert(key);
    assert(result && xrtValueType(result)==XVALUE_UINT && xrtValueGetUInt(result,&restored) && restored==integer);
    xrtValueRelease(result); xrtValueRelease(key); xrtValueRelease(list);
    t.value.type=xl6CharTypeV1(); t.value.size=sizeof(uint32_t);
    list=profile(&t,XL6_VALUE_ADMIT_CHAR,false,t.value.elementIdentity,xl6CharConverterV1());
    uint32_t scalar=0x4F60,character=0; assert(xl6ListSetV1(list,&t,INT64_MIN,&scalar));
    key=xrtValueInt(INT64_MIN); result=xl6ValueIndexReadCopyV1(list,key);
    assert(result && xrtValueType(result)==XVALUE_CHAR && xrtValueGetChar(result,&character) && character==scalar);
    xrtValueRelease(result); assert(xl6ValueIndexSetTakeV1(list,key,xrtValueChar(65)));
    assert(!xl6ValueIndexSetTakeV1(list,key,xrtValueUInt(65))); checkError(XERR_TYPE);
    xrtValueRelease(key); xrtValueRelease(list);
    t=tuple(); t.value.type=xl6NullableStringTypeV1(); t.value.elementIdentity++;
    list=profile(&t,XL6_VALUE_ADMIT_STRING,true,t.value.elementIdentity-1,xl6NullableStringConverterV1());
    xstrview absent={0}; assert(xl6ListSetV1(list,&t,0,&absent)); key=xrtValueInt(0);
    result=xl6ValueIndexReadCopyV1(list,key); assert(result==xrtValueNull()); xrtValueRelease(result);
    assert(xl6ValueIndexSetTakeV1(list,key,xrtValueString((xstrview){"",0})));
    result=xl6ValueIndexReadCopyV1(list,key); exact(result,"",0); xrtValueRelease(result);
    xrtValueRelease(key); xrtValueRelease(list);
    t=tuple(); t.value.transfer=3; t.value.type=xrtTypeValue(); t.value.size=sizeof(xvalue*);
    list=profile(&t,XL6_VALUE_ADMIT_BYTES,false,t.value.elementIdentity,NULL);
    const unsigned char binary[]={0,255,0,1}; xvalue* bytes=xrtValueBytes((xbytesview){binary,sizeof(binary)}); assert(bytes);
    assert(xl6ListSetV1(list,&t,INT64_MIN,&bytes)); key=xrtValueInt(INT64_MIN); result=xl6ValueIndexReadCopyV1(list,key);
    xbytesview data={0}; assert(result && xrtValueGetBytes(result,&data) && data.Size==sizeof(binary) && !memcmp(data.Data,binary,sizeof(binary)));
    assert(xl6ValueContainsV1(list,bytes)); xrtValueRelease(result);
    assert(!xl6ValueIndexSetTakeV1(list,key,query())); checkError(XERR_TYPE);
    xrtValueRelease(key); xrtValueRelease(bytes); xrtValueRelease(list);
}
static void indexVectors(void)
{
    xvalue* list=create(true),*key=xrtValueInt(INT64_MIN),*needle=query(); assert(key && needle);
    set(list,INT64_MIN); set(list,INT64_MAX);
    xvalue* value=xl6ValueIndexReadCopyV1(list,key); exact(value,"a\0\xE4\xBD\xA0",5);
    assert(xl6ListValueCountV1(list)==2 && value!=needle); xrtValueRelease(value);
    assert(xl6ValueContainsV1(list,needle));
    xvalue* absent=xrtValueString((xstrview){"absent",6}); assert(absent);
    assert(!xl6ValueContainsV1(list,absent) && !xrtGetError()); xrtValueRelease(absent);
    xvalue* missingKey=xrtValueInt(0); assert(missingKey);
    value=xl6ValueIndexReadCopyV1(list,missingKey); assert(value==xrtValueNull() && !xrtGetError()); xrtValueRelease(value);
    assert(xl6ValueIndexSetTakeV1(list,missingKey,xrtValueString((xstrview){"b\0c",3})));
    value=xl6ValueIndexReadCopyV1(list,missingKey); exact(value,"b\0c",3); xrtValueRelease(value);
    assert(xl6ListValueCountV1(list)==3);
    assert(xl6ValueIndexSetTakeV1(list,key,xrtValueString((xstrview){"tail",4})));
    value=xl6ListValueReadCopyV1(list,INT64_MIN); exact(value,"tail",4); xrtValueRelease(value);
    assert(!xl6ValueIndexSetTakeV1(list,key,xrtValueInt(9))); checkError(XERR_TYPE);
    void* token=(void*)1; xvalue* rejected=xrtValueHandleTake(&token,&indexDropOps,NULL); assert(rejected && !token);
    xl6_error_scope boundary={0}; assert(xl6ErrorScopeBegin(&boundary)); unsigned drops=indexDrops;
    assert(!xl6ValueIndexSetTakeV1(list,key,rejected)); xl6_error_result outcome=xl6ErrorScopeEnd(&boundary);
    assert(!outcome.ok && xrtErrorKind(outcome.error)==XERR_TYPE && indexDrops==drops+1);
    xrtErrorFree(outcome.error); xrtClearError();
    value=xl6ListValueReadCopyV1(list,INT64_MIN); exact(value,"tail",4); xrtValueRelease(value);
    XL6_ListLayoutV1 t=tuple(); xvalue* cursor=xl6ListCursorCreateV1(list,&t,0xD713,false,false,0); assert(cursor);
    assert(!xl6ValueIndexReadCopyV1(cursor,key)); checkError(XERR_TYPE);
    assert(!xl6ValueIndexSetTakeV1(cursor,key,query())); checkError(XERR_TYPE);
    assert(!xl6ValueContainsV1(cursor,needle)); checkError(XERR_TYPE); xrtValueRelease(cursor);
    void* raw=NULL; void* user=NULL; const xvaluehandleops* ops=NULL;
    assert(xrtValueGetHandle(list,&raw,&ops,&user)); XL6_ListGraphViewV1 pin={0};
    assert(xl6ListGraphBeginV1(ops,raw,user,t.identity,&pin)==1);
    assert(!xl6ValueIndexReadCopyV1(list,key)); checkError(XERR_STATE);
    assert(!xl6ValueIndexSetTakeV1(list,key,query())); checkError(XERR_STATE);
    assert(!xl6ValueContainsV1(list,needle)); checkError(XERR_STATE); xl6ListGraphEndV1(&pin);
    xvalue* plain=create(false); assert(!xl6ValueIndexReadCopyV1(plain,key)); checkError(XERR_STATE);
    assert(!xl6ValueIndexSetTakeV1(plain,key,query())); checkError(XERR_STATE);
    assert(!xl6ValueContainsV1(plain,needle)); checkError(XERR_STATE); xrtValueRelease(plain);
    raw=(void*)1; xvalue* foreign=xrtValueHandleTake(&raw,&foreignOps,NULL); assert(foreign);
    assert(xrtValueTypeIdBind(foreign,t.identity));
    value=xl6ValueIndexReadCopyV1(foreign,key); assert(value==xrtValueNull()); xrtValueRelease(value);
    assert(!xl6ValueIndexSetTakeV1(foreign,key,query())); checkError(XERR_TYPE);
    assert(!xl6ValueContainsV1(foreign,needle)); checkError(XERR_TYPE); xrtValueRelease(foreign);
    xrtValueRelease(missingKey); xrtValueRelease(key); xrtValueRelease(needle); xrtValueRelease(list);
}
static bool indexAttempt(unsigned operation,size_t point,bool scopes)
{
    xl6ProductMemoryBegin(SIZE_MAX); xvalue* list=create(true),*key=xrtValueInt(INT64_MIN),*needle=query(),*result=NULL;
    assert(key && needle); set(list,INT64_MIN); set(list,INT64_MAX);
    xl6_error_scope boundary={0}; assert(xl6ErrorScopeBegin(&boundary));
    if (scopes) { scopeSeen=0; scopeCountdown=(int)point+1; } else assert(xrtMemDebugFailAfter(point));
    bool success=false;
    if (!operation) { result=xl6ValueIndexReadCopyV1(list,key); success=result!=NULL; }
    else if (operation==1) success=xl6ValueIndexSetTakeV1(list,key,xrtValueRetain(needle));
    else success=xl6ValueContainsV1(list,needle);
    bool hit=scopes ? scopeSeen>=scopeCountdown : xrtMemDebugFailTriggered();
    scopeCountdown=0; xrtMemDebugFailClear(); xl6_error_result outcome=xl6ErrorScopeEnd(&boundary);
    assert(success==!hit && outcome.ok==!hit);
    if (hit) assert(!result && xl6ListValueCountV1(list)==2);
    xrtErrorFree(outcome.error); xrtValueRelease(result); xrtValueRelease(needle); xrtValueRelease(key); xrtValueRelease(list);
    xrtClearError(); assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    printf("List index operation=%u point=%zu hit=%d balanced\n",operation,point,hit); return hit;
}
#ifndef XL6_TEST_LIST_INDEX_NO_MAIN
int main(int argc,char** argv)
{
    assert(argc==2);
    if (!strcmp(argv[1],"vectors")) {
        xl6ProductMemoryBegin(SIZE_MAX); indexVectors(); indexProfiles(); profiles(); objectRetirement();
        assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
        puts("Native List index, domains, exact text and authenticated rejection balanced"); return 0;
    }
    bool scopes=!strcmp(argv[1],"scopes"); assert(scopes || !strcmp(argv[1],"oom"));
    for (unsigned operation=0;operation<3;++operation) {
        bool closed=false;
        for (size_t point=0;point<4096;++point) if (!indexAttempt(operation,point,scopes)) {
            printf("List index closed-batch operation=%u runs=%zu first-no-hit=%zu\n",operation,point+1,point);
            closed=true; break;
        }
        assert(closed);
    }
    return 0;
}
#endif
