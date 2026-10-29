/* Reuse genuine storage fixtures and full-prefix injection, not an oracle
 * container. Test-only boxed-key bridges call the production exact-view ABI. */
#include "src/runtime/xl6_dict_value_index.h"
static xvalue* exactRead(const xvalue*,const xvalue*);
static bool exactWrite(xvalue*,const xvalue*,xvalue*);
#define XL6_TEST_DICT_INDEX_NO_MAIN
#define xl6ValueIndexReadCopyV1 exactRead
#define xl6ValueIndexSetTakeV1 exactWrite
#include "stdlib_dict_index_native_host.c"
#undef xl6ValueIndexReadCopyV1
#undef xl6ValueIndexSetTakeV1
#undef main
static xvalue* exactRead(const xvalue* source,const xvalue* key)
{ xstrview text={0}; assert(xrtValueGetString(key,&text)); return xl6DictValueReadCopyV1(source,text); }
static bool exactWrite(xvalue* source,const xvalue* key,xvalue* item)
{ xstrview text={0}; assert(xrtValueGetString(key,&text)); return xl6DictValueSetTakeV1(source,text,item); }
static void exactRefusals(void)
{
    xvalue* dict=indexInput(); xstrview missing=XRT_STR_LITERAL("missing");
    assert(!xl6DictValueReadRequiredCopyV1(dict,missing)); indexError(XERR_RANGE);
    xvalue* value=xl6DictValueReadOrNullCopyV1(NULL,missing);
    assert(value==xrtValueNull() && !xrtGetError()); xrtValueRelease(value);
    value=xl6DictValueReadOrNullCopyV1(xrtValueNull(),missing);
    assert(value==xrtValueNull() && !xrtGetError()); xrtValueRelease(value);
    assert(!xl6DictValueReadCopyV1(NULL,missing)); indexError(XERR_ARGUMENT);
    assert(!xl6DictValueReadCopyV1(xrtValueNull(),missing)); indexError(XERR_TYPE);
    assert(!xl6DictValueReadOrNullCopyV1(NULL,(xstrview){NULL,1})); indexError(XERR_ARGUMENT);
    XL6_DictLayoutV1 t=layout(2,xl6StringTypeV1());
    xvalue* cursor=xl6DictCursorCreateV1(dict,&t,CURSOR_ID,false); assert(cursor);
    assert(!xl6DictValueReadOrNullCopyV1(cursor,missing)); indexError(XERR_TYPE); xrtValueRelease(cursor);
    ptr raw=NULL,user=NULL; const xvaluehandleops* ops=NULL;
    assert(xrtValueGetHandle(dict,&raw,&ops,&user)); XL6_DictGraphViewV1 view={0};
    assert(xl6DictGraphBeginV1(ops,raw,user,t.identity,&view)==1);
    assert(!xl6DictValueReadOrNullCopyV1(dict,missing)); indexError(XERR_STATE);
    assert(!xl6DictValueSetTakeV1(dict,missing,xrtValueString(missing))); indexError(XERR_STATE);
    xl6DictGraphEndV1(&view);
    xvalue* unbound=create(&t,XL6_VALUE_ADMIT_STRING); assert(unbound);
    assert(!xl6DictValueReadOrNullCopyV1(unbound,missing)); indexError(XERR_STATE); xrtValueRelease(unbound);
    raw=(ptr)(uintptr_t)1; xvalue* foreign=xrtValueHandleTake(&raw,&foreignOps,NULL);
    assert(foreign && xrtValueTypeIdBind(foreign,t.identity));
    assert(!xl6DictValueReadOrNullCopyV1(foreign,missing)); indexError(XERR_TYPE); xrtValueRelease(foreign);
    for (int failure=-1;failure<=1;failure+=2) {
        xvalue* rejected=indexRejected(); unsigned before=indexDrops;
        xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); entryFailure=failure;
        assert(!xl6DictValueSetTakeV1(dict,missing,rejected)); xl6_error_result outcome=xl6ErrorScopeEnd(&parent);
        assert(!outcome.ok && indexDrops==before+1 && !entryFailure);
        assert(failure<0 ? !outcome.error : xrtErrorKind(outcome.error)==XERR_MEMORY); xrtErrorFree(outcome.error);
    }
    xrtValueRelease(dict);
    dict=xrtValueObject(); assert(dict);
    assert(xl6DictValueSetTakeV1(dict,missing,xrtValueRetain(xrtValueNull())));
    value=xl6DictValueReadRequiredCopyV1(dict,missing);
    assert(value==xrtValueNull() && !xrtGetError()); xrtValueRelease(value); xrtValueRelease(dict);
}
int main(int argc,char** argv)
{
    (void)indexRefusals; /* Its open-dynamic foreign miss rule is not this ABI. */
    assert(argc==2);
    if (!strcmp(argv[1],"vectors")) {
        xl6ProductMemoryBegin(SIZE_MAX); indexVectors(); indexDomains(); exactRefusals();
        assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
        puts("[PASS] exact Dict index keys, owned outputs, required/safe errors balanced"); return 0;
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
