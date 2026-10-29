/* Reuse genuine private Dict fixtures; no test-side mirror storage. */
#define main xl6ExistingDictFoundationMain
#define __wrap_xl6ErrorScopeBegin xl6ExistingDictScopeBegin
#define __wrap_xl6ErrorScopeBeginCleanup xl6ExistingDictCleanupBegin
#include "stdlib_dict_native_host.c"
#undef __wrap_xl6ErrorScopeBeginCleanup
#undef __wrap_xl6ErrorScopeBegin
#undef main
#include "src/runtime/xl6_dict_methods.h"

static size_t point,seen;
static bool armed,providers;
static bool fail(void)
{ if (armed && seen++==point) { xrtClearError(); return true; } return false; }
bool __wrap_xl6ErrorScopeBegin(xl6_error_scope* s)
{ return !providers && fail() ? false : xl6ExistingDictScopeBegin(s); }
bool __wrap_xl6ErrorScopeBeginCleanup(xl6_error_scope* s,bool failed)
{ return !providers && fail() ? false : xl6ExistingDictCleanupBegin(s,failed); }
#define WRAP_BOOL(Name,Args,Call) extern bool __real_##Name Args; \
    bool __wrap_##Name Args { return providers && fail() ? false : __real_##Name Call; }
WRAP_BOOL(xl6DictErasedCountV1,(const xvalue* s,bool c,int64_t* o),(s,c,o))
WRAP_BOOL(xl6DictErasedKeyV1,(const xvalue* s,xstrview k,bool r,bool* o),(s,k,r,o))
WRAP_BOOL(xl6DictErasedClearV1,(const xvalue* s),(s))
WRAP_BOOL(xl6DictErasedReserveV1,(const xvalue* s,int64_t n),(s,n))
WRAP_BOOL(xl6DictErasedTrimV1,(const xvalue* s),(s))
WRAP_BOOL(xl6DictErasedReadV1,(const xvalue* s,xstrview k,bool t,xvalue** o,bool* f),(s,k,t,o,f))
WRAP_BOOL(xl6DictMergeV1,(const xvalue* t,const xvalue* s,const XL6_DictLayoutV1* l,bool r),(t,s,l,r))
WRAP_BOOL(xl6DictErasedSetV1,(const xvalue* s,xstrview k,const xvalue* v),(s,k,v))
extern xvalue* __real_xl6DictErasedCloneV1(const xvalue*);
xvalue* __wrap_xl6DictErasedCloneV1(const xvalue* s)
{ return providers && fail() ? NULL : __real_xl6DictErasedCloneV1(s); }

