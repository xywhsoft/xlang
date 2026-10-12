/* Reuse the original five-domain and rollback vectors unchanged. The actual
 * supplier operations below count one paired capture and inject partial-row
 * failure; they do not simulate the slot-map or its storage. */
#define main xl6UnarySnapshotsMain
#define __wrap_xrtTypedArrayPush xl6UnaryWrapPush
#define __wrap_xrtSlotMapIterNext xl6UnaryWrapNext
#include "stdlib_slot_map_snapshot_host.c"
#undef main
#undef __wrap_xrtTypedArrayPush
#undef __wrap_xrtSlotMapIterNext

static bool pairedCapture;
static unsigned begins,nexts,pushes,failPush,stopNext;
static int failureKind;
extern bool __real_xrtSlotMapIterBegin(const xslotmap*,xslotmapiter*);
bool __wrap_xrtSlotMapIterBegin(const xslotmap* source,xslotmapiter* iterator)
{
    if(pairedCapture) ++begins;
    return __real_xrtSlotMapIterBegin(source,iterator);
}
static void failSelected(void)
{
    if(failureKind>0) xrtSetErrorKind(XERR_STATE);
    else if(failureKind<0) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
}
bool __wrap_xrtTypedArrayPush(xtypedarray* output,const void* input)
{
    bool ok=xl6UnaryWrapPush(output,input);
    if(pairedCapture && ++pushes==failPush && ok) { failSelected(); return false; }
    return ok;
}
ptr __wrap_xrtSlotMapIterNext(xslotmapiter* iterator,xslot* slot)
{
    if(pairedCapture && ++nexts==stopNext) { failSelected(); return NULL; }
    return xl6UnaryWrapNext(iterator,slot);
}
static void observe(unsigned selectedPush,unsigned selectedNext,int kind)
{
    begins=nexts=pushes=0; failPush=selectedPush; stopNext=selectedNext; failureKind=kind; pairedCapture=true;
}
static void discardPair(XL6_SlotMapSnapshotPairV1* pair)
{ xrtValueRelease(pair->keys); xrtValueRelease(pair->values); *pair=(XL6_SlotMapSnapshotPairV1){0}; }
static void textPairs(void)
{
    XL6_SlotMapLayoutV1 layout=textLayout(); xvalue* map=make(&layout,XL6_VALUE_ADMIT_STRING); assert(map);
    xstrview a={"a\0你",5},b={"b\0好",5},c={"replacement",11}; XL6_SlotMapKeyV1 ka={0},kb={0},kc={0}; bool found=false;
    assert(xl6SlotMapInsertV1(map,&layout,&a,&ka) && xl6SlotMapInsertV1(map,&layout,&b,&kb));
    assert(xl6SlotMapKeyV1(map,&layout,ka,true,&found) && found && xl6SlotMapInsertV1(map,&layout,&c,&kc));
    XL6_SlotMapSnapshotPairV1 pair={0}; observe(0,0,0);
    assert(xl6SlotMapPairsV1(map,&layout,0xCA14,xl6StringConverterV1(),&pair)); pairedCapture=false;
    assert(begins==1 && nexts==3 && pushes==4);
    XL6_Sequence* keys=keysBegin(pair.keys),*values=valuesBegin(pair.values,&layout); assert(keys && values);
    assert(xrtTypedArrayCount(xl6SequenceStorageV1(keys))==2 && xrtTypedArrayCount(xl6SequenceStorageV1(values))==2);
    const XL6_SlotMapKeyV1* first=xrtTypedArrayConstGet(xl6SequenceStorageV1(keys),0);
    const xstrview* firstText=xrtTypedArrayConstGet(xl6SequenceStorageV1(values),0);
    assert(xl6SlotMapKeyEqualV1(*first,kc) && !xl6SlotMapKeyEqualV1(*first,ka));
    assert(firstText->Size==c.Size && !memcmp(firstText->Data,c.Data,c.Size) && firstText->Data!=c.Data);
    assert(xl6SlotMapKeyEqualV1(*(const XL6_SlotMapKeyV1*)xrtTypedArrayConstGet(xl6SequenceStorageV1(keys),1),kb));
    const xstrview* second=xrtTypedArrayConstGet(xl6SequenceStorageV1(values),1);
    assert(second->Size==b.Size && !memcmp(second->Data,b.Data,b.Size));
    xl6SequenceEndV1(values); xl6SequenceEndV1(keys);
    assert(xl6SlotMapSetV1(map,&layout,kc,&a,&found) && found && xl6SlotMapClearV1(map,&layout));
    xrtValueRelease(map); dropKey(&ka); dropKey(&kb); dropKey(&kc);
    keys=keysBegin(pair.keys); values=valuesBegin(pair.values,&layout); assert(keys && values);
    assert(xrtTypedArrayCount(xl6SequenceStorageV1(keys))==2 && xrtTypedArrayCount(xl6SequenceStorageV1(values))==2);
    second=xrtTypedArrayConstGet(xl6SequenceStorageV1(values),1); assert(second->Size==5 && second->Data[1]==0);
    xl6SequenceEndV1(values); xl6SequenceEndV1(keys); discardPair(&pair);

    map=make(&layout,XL6_VALUE_ADMIT_STRING); assert(map);
    observe(0,0,0); assert(xl6SlotMapPairsV1(map,&layout,0xCA14,xl6StringConverterV1(),&pair)); pairedCapture=false;
    assert(begins==1 && nexts==1 && pushes==0);
    keys=keysBegin(pair.keys); values=valuesBegin(pair.values,&layout);
    assert(keys && values && !xrtTypedArrayCount(xl6SequenceStorageV1(keys)) && !xrtTypedArrayCount(xl6SequenceStorageV1(values)));
    xl6SequenceEndV1(values); xl6SequenceEndV1(keys); discardPair(&pair); xrtValueRelease(map);
}
static void failedPairs(void)
{
    XL6_SlotMapLayoutV1 layout=textLayout(); xvalue* map=make(&layout,XL6_VALUE_ADMIT_STRING); assert(map);
    xstrview text={"a\0你",5}; XL6_SlotMapKeyV1 a={0},b={0};
    assert(xl6SlotMapInsertV1(map,&layout,&text,&a) && xl6SlotMapInsertV1(map,&layout,&text,&b));
    XL6_SlotMapSnapshotPairV1 pair={0};
    for(int kind=-1;kind<=1;kind+=2) for(unsigned selected=1;selected<=4;++selected) {
        xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope)); observe(selected,0,kind);
        assert(!xl6SlotMapPairsV1(map,&layout,0xCA14,xl6StringConverterV1(),&pair)); pairedCapture=false;
        assert(!pair.keys && !pair.values && pushes==selected);
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope); assert(!outcome.ok);
        if(kind<0) assert(!outcome.error); else assert(xrtErrorKind(outcome.error)==XERR_STATE);
        xrtErrorFree(outcome.error); assert(xl6SlotMapCountV1(map,&layout)==2);
        assert(xl6SlotMapPairsV1(map,&layout,0xCA14,xl6StringConverterV1(),&pair)); discardPair(&pair);
    }
    for(int kind=-1;kind<=1;++kind) {
        xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope)); observe(0,2,kind);
        assert(!xl6SlotMapPairsV1(map,&layout,0xCA14,xl6StringConverterV1(),&pair)); pairedCapture=false;
        assert(!pair.keys && !pair.values); xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
        assert(!outcome.ok); xrtErrorFree(outcome.error);
    }
    xvalue* witness=xl6SequenceSlotMapKeyCreateV1(1,0xCA14),*alias=xrtValueRetain(witness); assert(alias);
    assert(xl6SlotMapSnapshotPairTakeV1(map,&layout,witness,xl6SlotMapKeyConverterV1(),xl6StringConverterV1(),&pair));
    XL6_Sequence* empty=keysBegin(alias); assert(empty && !xrtTypedArrayCount(xl6SequenceStorageV1(empty)));
    xl6SequenceEndV1(empty); xrtValueRelease(alias); discardPair(&pair);
    witness=xl6SequenceSlotMapKeyCreateV1(1,0xCA14); alias=xrtValueRetain(witness); assert(alias);
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope)); observe(2,0,-1);
    assert(!xl6SlotMapSnapshotPairTakeV1(map,&layout,witness,xl6SlotMapKeyConverterV1(),xl6StringConverterV1(),&pair));
    pairedCapture=false; xl6_error_result failure=xl6ErrorScopeEnd(&scope); assert(!failure.ok && !failure.error);
    empty=keysBegin(alias); assert(empty && !xrtTypedArrayCount(xl6SequenceStorageV1(empty)));
    xl6SequenceEndV1(empty); xrtValueRelease(alias);
    xvalueconverter bad=*xl6StringConverterV1(); bad.Context=(ptr)1;
    assert(!xl6SlotMapPairsV1(map,&layout,0xCA14,&bad,&pair)); xrtClearError();
    assert(!pair.keys && !pair.values);
    assert(!xl6SlotMapPairsV1(map,&layout,0,xl6StringConverterV1(),&pair)); xrtClearError();
    XL6_SlotMapLayoutV1 wrong=layout; ++wrong.value.elementIdentity;
    assert(!xl6SlotMapPairsV1(map,&wrong,0xCA14,xl6StringConverterV1(),&pair)); xrtClearError();
    assert(xl6SlotMapPairsV1(map,&layout,0xCA14,xl6StringConverterV1(),&pair));
    XL6_SlotMapSnapshotPairV1 saved=pair;
    assert(!xl6SlotMapPairsV1(map,&layout,0xCA14,xl6StringConverterV1(),&pair));
    assert(pair.keys==saved.keys && pair.values==saved.values); xrtClearError(); discardPair(&pair);
    sourceMap=map; sourceLayout=layout; reenter=true;
    assert(!xl6SlotMapPairsV1(map,&layout,0xCA14,xl6StringConverterV1(),&pair) && !reenter);
    assert(!pair.keys && !pair.values); xrtClearError();
    xrtSetErrorKind(XERR_ARGUMENT); const xerror* pending=xrtGetError();
    assert(xl6SlotMapPairsV1(map,&layout,0xCA14,xl6StringConverterV1(),&pair) && xrtGetError()==pending);
    xrtClearError(); discardPair(&pair); dropKey(&a); dropKey(&b); xrtValueRelease(map);
}
static void pairDomain(XL6_SlotMapLayoutV1 layout,unsigned rule,const void* input,const xvalueconverter* converter)
{
    xvalue* map=make(&layout,rule); assert(map); XL6_SlotMapKeyV1 key={0};
    assert(xl6SlotMapInsertV1(map,&layout,input,&key)); XL6_SlotMapSnapshotPairV1 pair={0};
    assert(xl6SlotMapPairsV1(map,&layout,0xCA14,converter,&pair));
    xrtValueRelease(map); XL6_Sequence* kp=keysBegin(pair.keys),*vp=valuesBegin(pair.values,&layout); assert(kp && vp);
    assert(xrtTypedArrayCount(xl6SequenceStorageV1(kp))==1 && xrtTypedArrayCount(xl6SequenceStorageV1(vp))==1);
    assert(xl6SlotMapKeyEqualV1(*(const XL6_SlotMapKeyV1*)xrtTypedArrayConstGet(xl6SequenceStorageV1(kp),0),key));
    const void* copy=xrtTypedArrayConstGet(xl6SequenceStorageV1(vp),0);
    if(layout.value.transfer==1) assert(*(const int64_t*)copy==*(const int64_t*)input);
    if(layout.value.transfer==3) assert(*(xvalue* const*)copy==*(xvalue* const*)input);
    if(layout.value.transfer==4) assert(*(xfuture* const*)copy==*(xfuture* const*)input);
    if(layout.value.transfer==5) assert(xl6SlotMapKeyEqualV1(*(const XL6_SlotMapKeyV1*)copy,*(const XL6_SlotMapKeyV1*)input));
    xl6SequenceEndV1(vp); xl6SequenceEndV1(kp); dropKey(&key); discardPair(&pair);
}
static void otherPairs(void)
{
    pairDomain((XL6_SlotMapLayoutV1){0xD011,{0xD012,0xD013,1,sizeof(int64_t),xrtTypeInt64()}},XL6_VALUE_ADMIT_I64,&(int64_t){42},NULL);
    xvalue* reference=xrtValueArray(); assert(reference);
    pairDomain((XL6_SlotMapLayoutV1){0xD021,{0xD022,0xD023,3,sizeof(xvalue*),xrtTypeValue()}},XL6_VALUE_ADMIT_ANY,&reference,xl6ArrayReferenceConverterV1());
    xrtValueRelease(reference);
    xfuture* future=NULL; xpromise* promise=xrtPromiseCreate(&future,NULL); assert(promise);
    pairDomain((XL6_SlotMapLayoutV1){0xD031,{0xD032,0xD033,4,sizeof(xfuture*),xrtTypeFuture()}},0,&future,NULL);
    xrtPromiseDestroy(promise); xrtFutureDestroy(future);
    XL6_SlotMapLayoutV1 source=textLayout(); xvalue* map=make(&source,XL6_VALUE_ADMIT_STRING); assert(map);
    XL6_SlotMapKeyV1 key={0}; assert(xl6SlotMapInsertV1(map,&source,&(xstrview){"key",3},&key));
    pairDomain((XL6_SlotMapLayoutV1){0xD041,{0xD042,XL6_SLOT_MAP_KEY_LANGUAGE_VALUE_ID,5,sizeof(key),xl6SlotMapKeyTypeV1()}},0,&key,xl6SlotMapKeyConverterV1());
    xrtValueRelease(map); dropKey(&key);
}
static bool pairFault(size_t point)
{
    xl6ProductMemoryBegin(SIZE_MAX); XL6_SlotMapLayoutV1 layout=textLayout();
    xvalue* map=make(&layout,XL6_VALUE_ADMIT_STRING); assert(map); XL6_SlotMapKeyV1 a={0},b={0}; xstrview text={"a\0你",5};
    assert(xl6SlotMapInsertV1(map,&layout,&text,&a) && xl6SlotMapInsertV1(map,&layout,&text,&b));
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope)); assert(xrtMemDebugFailAfter(point));
    XL6_SlotMapSnapshotPairV1 pair={0}; bool success=xl6SlotMapPairsV1(map,&layout,0xCA14,xl6StringConverterV1(),&pair);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(hit ? !success && !pair.keys && !pair.values && !outcome.ok : success && pair.keys && pair.values && outcome.ok);
    if(hit) assert(xrtErrorKind(outcome.error)==XERR_MEMORY);
    assert(xl6SlotMapCountV1(map,&layout)==2);
    xstrview read={0}; bool found=false;
    assert(xl6SlotMapReadV1(map,&layout,a,&read,&found) && found && read.Size==5 && !memcmp(read.Data,text.Data,5));
    xrtFree((ptr)read.Data); XL6_SlotMapSnapshotPairV1 retry={0};
    assert(xl6SlotMapPairsV1(map,&layout,0xCA14,xl6StringConverterV1(),&retry)); discardPair(&retry); discardPair(&pair);
    xrtErrorFree(outcome.error); dropKey(&a); dropKey(&b); xrtValueRelease(map); xrtClearError();
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify(); return hit;
}
int main(int argc,char** argv)
{
    assert(argc==2);
    if(!strcmp(argv[1],"vectors") || !strcmp(argv[1],"pairs")) {
        xl6ProductMemoryBegin(SIZE_MAX);
        if(!strcmp(argv[1],"vectors")) { textVectors(); otherDomains(); refusedAndFailed(); }
        textPairs(); failedPairs(); otherPairs();
        assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
        puts("paired snapshot: five domains, one traversal, aligned values, atomic rollback and lifetimes balanced");
    } else {
        assert(!strcmp(argv[1],"prefixes")); size_t at=0;
        while(at<4096 && pairFault(at)) ++at;
        assert(at<4096);
        printf("paired snapshot full prefixes=%zu first-no-hit=%zu balanced\n",at+1,at);
    }
    return 0;
}
