/* Reuse actual native storage/schema fixtures; no mirrored Dict oracle. */
#define main xl6PriorDictNativeMain
#include "stdlib_dict_native_host.c"
#undef main
static int copyFailure;
extern bool __real_xrtTypeCopyValue(const xrttype*,ptr,const void*);
bool __wrap_xrtTypeCopyValue(const xrttype* type,ptr target,const void* source)
{
    int selected=copyFailure; copyFailure=0;
    bool success=__real_xrtTypeCopyValue(type,target,source);
    if (success && selected) {
        if (selected>0) xrtSetErrorKind(XERR_STATE);
        else xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    }
    return success;
}
static void directVectors(void)
{
    XL6_DictLayoutV1 t=layout(1,xrtTypeInt64()); xvalue* dict=create(&t,XL6_VALUE_ADMIT_I64); assert(dict);
    xstrview key={"a\0b",3}; int64_t output=0;
    assert(xl6DictSetV1(dict,&t,key,&(int64_t){7}));
    assert(xl6DictSetV1(dict,&t,XRT_STR_LITERAL("a"),&(int64_t){19}));
    xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
    assert(xrtMemDebugFailAfter(0));
    assert(xl6DictReadSlotV1(dict,&t,key,&output,true) && output==7);
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); xrtMemDebugSnapshot(&after);
    assert(before.AllocCount==after.AllocCount && before.LiveCount==after.LiveCount);
    output=0; xrtSetErrorKind(XERR_ARGUMENT); const xerror* incoming=xrtGetError();
    assert(xl6DictReadSlotV1(dict,&t,key,&output,true) && output==7 && xrtGetError()==incoming); xrtClearError();
    output=0; assert(!xl6DictReadSlotV1(dict,&t,XRT_STR_LITERAL("missing"),&output,true));
    assert(!output && xrtErrorKind(xrtGetError())==XERR_RANGE); xrtClearError();
    XL6_DictLayoutV1 wrong=t; wrong.value.elementIdentity+=1;
    assert(!xl6DictReadSlotV1(dict,&wrong,key,&output,true) && !output); xrtClearError();
    xvalue* cursor=xl6DictCursorCreateV1(dict,&t,CURSOR_ID,false); assert(cursor);
    assert(!xl6DictReadSlotV1(cursor,&t,key,&output,true) && !output); xrtClearError(); xrtValueRelease(cursor);
    xvalue* wire=xrtValueObject(); assert(wire);
    assert(!xl6DictReadSlotV1(wire,&t,key,&output,true) && !output); xrtClearError(); xrtValueRelease(wire);
    for (int failure=-1;failure<=1;failure+=2) {
        xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); entryFailure=failure;
        assert(!xl6DictReadSlotV1(dict,&t,key,&output,true) && !output && !entryFailure);
        xl6_error_result outcome=xl6ErrorScopeEnd(&parent); assert(!outcome.ok);
        assert(failure<0 ? !outcome.error : xrtErrorKind(outcome.error)==XERR_MEMORY); xrtErrorFree(outcome.error);
    }
    xrtValueRelease(dict);
    t=layout(2,xl6StringTypeV1()); dict=create(&t,XL6_VALUE_ADMIT_STRING); assert(dict);
    xstrview text={"v\0你",5},result={0}; assert(xl6DictSetV1(dict,&t,key,&text));
    assert(xl6DictReadSlotV1(dict,&t,key,&result,true));
    assert(result.Size==text.Size && result.Data!=text.Data && !memcmp(result.Data,text.Data,text.Size));
    ((char*)result.Data)[0]='Z'; xrtFree((ptr)result.Data); result=(xstrview){0};
    for (int failure=-1;failure<=1;failure+=2) for (int cleanup=0;cleanup<=1;++cleanup) {
        xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); copyFailure=failure;
        cleanupFailure=cleanup ? -1 : 0;
        assert(!xl6DictReadSlotV1(dict,&t,key,&result,true) && !result.Data && !result.Size);
        xl6_error_result outcome=xl6ErrorScopeEnd(&parent); assert(!outcome.ok && !copyFailure && !cleanupFailure);
        assert(failure<0 ? !outcome.error : xrtErrorKind(outcome.error)==XERR_STATE); xrtErrorFree(outcome.error);
    }
    assert(xl6DictReadSlotV1(dict,&t,key,&result,true) && !memcmp(result.Data,text.Data,text.Size));
    xrtFree((ptr)result.Data); xrtValueRelease(dict);
}
static bool directFault(size_t point,bool write)
{
    xl6ProductMemoryBegin(SIZE_MAX); XL6_DictLayoutV1 t=layout(2,xl6StringTypeV1());
    xvalue* dict=create(&t,XL6_VALUE_ADMIT_STRING); assert(dict);
    xstrview key={"a\0b",3},old={"v\0你",5},next={"next\0",5},result={0};
    assert(xl6DictSetV1(dict,&t,key,&old)); xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent));
    assert(xrtMemDebugFailAfter(point));
    bool success=write ? xl6DictSetV1(dict,&t,key,&next) : xl6DictReadSlotV1(dict,&t,key,&result,true);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); xl6_error_result outcome=xl6ErrorScopeEnd(&parent);
    assert(hit ? !success && !outcome.ok : success && outcome.ok);
    if (hit) assert(!result.Data && xrtErrorKind(outcome.error)==XERR_MEMORY);
    xrtFree((ptr)result.Data); result=(xstrview){0}; xrtErrorFree(outcome.error);
    assert(xl6DictReadSlotV1(dict,&t,key,&result,true));
    xstrview expected=write && !hit ? next : old;
    assert(result.Size==expected.Size && !memcmp(result.Data,expected.Data,expected.Size));
    xrtFree((ptr)result.Data); xrtValueRelease(dict); xrtClearError();
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    printf("direct slot write=%u point=%zu hit=%u balanced\n",write,point,hit); return hit;
}
int main(int argc,char** argv)
{
    assert(argc==2);
    if (!strcmp(argv[1],"vectors")) {
        assert(!xl6PriorDictNativeMain(argc,argv)); xl6ProductMemoryBegin(SIZE_MAX); directVectors();
        assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
        puts("[PASS] direct Dict slots, zero-allocation scalars, exact text and rollback"); return 0;
    }
    assert(!strcmp(argv[1],"prefixes"));
    for (unsigned write=0;write<2;++write) {
        size_t point=0; while(point<4096 && directFault(point,write)) ++point; assert(point<4096);
        printf("direct slot closed write=%u runs=%zu first-no-hit=%zu\n",write,point+1,point);
    }
    return 0;
}
