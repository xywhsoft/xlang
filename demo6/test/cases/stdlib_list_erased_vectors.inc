/* Included by the native List host: reuse its actual constructors, codec fault
 * vectors and linker error-boundary controls, not a second test container. */
static bool erasedFaultLastHit;
static void erasedMethods(void)
{
    XL6_ListLayoutV1 tuple=layout(1,xrtTypeInt64());
    xvalue* list=projected(&tuple,XL6_VALUE_ADMIT_I64,NULL); assert(list);
    int64_t count=99,item=7; bool found=true;
    assert(xl6ListErasedCountV1(list,&count) && count==0);
    assert(xl6ListErasedKeyV1(list,INT64_MIN,true,&found) && !found);
    assert(xl6ListSetV1(list,&tuple,INT64_MIN,&item) && xl6ListSetV1(list,&tuple,INT64_MAX,&item));
    assert(xl6ListErasedCountV1(list,&count) && count==2);
    assert(xl6ListErasedKeyV1(list,INT64_MAX,false,&found) && found);
    assert(xl6ListErasedKeyV1(list,INT64_MAX,true,&found) && found);
    assert(xl6ListErasedKeyV1(list,INT64_MAX,true,&found) && !found);
    assert(xl6ListErasedKeyV1(list,INT64_MIN,false,&found) && found);
    xrtSetErrorKind(XERR_ARGUMENT); const xerror* prior=xrtGetError(); assert(prior);
    assert(xl6ListErasedCountV1(list,&count) && count==1 && xrtGetError()==prior);
    xrtClearError();
    assert(xl6ListErasedClearV1(list) && xl6ListErasedClearV1(list));
    assert(xl6ListErasedCountV1(list,&count) && count==0);
    xrtValueRelease(list);
}
static void erasedIterator(void)
{
    XL6_ListLayoutV1 tuple=layout(2,xl6StringTypeV1()),actual={0};
    const int64_t keys[]={INT64_MIN,-9,INT64_MAX}; xstrview text={"a\0\xE4\xBD\xA0",5};
    xvalue* list=projected(&tuple,XL6_VALUE_ADMIT_STRING,xl6StringConverterV1()); assert(list);
    for (unsigned i=0;i<3;++i) assert(xl6ListSetV1(list,&tuple,keys[i],&text));
    xvalue* cursor=NULL;
    assert(xl6ListIteratorCreateErasedV1(list,CURSOR_ID,&cursor,&actual)==1 && cursor);
    assert(actual.identity==tuple.identity && actual.value.identity==tuple.value.identity &&
        actual.value.elementIdentity==ELEMENT_ID && actual.value.transfer==2 &&
        actual.value.size==sizeof(xstrview) && actual.value.type==tuple.value.type);
    assert(xl6ListErasedClearV1(list)); xrtValueRelease(list);
    for (unsigned i=0;i<3;++i) {
        bool found=false; int64_t key=99; xvalue* item=NULL; xstrview read={0};
        assert(xl6ListIteratorAdvanceValueV1(cursor,&actual,CURSOR_ID,&found,&item,&key) && found && item);
        assert(key==keys[i] && xrtValueTypeId(item)==ELEMENT_ID && xrtValueGetString(item,&read));
        assert(read.Size==text.Size && !memcmp(read.Data,text.Data,text.Size)); xrtValueRelease(item);
    }
    for (unsigned i=0;i<2;++i) {
        bool found=true; int64_t key=99; xvalue* item=NULL;
        assert(xl6ListIteratorAdvanceValueV1(cursor,&actual,CURSOR_ID,&found,&item,&key) && !found && !item && key==0);
    }
    xrtValueRelease(cursor);
    /* A large reversed insertion order still yields an O(n) sorted snapshot,
     * not repeated ordinal Get or one whole-storage boxed wire conversion. */
    tuple=layout(1,xrtTypeInt64()); list=projected(&tuple,XL6_VALUE_ADMIT_I64,NULL); assert(list);
    for (int64_t i=2047;i>=0;--i) assert(xl6ListSetV1(list,&tuple,i,&i));
    cursor=NULL; assert(xl6ListIteratorCreateErasedV1(list,CURSOR_ID,&cursor,&actual)==1);
    xrtValueRelease(list);
    for (int64_t i=0;i<2048;++i) {
        bool found=false; int64_t key=-1,read=-1; xvalue* item=NULL;
        assert(xl6ListIteratorAdvanceValueV1(cursor,&actual,CURSOR_ID,&found,&item,&key) && found);
        assert(key==i && xrtValueTypeId(item)==ELEMENT_ID && xrtValueGetInt(item,&read) && read==i);
        xrtValueRelease(item);
    }
    xrtValueRelease(cursor);
}
static void erasedReferences(void)
{
    XL6_ListLayoutV1 tuple=layout(3,xrtTypeValue()),actual={0};
    const unsigned rules[]={XL6_VALUE_ADMIT_ANY,XL6_VALUE_ADMIT_BYTES};
    for (unsigned i=0;i<2;++i) {
        xvalue* list=projected(&tuple,rules[i],NULL),*cursor=NULL;
        const unsigned char bytes[]={0,255,0,9};
        xvalue* value=xrtValueBytes((xbytesview){bytes,sizeof(bytes)}); assert(list && value);
        assert(xl6ListSetV1(list,&tuple,INT64_MIN,&value) && xl6ListSetV1(list,&tuple,INT64_MAX,&value));
        assert(xl6ListIteratorCreateErasedV1(list,CURSOR_ID,&cursor,&actual)==1);
        assert(xl6ListErasedClearV1(list)); xrtValueRelease(list);
        xrtownershipref owner=ownerOf(cursor); Edges slots={0};
        assert(xl6ListOwnershipAdapterV1(owner) && owner.Ops->Trace(owner.Data,edge,&slots) && slots.count==2 && slots.same);
        for (unsigned j=0;j<2;++j) {
            bool found=false; int64_t key=0; xvalue* item=NULL; xbytesview view={0};
            assert(xl6ListIteratorAdvanceValueV1(cursor,&actual,CURSOR_ID,&found,&item,&key) && found);
            assert(item==value && key==(j ? INT64_MAX : INT64_MIN) && xrtValueGetBytes(item,&view));
            assert(view.Size==sizeof(bytes) && !memcmp(view.Data,bytes,sizeof(bytes))); xrtValueRelease(item);
        }
        xrtValueRelease(value); xrtValueRelease(cursor);
    }
}
static void erasedProfiles(void)
{
    XL6_ListLayoutV1 tuple=layout(1,xrtTypeUInt64()),actual={0};
    xvalue* list=projected(&tuple,XL6_VALUE_ADMIT_U64,NULL),*cursor=NULL,*value=NULL; assert(list);
    uint64_t number=UINT64_MAX,readNumber=0; bool found=false; int64_t key=0;
    assert(xl6ListSetV1(list,&tuple,INT64_MAX,&number) && xl6ListIteratorCreateErasedV1(list,CURSOR_ID,&cursor,&actual)==1);
    xrtValueRelease(list);
    assert(xl6ListIteratorAdvanceValueV1(cursor,&actual,CURSOR_ID,&found,&value,&key) && found && key==INT64_MAX);
    assert(xrtValueGetUInt(value,&readNumber) && readNumber==number && xrtValueTypeId(value)==ELEMENT_ID);
    xrtValueRelease(value); xrtValueRelease(cursor);
    tuple=layout(1,xl6CharTypeV1()); list=projected(&tuple,XL6_VALUE_ADMIT_CHAR,xl6CharConverterV1());
    uint32_t character=0x4F60,readChar=0; cursor=NULL; value=NULL; assert(list);
    assert(xl6ListSetV1(list,&tuple,-1,&character) && xl6ListIteratorCreateErasedV1(list,CURSOR_ID,&cursor,&actual)==1);
    xrtValueRelease(list);
    assert(xl6ListIteratorAdvanceValueV1(cursor,&actual,CURSOR_ID,&found,&value,&key) && found && key==-1);
    assert(xrtValueGetChar(value,&readChar) && readChar==character && xrtValueTypeId(value)==ELEMENT_ID);
    xrtValueRelease(value); xrtValueRelease(cursor);
    tuple=layout(2,xl6NullableStringTypeV1()); tuple.value.elementIdentity=ELEMENT_ID+1;
    xvalue* witness=xl6SequencePrimitiveCreateV1(1,ARRAY_ID,ELEMENT_ID+1,2,sizeof(xstrview),
        tuple.value.type,XL6_VALUE_ADMIT_STRING,true,ELEMENT_ID);
    assert(witness); list=xl6ListProjectionTakeV1(xl6ListCreateTakeV1(&tuple,witness),&tuple,xl6NullableStringConverterV1());
    xstrview absent={0},empty={"",0}; cursor=NULL; assert(list);
    assert(xl6ListSetV1(list,&tuple,-1,&absent) && xl6ListSetV1(list,&tuple,42,&empty));
    assert(xl6ListIteratorCreateErasedV1(list,CURSOR_ID,&cursor,&actual)==1); xrtValueRelease(list);
    value=NULL; assert(xl6ListIteratorAdvanceValueV1(cursor,&actual,CURSOR_ID,&found,&value,&key) && found && key==-1);
    assert(xrtValueType(value)==XVALUE_NULL && xrtValueTypeId(value)==0); xrtValueRelease(value);
    value=NULL; xstrview read={0};
    assert(xl6ListIteratorAdvanceValueV1(cursor,&actual,CURSOR_ID,&found,&value,&key) && found && key==42);
    assert(xrtValueTypeId(value)==ELEMENT_ID && xrtValueGetString(value,&read) && read.Data && read.Size==0);
    xrtValueRelease(value); xrtValueRelease(cursor);
    tuple=layout(XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1,xrtTypeFuture()); list=projected(&tuple,0,NULL); cursor=NULL; assert(list);
    xfuture* future=NULL; xpromise* promise=xrtPromiseCreate(&future,NULL); assert(promise && future);
    assert(xl6ListSetV1(list,&tuple,INT64_MIN,&future) && xl6ListIteratorCreateErasedV1(list,CURSOR_ID,&cursor,&actual)==1);
    xrtownershipref owner=ownerOf(cursor); Edges slots={0};
    const XL6_SequenceAdmissionOpsV2* allowed[]={xl6ArrayFutureAdmissionOpsV2Get()};
    assert(!xl6ListOwnershipAdapterV1(owner) && xl6ListOwnershipAdapterV2(owner,allowed,1));
    assert(owner.Ops->Trace(owner.Data,edge,&slots) && slots.count==1 && slots.first.Data==xrtFutureOwnership(future).Data);
    xrtValueRelease(list); xrtPromiseDestroy(promise); xrtFutureDestroy(future);
    value=NULL; assert(xl6ListIteratorAdvanceValueV1(cursor,&actual,CURSOR_ID,&found,&value,&key) && found && key==INT64_MIN);
    assert(xrtValueTypeId(value)==ELEMENT_ID && xrtValueIsFuture(value) && xrtValueGetFuture(value)==future);
    xrtValueRelease(value); xrtValueRelease(cursor);
}
static void erasedRejected(void)
{
    XL6_ListLayoutV1 tuple=layout(1,xrtTypeInt64()),actual={.identity=99};
    xvalue* unbound=create(&tuple,XL6_VALUE_ADMIT_I64),*list=projected(&tuple,XL6_VALUE_ADMIT_I64,NULL);
    assert(unbound && list); int64_t count=99; bool found=true;
    xvalue* cursor=NULL;
    assert(!xl6ListErasedCountV1(unbound,&count) && count==99); xrtClearError();
    assert(xl6ListIteratorCreateErasedV1(unbound,CURSOR_ID,&cursor,&actual)==-1 && !cursor && actual.identity==99); xrtClearError();
    assert(!xl6ListErasedKeyV1(unbound,0,true,&found) && found); xrtClearError();
    assert(!xl6ListErasedClearV1(unbound)); xrtClearError();
    void* raw=(void*)(uintptr_t)1;
    xvalue* foreign=xrtValueHandleTake(&raw,&foreignOps,NULL); assert(foreign && xrtValueTypeIdBind(foreign,LIST_ID));
    xvalue* wire=xrtValueIntMap(); assert(wire && xrtValueTypeIdBind(wire,LIST_ID));
    const xvalue* rejected[]={foreign,wire,xrtValueNull(),NULL};
    for (unsigned i=0;i<4;++i) {
        assert(!xl6ListErasedCountV1(rejected[i],&count) && count==99); xrtClearError();
        assert(!xl6ListErasedKeyV1(rejected[i],0,true,&found) && found); xrtClearError();
        assert(!xl6ListErasedClearV1(rejected[i])); xrtClearError();
        xrtSetErrorKind(XERR_ARGUMENT); const xerror* prior=xrtGetError(); assert(prior);
        assert(xl6ListIteratorCreateErasedV1(rejected[i],CURSOR_ID,&cursor,&actual)==0 && !cursor && actual.identity==99);
        assert(xrtGetError()==prior); xrtClearError();
    }
    const uint64_t invalid[]={0,LIST_ID,ARRAY_ID};
    for (unsigned i=0;i<3;++i) {
        assert(xl6ListIteratorCreateErasedV1(list,invalid[i],&cursor,&actual)==-1 && !cursor && actual.identity==99); xrtClearError();
    }
    assert(xl6ListIteratorCreateErasedV1(list,CURSOR_ID,&cursor,&actual)==1);
    assert(!xl6ListErasedCountV1(cursor,&count) && count==99); xrtClearError();
    xvalue* other=NULL; XL6_ListLayoutV1 untouched={.identity=99};
    assert(xl6ListIteratorCreateErasedV1(cursor,CURSOR_ID+1,&other,&untouched)==-1 && !other && untouched.identity==99); xrtClearError();
    XL6_ListLayoutV1 bad=actual; ++bad.value.elementIdentity; int64_t key=99;
    assert(!xl6ListIteratorAdvanceValueV1(cursor,&bad,CURSOR_ID,&found,&other,&key) && found && !other && key==99); xrtClearError();
    assert(!xl6ListIteratorAdvanceValueV1(list,&actual,CURSOR_ID,&found,&other,&key) && found && !other && key==99); xrtClearError();
    assert(!xl6ListIteratorAdvanceValueV1(cursor,&actual,CURSOR_ID+1,&found,&other,&key) && found && !other && key==99); xrtClearError();
    assert(!xl6ListErasedCountV1(list,NULL)); xrtClearError();
    assert(!xl6ListErasedKeyV1(list,0,false,NULL)); xrtClearError();
    assert(xl6ListIteratorCreateErasedV1(list,CURSOR_ID,NULL,&untouched)==-1 && untouched.identity==99); xrtClearError();
    assert(xl6ListIteratorCreateErasedV1(list,CURSOR_ID,&other,NULL)==-1 && !other); xrtClearError();
    xrtValueRelease(cursor); xrtValueRelease(wire); xrtValueRelease(foreign); xrtValueRelease(unbound); xrtValueRelease(list);
}
static void erasedErrors(void)
{
    XL6_ListLayoutV1 tuple=layout(2,xl6StringTypeV1()),actual={0}; xstrview text={"a\0b",3};
    for (unsigned absent=0;absent<2;++absent) {
        codecFaultMode=0; codecFaultNull=absent;
        xvalue* list=projected(&tuple,XL6_VALUE_ADMIT_STRING,&faultCodec),*cursor=NULL; assert(list);
        assert(xl6ListSetV1(list,&tuple,-1,&text) && xl6ListSetV1(list,&tuple,42,&text));
        assert(xl6ListIteratorCreateErasedV1(list,CURSOR_ID,&cursor,&actual)==1);
        xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
        codecFaultMode=2; codecCalls=0; bool found=true; int64_t key=99; xvalue* item=NULL;
        assert(!xl6ListIteratorAdvanceValueV1(cursor,&actual,CURSOR_ID,&found,&item,&key) && found && !item && key==99 && codecCalls==1);
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope); assert(!outcome.ok);
        assert(absent ? !outcome.error : outcome.error && xrtErrorKind(outcome.error)==XERR_VALUE); xrtErrorFree(outcome.error);
        codecFaultMode=0;
        assert(!xl6ListIteratorAdvanceValueV1(cursor,&actual,CURSOR_ID,&found,&item,&key) && found && !item && key==99);
        assert(xrtErrorKind(xrtGetError())==XERR_STATE); xrtClearError();
        readText(list,-1,text); readText(list,42,text); xrtValueRelease(cursor); xrtValueRelease(list);
    }
    codecFaultMode=0; codecFaultNull=false;
    /* Callback clears its reentrant refusal diagnostic: the independent failure
     * bit must still prevent publication and permanently retire advancement. */
    xvalue* list=projected(&tuple,XL6_VALUE_ADMIT_STRING,&faultCodec),*cursor=NULL; assert(list);
    assert(xl6ListSetV1(list,&tuple,-1,&text) && xl6ListIteratorCreateErasedV1(list,CURSOR_ID,&cursor,&actual)==1);
    codecSource=cursor; codecFaultMode=5; bool found=true; int64_t key=99; xvalue* item=NULL;
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
    assert(!xl6ListIteratorAdvanceValueV1(cursor,&actual,CURSOR_ID,&found,&item,&key) && !item && found && key==99);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope); assert(!outcome.ok); xrtErrorFree(outcome.error);
    codecFaultMode=0; codecSource=NULL; readText(list,-1,text); xrtValueRelease(cursor); xrtValueRelease(list);
    /* Entry refusal is not a fake EOF and must leave source and all outputs. */
    for (unsigned absent=0;absent<2;++absent) {
        list=projected(&tuple,XL6_VALUE_ADMIT_STRING,xl6StringConverterV1()); cursor=NULL; assert(list);
        actual=(XL6_ListLayoutV1){.identity=99};
        assert(xl6ErrorScopeBegin(&scope)); entryFailure=absent ? -1 : 1;
        assert(xl6ListIteratorCreateErasedV1(list,CURSOR_ID,&cursor,&actual)==-1 && !cursor && actual.identity==99);
        xl6ErrorScopeCaptureFailure(&scope); /* Explicit checked status when Begin cannot install a child scope. */
        outcome=xl6ErrorScopeEnd(&scope); assert(!outcome.ok);
        assert(absent ? !outcome.error : outcome.error && xrtErrorKind(outcome.error)==XERR_MEMORY); xrtErrorFree(outcome.error);
        xrtValueRelease(list);
    }
}
static bool erasedFault(const char* operation,size_t point)
{
    if (strncmp(operation,"erased-",7)) return false;
    XL6_ListLayoutV1 tuple=layout(2,xl6StringTypeV1()),actual={.identity=99};
    xstrview text={"a\0\xE4\xBD\xA0",5};
    xvalue* list=projected(&tuple,XL6_VALUE_ADMIT_STRING,xl6StringConverterV1()),*cursor=NULL,*item=NULL;
    assert(list && xl6ListSetV1(list,&tuple,-7,&text) && xl6ListSetV1(list,&tuple,42,&text));
    bool step=!strcmp(operation,"erased-step");
    if (step) assert(xl6ListIteratorCreateErasedV1(list,CURSOR_ID,&cursor,&actual)==1);
    bool found=true,success=false; int64_t key=99,count=99;
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope)); assert(xrtMemDebugFailAfter(point));
    if (step) success=xl6ListIteratorAdvanceValueV1(cursor,&actual,CURSOR_ID,&found,&item,&key);
    else if (!strcmp(operation,"erased-cursor")) success=xl6ListIteratorCreateErasedV1(list,CURSOR_ID,&cursor,&actual)==1;
    else if (!strcmp(operation,"erased-count")) success=xl6ListErasedCountV1(list,&count);
    else if (!strcmp(operation,"erased-key")) success=xl6ListErasedKeyV1(list,42,false,&found);
    else if (!strcmp(operation,"erased-remove")) success=xl6ListErasedKeyV1(list,42,true,&found);
    else if (!strcmp(operation,"erased-clear")) success=xl6ListErasedClearV1(list);
    else assert(0);
    bool hit=xrtMemDebugFailTriggered(); erasedFaultLastHit=hit; xrtMemDebugFailClear();
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(hit ? !success && !outcome.ok : success && outcome.ok);
    assert(!hit || (outcome.error && xrtErrorKind(outcome.error)==XERR_MEMORY));
    if (!strcmp(operation,"erased-cursor")) assert(hit ? !cursor && actual.identity==99 : cursor && actual.identity==LIST_ID);
    if (!strcmp(operation,"erased-count")) assert(count==(hit ? 99 : 2));
    if (step) {
        assert(hit ? found && !item && key==99 : found && item && key==-7);
        if (item) { xstrview read={0}; assert(xrtValueGetString(item,&read) && read.Size==text.Size && !memcmp(read.Data,text.Data,text.Size)); }
        if (hit) {
            bool retryFound=true; int64_t retryKey=99; xvalue* retry=NULL;
            assert(!xl6ListIteratorAdvanceValueV1(cursor,&actual,CURSOR_ID,&retryFound,&retry,&retryKey) && retryFound && !retry && retryKey==99);
            assert(xrtErrorKind(xrtGetError())==XERR_STATE); xrtClearError();
        }
    }
    int64_t expected=!hit && !strcmp(operation,"erased-clear") ? 0 : !hit && !strcmp(operation,"erased-remove") ? 1 : 2;
    assert(xl6ListCountV1(list,&tuple)==expected);
    if (expected) readText(list,-7,text);
    if (expected==2) readText(list,42,text);
    xrtErrorFree(outcome.error); xrtValueRelease(item); xrtValueRelease(cursor); xrtValueRelease(list);
    printf("native List %s point=%zu hit=%d complete-prefix balanced\n",operation,point,hit); return true;
}
