/* Reuse the real resident scalar/text profiles, not a second test descriptor
 * that bypasses exact admission or the selected per-profile slot codec. */
#define main xl6PrimitiveIteratorFixtureMain
#include "test/cases/stdlib_array_primitive_admission_native_host.c"
#undef main
#include "src/runtime/xl6_array_value_iterator.h"
#include "src/runtime/xl6_value_iterator.h"
static const XL6_ValueIteratorRuntimeV2* iteratorApi;
#if PROFILE==9
static XL6_ValueIterator* activeIterator;
static unsigned hookMode,hookCalls;
static xvalue* hookFrom(const xrttype* type,const void* slot,ptr context)
{
    (void)context; ++hookCalls;
    if (hookMode==1) { xrtSetErrorKind(XERR_TYPE); return NULL; }
    if (hookMode==2) {
        bool found=true;
        assert(!iteratorApi->Advance(activeIterator,&found) && !found); error(XERR_STATE);
        assert(!iteratorApi->View(activeIterator)); error(XERR_STATE);
        hookMode=0;
    }
    return xrtValueFromTyped(type,slot,xl6StringConverterV1());
}
static bool hookTo(const xvalue* value,const xrttype* type,ptr slot,ptr context)
{ (void)context; return xrtValueToTyped(value,type,slot,xl6StringConverterV1()); }
static const xvalueconverter hookConverter={.ToTyped=hookTo,.FromTyped=hookFrom};
#endif
static xvalue* populated(const Spec* s,const xvalueconverter* converter)
{
    xvalue* owner=create(s),*good=element(s); assert(owner && good);
    Slot slot={0}; toSlot(s,good,&slot);
    assert(xl6ArrayPushV1(owner,LAYOUT(s),&slot) && xl6ArrayPushV1(owner,LAYOUT(s),&slot));
    xrtTypeDropValue(s->type,&slot); xrtValueRelease(good);
    xvalue* alias=xl6ArrayEraseV1(owner,LAYOUT(s),converter);
    assert(alias==owner); xrtValueRelease(alias); return owner;
}
static void projection(const Spec* s,const XL6_ValueIteratorView* view,size_t ordinal)
{
    assert(view && view->item && view->ordinal==ordinal &&
        view->key.Type==XVALUE_KEY_INDEX && view->key.Index==ordinal);
    assert(xrtValueTypeId(view->item)==projectionIdentity(s,view->item) &&
        xl6ValueAdmitsV1(view->item,s->rule,s->nullable,s->identity));
    xvalue* expected=element(s); assert(expected);
    if (projectionIdentity(s,expected)) assert(xrtValueTypeIdBind(expected,s->identity));
    assert(xrtValueEqual(view->item,expected)); xrtValueRelease(expected);
}
static void iteratorVectors(const Spec* s)
{
    xvalue* owner=populated(s,s->converter);
    XL6_Sequence* pin=xl6SequenceBeginV1(owner,LAYOUT(s)); assert(pin);
    assert(!iteratorApi->Create(owner)); error(XERR_STATE); xl6SequenceEndV1(pin);
    XL6_ValueIterator* cursor=iteratorApi->Create(owner); assert(cursor);
    /* Source mutation/release cannot alter the native snapshot, its exact
     * policy, owned strings or duplicate retained managed references. */
    assert(xl6ArrayClearV1(owner,LAYOUT(s))); xrtValueRelease(owner);
    bool found=false;
    for (size_t i=0;i<2;++i) {
        assert(iteratorApi->Advance(cursor,&found) && found);
        projection(s,iteratorApi->View(cursor),i);
    }
    assert(iteratorApi->Advance(cursor,&found) && !found && !iteratorApi->View(cursor)->item);
    assert(xrtMemDebugFailAfter(0));
    assert(iteratorApi->Advance(cursor,&found) && !found && !xrtMemDebugFailTriggered());
    xrtMemDebugFailClear(); iteratorApi->Destroy(cursor);
    owner=create(s); assert(owner); assert(!iteratorApi->Create(owner)); error(XERR_STATE);
    xvalue* alias=xl6ArrayEraseV1(owner,LAYOUT(s),s->converter); assert(alias==owner);
    cursor=iteratorApi->Create(owner); assert(cursor);
    assert(iteratorApi->Advance(cursor,&found) && !found); iteratorApi->Destroy(cursor);
    xrtValueRelease(alias); xrtValueRelease(owner);
#if PROFILE==9
    for (unsigned mode=1;mode<3;++mode) {
        owner=populated(s,&hookConverter); cursor=iteratorApi->Create(owner); assert(cursor);
        xrtValueRelease(owner); activeIterator=cursor; hookMode=mode; unsigned before=hookCalls;
        bool ok=iteratorApi->Advance(cursor,&found); assert(hookCalls==before+1);
        if (mode==1) {
            assert(!ok && !found); error(XERR_TYPE);
            assert(!iteratorApi->Advance(cursor,&found) && !found && hookCalls==before+1); error(XERR_STATE);
            assert(!iteratorApi->View(cursor)); error(XERR_STATE);
        } else { assert(ok && found && !hookMode); projection(s,iteratorApi->View(cursor),0); }
        activeIterator=NULL; hookMode=0; iteratorApi->Destroy(cursor);
    }
#endif
}
static void iteratorFault(const Spec* s,const char* operation,size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* owner=populated(s,s->converter); XL6_ValueIterator* cursor=NULL;
    if (strcmp(operation,"create")) { cursor=iteratorApi->Create(owner); assert(cursor); }
    if (!strcmp(operation,"second")) { bool found=false; assert(iteratorApi->Advance(cursor,&found) && found); }
    assert(xrtMemDebugFailAfter(point)); bool ok,found=false;
    if (!strcmp(operation,"create")) { cursor=iteratorApi->Create(owner); ok=cursor!=NULL; }
    else ok=iteratorApi->Advance(cursor,&found);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    if (hit) {
        assert(!ok && !found); error(XERR_MEMORY);
        if (cursor) {
            assert(!iteratorApi->Advance(cursor,&found) && !found); error(XERR_STATE);
            assert(!iteratorApi->View(cursor)); error(XERR_STATE);
        }
    } else { assert(ok && !xrtGetError()); if (strcmp(operation,"create")) projection(s,iteratorApi->View(cursor),!strcmp(operation,"second")); }
    assert(xl6ArraySizeV1(owner,LAYOUT(s),false)==2);
    iteratorApi->Destroy(cursor); xrtValueRelease(owner); balanced(&before);
    printf("profile=%d operation=%s point=%zu hit=%d balanced\n",PROFILE,operation,point,(int)hit);
}
int main(int argc,char** argv)
{
    iteratorApi=xl6ValueIteratorRuntimeV2Get(); assert(xrtMemDebugEnable(true));
    xrtSetErrorKind(XERR_MEMORY); xrtClearError();
    if (argc==3) {
        size_t point=(size_t)strtoull(argv[2],NULL,10);
        for (size_t i=0;i<sizeof(rules)/sizeof(*rules);++i) {
            Spec s=spec(rules[i]); iteratorFault(&s,argv[1],point);
        }
    } else {
        assert(argc==1);
        for (unsigned cycle=0;cycle<100;++cycle) {
            xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
            for (size_t i=0;i<sizeof(rules)/sizeof(*rules);++i) { Spec s=spec(rules[i]); iteratorVectors(&s); }
            balanced(&before);
        }
        printf("Native projected iterator profile=%d: 100 physical cycles balanced\n",PROFILE);
    }
    return 0;
}
