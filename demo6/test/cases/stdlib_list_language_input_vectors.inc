/* Resident input contract shared by real language producers and native C. */
static xvalue* languageLeaf(unsigned profile)
{
    if (profile!=6) return leaf(profile,false,true);
    xvalue* value=xrtValueFloat((double)(float)0.1);
    assert(value && xrtValueTypeIdBind(value,LEAF)); return value;
}
static bool languagePut(xvalue* target,size_t depth,int64_t key,const xvalue* value,bool native)
{
    return depth%2 ? (native ? xl6ArrayErasedPushV1(target,value) : xrtValueArrayAppend(target,value)) :
        (native ? xl6ListErasedSetV1(target,key,value) : xrtValueIntMapSet(target,key,value));
}
static xvalue* languageSource(size_t count,unsigned profile,bool native,size_t width)
{
    xvalue* values[MAX]={0}; const XL6_CollectionImportProviderV1* providers[MAX]={0};
    if (native) seeds(values,providers,count,profile);
    for (size_t at=count;at>0;--at) {
        size_t i=at-1;
        if (!native) values[i]=i%2 ? xrtValueArray() : xrtValueIntMap();
        assert(values[i]);
        xvalue* child=i+1<count ? values[i+1] : languageLeaf(profile);
        assert(languagePut(values[i],i,i%2 ? 0 : INT64_MIN,child,native));
        if (i+1<count || nullable(profile)) {
            assert(languagePut(values[i],i,i%2 ? 1 : INT64_MAX,
                i+1<count ? child : xrtValueNull(),native));
        }
        if (i+1==count) xrtValueRelease(child);
        else { xrtValueRelease(values[i+1]); values[i+1]=NULL; }
    }
    if (width>1) {
        assert(count==3 && !native); xrtValueRelease(values[0]); values[0]=xrtValueIntMap();
        for (size_t i=0;i<width;++i) {
            xvalue* child=languageSource(3,profile,false,1); xvalue* middle=xrtValueIntMapGet(child,INT64_MIN);
            assert(middle && xrtValueIntMapSet(values[0],(int64_t)(UINT64_C(1)<<40)+2*(int64_t)i,middle) &&
                xrtValueIntMapSet(values[0],(int64_t)(UINT64_C(1)<<40)+2*(int64_t)i+1,middle));
            xrtValueRelease(child);
        }
    }
    return values[0];
}
static XL6_CollectionImportPlanV1 languagePlan(const XL6_CollectionImportProviderV1* const* providers)
{
    return (XL6_CollectionImportPlanV1){.size=sizeof(XL6_CollectionImportPlanV1),
        .providers=providers,.flags=XL6_COLLECTION_IMPORT_LANGUAGE_INPUT_V1};
}
static void languageObserved(const xvalue* value,size_t count,unsigned profile)
{
    xvalue* current=xrtValueRetain(value);
    for (size_t i=0;i+1<count;++i) {
        xvalue* a=item(current,i,i%2 ? 0 : INT64_MIN),*b=item(current,i,i%2 ? 1 : INT64_MAX);
        assert(a && a==b && a!=current); xrtValueRelease(current); xrtValueRelease(b); current=a;
    }
    xvalue* actual=item(current,count-1,(count-1)%2 ? 0 : INT64_MIN),*expected=languageLeaf(profile);
    assert(actual && xrtValueEqual(actual,expected) && xrtValueTypeId(actual)==xrtValueTypeId(expected));
    xrtValueRelease(actual); xrtValueRelease(expected);
    if (nullable(profile)) {
        actual=item(current,count-1,(count-1)%2 ? 1 : INT64_MAX);
        assert(xrtValueType(actual)==XVALUE_NULL); xrtValueRelease(actual);
    }
    xrtValueRelease(current);
}
static void languageProfiles(void)
{
    for (unsigned profile=0;profile<9;++profile) for (unsigned native=0;native<2;++native) {
        xvalue* values[MAX]={0}; const XL6_CollectionImportProviderV1* providers[MAX]={0};
        seeds(values,providers,1,profile); xvalue* source=languageSource(1,profile,native,1);
        XL6_CollectionImportPlanV1 p=languagePlan(providers);
        xrtSetErrorKind(XERR_TIMEOUT); const xerror* prior=xrtGetError();
        xvalue* result=xl6CollectionImportChainTakeV1(values,1,source,&p);
        assert(result && result!=source && !values[0] && xrtGetError()==prior); xrtClearError();
        languageObserved(result,1,profile); assert(xrtValueType(source)==(native ? XVALUE_HANDLE : XVALUE_INT_MAP));
        if (!native) assert(xrtValueTypeId(xrtValueIntMapGet(source,INT64_MIN))==(profile==8 ? 0 : LEAF));
        xrtValueRelease(result); xrtValueRelease(source);
    }
    for (unsigned native=0;native<2;++native) {
        xvalue* values[MAX]={0}; const XL6_CollectionImportProviderV1* providers[MAX]={0};
        seeds(values,providers,1,1); xvalue* source;
        if (native) {
            xvalue* input[MAX]={0}; const XL6_CollectionImportProviderV1* ignored[MAX]={0};
            seeds(input,ignored,1,1); source=input[0];
        } else source=xrtValueIntMap();
        XL6_CollectionImportPlanV1 p=languagePlan(providers);
        xvalue* result=xl6CollectionImportChainTakeV1(values,1,source,&p); int64_t n=-1;
        assert(result && result!=source && xl6ListErasedCountV1(result,&n) && !n);
        xrtValueRelease(result); xrtValueRelease(source);
    }
}
static void languageMixed(void)
{
    for (size_t count=2;count<=4;++count) for (unsigned native=0;native<2;++native) {
        xvalue* values[MAX]={0}; const XL6_CollectionImportProviderV1* providers[MAX]={0};
        seeds(values,providers,count,1); xvalue* source=languageSource(count,1,native,1);
        XL6_Sequence* busy=NULL; xvalue* pinned=NULL;
        if (native && count==2) {
            XL6_SequenceLayoutV1 layout={0}; const xvalueconverter* converter=NULL;
            pinned=item(source,0,INT64_MIN);
            assert(xl6SequenceBeginErasedArrayV1(pinned,&busy,&layout,&converter)==1);
        }
        XL6_CollectionImportPlanV1 p=languagePlan(providers);
        xvalue* result=xl6CollectionImportChainTakeV1(values,count,source,&p); assert(result && result!=source);
        if (busy) xl6SequenceEndV1(busy);
        xrtValueRelease(pinned);
        languageObserved(result,count,1);
        if (native) {
            xvalue* a=item(result,0,INT64_MIN),*b=item(source,0,INT64_MIN); assert(a==b);
            xrtValueRelease(a); xrtValueRelease(b);
        } else assert(xrtValueType(xrtValueIntMapGet(source,INT64_MIN))==XVALUE_ARRAY);
        xrtValueRelease(result); xrtValueRelease(source);
    }
    xvalue* values[MAX]={0}; const XL6_CollectionImportProviderV1* providers[MAX]={0};
    seeds(values,providers,3,1); xvalue* source=languageSource(3,1,false,40);
    XL6_CollectionImportPlanV1 p=languagePlan(providers);
    xvalue* result=xl6CollectionImportChainTakeV1(values,3,source,&p); assert(result);
    xvalue* prior=NULL;
    for (int64_t i=0;i<40;++i) {
        int64_t key=(int64_t)(UINT64_C(1)<<40)+2*i;
        xvalue* a=item(result,0,key),*b=item(result,0,key+1); assert(a && a==b && a!=prior);
        xrtValueRelease(prior); xrtValueRelease(b); prior=a;
    }
    xrtValueRelease(prior); xrtValueRelease(result); xrtValueRelease(source);
    /* Authentic projected ANY Arrays are language erasure boundaries, not
     * wrong typed children. All leaf profiles import one independent shared
     * child through the SAME memo; ordinary typed children above stay aliases. */
    for (unsigned profile=0;profile<9;++profile) {
        xvalue* targets[MAX]={0}; const XL6_CollectionImportProviderV1* adapters[MAX]={0};
        seeds(targets,adapters,2,profile);
        uint64_t anyOwner=UINT64_C(0xF991),anyElement=UINT64_C(0xF992);
        xvalue* any=xl6SequenceReferenceCreateV1(1,anyOwner,anyElement,XL6_VALUE_ADMIT_ANY,false,anyElement);
        any=xl6ArrayProjectionTakeV1(any,anyOwner,anyElement,3,sizeof(xvalue*),xrtTypeValue(),NULL);
        xvalue* value=languageLeaf(profile); assert(any && value && xl6ArrayErasedPushV1(any,value));
        xrtValueRelease(value);
        if (nullable(profile)) assert(xl6ArrayErasedPushV1(any,xrtValueNull()));
        xvalue* raw=xrtValueIntMap(); assert(raw && xrtValueIntMapSet(raw,INT64_MIN,any) && xrtValueIntMapSet(raw,INT64_MAX,any));
        XL6_CollectionImportPlanV1 policy=languagePlan(adapters);
        xvalue* imported=xl6CollectionImportChainTakeV1(targets,2,raw,&policy); assert(imported);
        xvalue* a=item(imported,0,INT64_MIN),*b=item(imported,0,INT64_MAX);
        assert(a && a==b && a!=any); languageObserved(imported,2,profile);
        xrtValueRelease(a); xrtValueRelease(b); xrtValueRelease(imported); xrtValueRelease(raw); xrtValueRelease(any);
    }
}
static void languageRejected(void)
{
    for (unsigned variant=0;variant<13;++variant) {
        xvalue* values[MAX]={0}; const XL6_CollectionImportProviderV1* providers[MAX]={0};
        seeds(values,providers,1,1); xvalue* source=NULL;
        XL6_CollectionImportProviderV1 adapter=*xl6ListImportProviderV1();
        XL6_CollectionImportPlanV1 p=languagePlan(providers); XL6_ListGraphViewV1 pin={0};
        if (variant==0) source=xrtValueArray();
        else if (variant==1) source=xrtValueObject();
        else if (variant==2) source=xrtValueString(XRT_STR_LITERAL("not a List"));
        else if (variant==3) {
            ptr raw=&drops; source=xrtValueHandleTake(&raw,&failingOps,NULL);
            assert(source && xrtValueTypeIdBind(source,OWNER(0)));
        } else if (variant==4) {
            source=languageSource(1,1,true,1); void* raw=NULL; void* user=NULL; const xvaluehandleops* ops=NULL;
            assert(xrtValueGetHandle(source,&raw,&ops,&user) && xl6ListGraphBeginV1(ops,raw,user,OWNER(0),&pin)==1);
        } else source=languageSource(1,1,false,1);
        if (variant==5) { adapter.InputBegin=NULL; providers[0]=&adapter; }
        else if (variant==6) { adapter.InputEnd=NULL; providers[0]=&adapter; }
        else if (variant==7) { adapter.InputBegin=NULL; adapter.InputEnd=NULL; providers[0]=&adapter; }
        else if (variant==8) p.flags|=XL6_COLLECTION_IMPORT_RESTORE_V1;
        else if (variant==9) p.flags|=XL6_COLLECTION_IMPORT_JSON_V1;
        else if (variant==10) { p.leafIdentity=LEAF; }
        else if (variant==11) {
            xvalue* invalid=xrtValueString(XRT_STR_LITERAL("different nominal domain"));
            assert(invalid && xrtValueTypeIdBind(invalid,LEAF+1) && xrtValueIntMapSetNew(source,INT64_MAX,invalid));
        } else if (variant==12) {
            xrtValueRelease(values[0]); values[0]=xrtValueIntMap(); assert(xrtValueTypeIdBind(values[0],OWNER(0)));
        }
        assert(source && !xl6CollectionImportChainTakeV1(values,1,source,&p) && !values[0] && xrtGetError());
        if (variant==3) assert(xrtErrorKind(xrtGetError())==XERR_TYPE);
        xrtClearError(); if (pin.owner) xl6ListGraphEndV1(&pin);
        if (variant==3) { xrtValueRelease(source); assert(xrtErrorKind(xrtGetError())==XERR_VALUE); xrtClearError(); }
        else xrtValueRelease(source);
    }
    /* Compact persistence arrays and language sparse maps are never silently
     * interchangeable, even with the same target schema. */
    for (unsigned language=0;language<2;++language) {
        xvalue* values[MAX]={0}; const XL6_CollectionImportProviderV1* providers[MAX]={0};
        seeds(values,providers,1,1);
        xvalue* source=language ? wire(1,1,false,true,1) : languageSource(1,1,false,1);
        XL6_CollectionImportPlanV1 p=language ? languagePlan(providers) : plan(providers,1,false,false);
        assert(!xl6CollectionImportChainTakeV1(values,1,source,&p) && !values[0] && xrtGetError());
        xrtClearError(); xrtValueRelease(source);
    }
}
static unsigned inputFailure,inputEnds;
static void languageSnapshot(void)
{
    const XL6_CollectionImportProviderV1* provider=xl6ListImportProviderV1();
    for (unsigned native=0;native<2;++native) {
        xvalue* source=languageSource(1,1,native,1); XL6_CollectionImportInputV1 input={0};
        assert(provider->InputBegin(source,&input) && input.count==1 && input.keys[0]==INT64_MIN);
        xvalue* changed=xrtValueString(XRT_STR_LITERAL("changed")); assert(changed && xrtValueTypeIdBind(changed,LEAF));
        assert(native ? xl6ListErasedSetV1(source,INT64_MIN,changed) : xrtValueIntMapSet(source,INT64_MIN,changed));
        xrtValueRelease(changed); xrtValueRelease(source);
        xstrview text={0}; assert(xrtValueGetString(input.items[0],&text) && text.Size==5 && !memcmp(text.Data,"a\0\xE4\xBD\xA0",5));
        assert(!provider->InputBegin(NULL,&input) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT); xrtClearError();
        provider->InputEnd(&input); assert(!input.context && !input.items && !input.keys && !input.count);
        provider->InputEnd(&input); assert(!xrtGetError());
    }
    XL6_CollectionImportInputV1 input={0}; xvalue* wrong=xrtValueArray(); assert(wrong);
    assert(!provider->InputBegin(wrong,&input) && input.context && !input.items && !input.count);
    assert(xrtErrorKind(xrtGetError())==XERR_TYPE); const xerror* prior=xrtGetError();
    provider->InputEnd(&input); assert(!input.context && xrtGetError()==prior); xrtClearError(); xrtValueRelease(wrong);
}
static bool failedInputBegin(const xvalue* source,XL6_CollectionImportInputV1* output)
{
    bool ok=xl6ListImportProviderV1()->InputBegin(source,output);
    if (ok && inputFailure==1) return false; /* Deliberately no diagnostic. */
    if (ok && inputFailure==2) xrtSetErrorKind(XERR_TIMEOUT);
    if (ok && inputFailure==3) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    return ok;
}
static void failedInputEnd(XL6_CollectionImportInputV1* output)
{
    ++inputEnds; xl6ListImportProviderV1()->InputEnd(output);
    if (inputFailure) xrtSetErrorKind(XERR_VALUE);
}
static bool failedInputAppend(xvalue* target,int64_t key,const xvalue* value)
{
    bool ok=xl6ListImportProviderV1()->Append(target,key,value);
    if (ok && inputFailure==5) return false;
    if (ok && inputFailure==6) { xrtSetErrorKind(XERR_TIMEOUT); return false; }
    if (ok && inputFailure==7) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    return ok;
}
static void languageInputErrors(void)
{
    for (inputFailure=1;inputFailure<=7;++inputFailure) {
        xvalue* values[MAX]={0}; const XL6_CollectionImportProviderV1* providers[MAX]={0};
        seeds(values,providers,1,1); xvalue* source=languageSource(1,1,false,1);
        XL6_CollectionImportProviderV1 adapter=*xl6ListImportProviderV1();
        adapter.InputBegin=failedInputBegin; adapter.InputEnd=failedInputEnd;
        adapter.Append=failedInputAppend; providers[0]=&adapter;
        XL6_CollectionImportPlanV1 p=languagePlan(providers); xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
        unsigned before=inputEnds;
        assert(!xl6CollectionImportChainTakeV1(values,1,source,&p) && !values[0] && inputEnds==before+1);
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope); assert(!outcome.ok);
        assert(inputFailure==2 || inputFailure==4 || inputFailure==6 ? outcome.error &&
            xrtErrorKind(outcome.error)==(inputFailure==4 ? XERR_VALUE : XERR_TIMEOUT) : !outcome.error);
        xrtErrorFree(outcome.error); xrtClearError(); assert(xrtValueCount(source)==1); xrtValueRelease(source);
    }
    inputFailure=0;
}
static bool languageFault(const char* name,size_t point)
{
    if (strncmp(name,"restore-language-",17)) return false;
    bool native=!strcmp(name,"restore-language-native"),take=!strcmp(name,"restore-language-take"),
        wide=!strcmp(name,"restore-language-wide");
    assert(native || take || wide || !strcmp(name,"restore-language-wire"));
    xvalue* values[MAX]={0}; const XL6_CollectionImportProviderV1* providers[MAX]={0};
    seeds(values,providers,3,1); xvalue* source=languageSource(3,1,native,wide ? 40 : 1);
    xvalue* owned=take ? xrtValueRetain(source) : NULL; XL6_CollectionImportPlanV1 p=languagePlan(providers);
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope)); assert(xrtMemDebugFailAfter(point));
    xvalue* result=take ? xl6CollectionImportChainSourceTakeV1(values,3,owned,&p) :
        xl6CollectionImportChainTakeV1(values,3,source,&p);
    bool hit=xrtMemDebugFailTriggered(); restoreFaultLastHit=hit;
    xrtMemDebugFailClear(); xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(hit ? !result && !outcome.ok : result && outcome.ok);
    for (size_t i=0;i<3;++i) assert(!values[i]);
    int64_t n=-1; assert(native ? xl6ListErasedCountV1(source,&n) && n==2 : xrtValueCount(source)==(wide ? 80u : 2u));
    if (!hit && !wide) languageObserved(result,3,1);
    xrtErrorFree(outcome.error); xrtValueRelease(result); xrtValueRelease(source); xrtClearError();
    printf("native List %s point=%zu hit=%d complete-prefix balanced\n",name,point,hit); return true;
}