static const xstrview exact={"a\0b",3},prefix={"a",1};
static xvalue* input(bool wire)
{
    XL6_DictLayoutV1 t=layout(1,xrtTypeInt64());
    xvalue* dict=wire ? xrtValueObject() : xl6DictProjectionTakeV1(create(&t,XL6_VALUE_ADMIT_I64),&t,NULL);
    assert(dict);
    for (unsigned i=0;i<2;++i) {
        int64_t number=i ? 2 : 7; xstrview k=i ? prefix : exact;
        if (wire) { xvalue* v=xrtValueInt(number); assert(v && xrtValueObjectSetTake(dict,k,&v)); }
        else assert(xl6DictSetV1(dict,&t,k,&number));
    }
    return dict;
}
static void integer(const xvalue* result,int64_t expected)
{ int64_t value=0; assert(result && xrtValueGetInt(result,&value) && value==expected); }
static void unchanged(const xvalue* source)
{
    assert(xl6DictValueCountV1(source)==2);
    xvalue* result=xl6DictValueCloneV1(source); assert(result);
    xvalue* item=xl6DictValueTakeV1(result,exact); integer(item,7);
    xrtValueRelease(item); xrtValueRelease(result);
}
static void vectors(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    for (unsigned wire=0;wire<2;++wire) {
        xvalue* source=input(wire!=0),*text=xrtValueString(exact); assert(text);
        assert(xl6DictValueHasV1(source,text)); assert(xl6DictValueCountV1(source)==2);
        xvalue* exactOnly=xl6DictValueCloneV1(source); assert(exactOnly);
        xvalue* prefixItem=xl6DictValueTakeV1(exactOnly,prefix); integer(prefixItem,2);
        assert(xl6DictValueHasV1(exactOnly,text));
        xrtValueRelease(prefixItem); xrtValueRelease(exactOnly);
        assert(xl6DictValueCapacityV1(source)>=2);
        assert(!xl6DictValueRemoveV1(source,XRT_STR_LITERAL("missing")) && !xrtGetError());
        assert(xl6DictValueReserveV1(source,64) && xl6DictValueCapacityV1(source)>=64);
        assert(xl6DictValueTrimV1(source));
        xvalue* copy=xl6DictValueCloneV1(source); assert(copy && copy!=source);
        xvalue* item=xl6DictValueTakeV1(copy,exact); integer(item,7);
        assert(xl6DictValueCountV1(copy)==1); unchanged(source);
        xrtValueRelease(item); item=xl6DictValueTakeV1(copy,exact); assert(item==xrtValueNull());
        xrtValueRelease(item); assert(xl6DictValueClearV1(copy) && xl6DictValueClearV1(copy));
        assert(xl6DictValueCountV1(copy)==0); xrtValueRelease(copy);
        xl6_error_scope s={0}; assert(xl6ErrorScopeBegin(&s));
        assert(!xl6DictValueReserveV1(source,-1)); xl6_error_result e=xl6ErrorScopeEnd(&s);
        assert(!e.ok); xrtErrorFree(e.error); xrtClearError();
        int64_t nullable=99; assert(!xl6DictValueCountIfPresentV1(NULL,&nullable) && nullable==99);
        item=xl6DictValueCountOrNullV1(NULL); assert(item==xrtValueNull()); xrtValueRelease(item);
        xrtValueRelease(text); xrtValueRelease(source);
    }
    for (unsigned mix=0;mix<4;++mix) {
        xvalue* left=input((mix&1)!=0),*right=input((mix&2)!=0);
        xvalue* result=xl6DictValueMergeRightV1(left,right); assert(result);
        unchanged(left); unchanged(right); assert(xl6DictValueCountV1(result)==2);
        xrtValueRelease(result); result=xl6DictValueWithoutKeyV1(left,exact);
        assert(result && xl6DictValueCountV1(result)==1); unchanged(left);
        xvalue* prefixItem=xl6DictValueTakeV1(result,prefix); integer(prefixItem,2); xrtValueRelease(prefixItem);
        xrtValueRelease(result); xrtValueRelease(right); xrtValueRelease(left);
    }
    xvalue* dict=input(false); XL6_DictLayoutV1 t=layout(1,xrtTypeInt64());
    xvalue* cursor=xl6DictCursorCreateV1(dict,&t,CURSOR_ID,false); assert(cursor);
    xvalue* wrong=xrtValueArray(); assert(wrong);
    const xvalue* invalid[]={cursor,wrong};
    for (unsigned i=0;i<2;++i) {
        xl6_error_scope s={0}; assert(xl6ErrorScopeBegin(&s));
        assert(xl6DictValueCountV1(invalid[i])==0);
        xl6_error_result e=xl6ErrorScopeEnd(&s); assert(!e.ok); xrtErrorFree(e.error); xrtClearError();
        assert(xl6ErrorScopeBegin(&s)); assert(!xl6DictValueCloneV1(invalid[i]));
        e=xl6ErrorScopeEnd(&s); assert(!e.ok); xrtErrorFree(e.error); xrtClearError();
    }
    xrtValueRelease(wrong); xrtValueRelease(cursor); xrtValueRelease(dict);
    assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    puts("[PASS] Dict resident methods and mixed explicit wire vectors");
}
static bool attempt(unsigned lane,size_t at,const char* mode)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    xvalue* source=input(lane==13 || lane==14),*right=input(lane==12),*text=xrtValueString(exact),*result=NULL;
    assert(text); xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent));
    point=at; seen=0; providers=!strcmp(mode,"providers"); armed=strcmp(mode,"oom")!=0;
    if (!armed) assert(xrtMemDebugFailAfter(at));
    bool ok=true; int64_t size=0;
    switch(lane) {
    case 0: size=xl6DictValueCountV1(source); break;
    case 1: size=xl6DictValueCapacityV1(source); break;
    case 2: ok=xl6DictValueCountIfPresentV1(source,&size); break;
    case 3: ok=xl6DictValueHasV1(source,text); break;
    case 4: ok=xl6DictValueRemoveV1(source,exact); break;
    case 5: case 14: ok=xl6DictValueClearV1(source); break;
    case 6: ok=xl6DictValueReserveV1(source,64); break;
    case 7: ok=xl6DictValueTrimV1(source); break;
    case 8: result=xl6DictValueTakeV1(source,exact); ok=result!=NULL; break;
    case 9: result=xl6DictValueCloneV1(source); ok=result!=NULL; break;
    case 10: case 12: case 13: result=xl6DictValueMergeRightV1(source,right); ok=result!=NULL; break;
    case 11: result=xl6DictValueWithoutKeyV1(source,exact); ok=result!=NULL; break;
    default: assert(false);
    }
    bool hit=armed ? seen>at : xrtMemDebugFailTriggered(); armed=false; providers=false;
    xrtMemDebugFailClear(); xl6_error_result outcome=xl6ErrorScopeEnd(&parent);
    assert(outcome.ok==!hit); if (!hit) assert(ok);
    if (hit && lane>=8 && lane<=13) assert(!result);
    if (hit && strcmp(mode,"oom")) assert(!outcome.error);
    xrtErrorFree(outcome.error); xrtClearError();
    if (!hit && lane==0) assert(size==2);
    if (lane>=9 && lane<=13) { unchanged(source); unchanged(right); }
    xrtValueRelease(result); xrtValueRelease(text); xrtValueRelease(right); xrtValueRelease(source); xrtClearError();
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    printf("Dict methods lane=%u mode=%s point=%zu hit=%d balanced\n",lane,mode,at,hit); return hit;
}
int main(int argc,char** argv)
{
    assert(argc==2); if (!strcmp(argv[1],"vectors")) { vectors(); return 0; }
    assert(!strcmp(argv[1],"oom") || !strcmp(argv[1],"scopes") || !strcmp(argv[1],"providers"));
    for (unsigned lane=0;lane<15;++lane) {
        bool closed=false;
        for (size_t at=0;at<4096;++at) if (!attempt(lane,at,argv[1])) {
            printf("Dict methods closed lane=%u mode=%s runs=%zu first-no-hit=%zu\n",lane,argv[1],at+1,at);
            closed=true; break;
        }
        assert(closed);
    }
    return 0;
}
