#define XL6_GRAPH_FIXTURE_MAIN xl6PersistenceGraphFixtureMain
#include "test/cases/stdlib_array_graph_native_host.c"
#include "src/runtime/xl6_persistence_project.h"

static const XL6_PersistenceFieldV1 projectedFields[]={
    {XRT_STR_LITERAL("native"),XRT_STR_LITERAL("n\0ative"),0},
    {XRT_STR_LITERAL("again"),XRT_STR_LITERAL("alias"),0},
    {XRT_STR_LITERAL("text"),XRT_STR_LITERAL("text"),0},
    {XRT_STR_LITERAL("missing"),XRT_STR_LITERAL("missing"),0},
    {XRT_STR_LITERAL("null"),XRT_STR_LITERAL("skipNull"),XL6_PERSIST_OMIT_NULL_V1},
    {XRT_STR_LITERAL("empty"),XRT_STR_LITERAL("skipEmpty"),XL6_PERSIST_OMIT_EMPTY_V1},
    {XRT_STR_LITERAL("zero"),XRT_STR_LITERAL("skipZero"),XL6_PERSIST_OMIT_EMPTY_V1}
};
#if XL6_PROJECT_OPEN
static void dropFields(ptr raw,ptr user)
{ assert(!user); xrtDynamicFieldsUnref(raw); }
static const xvaluehandleops fieldsOps={NULL,dropFields,NULL,NULL};
static const XL6_PersistenceOpenV1 openFields={XRT_STR_LITERAL("storage"),XRT_STR_LITERAL("handle"),&fieldsOps};
static const XL6_PersistenceOpenV1 mutateOpen={XRT_STR_LITERAL("unused"),XRT_STR_LITERAL("unused"),&fieldsOps};
static xrtdynamicfields* mutationFields;
static bool projectMutating(const XL6_PersistenceOpenV1* open,xvaluegraphnextv1* next,xvalue* result)
{
    if (open!=&mutateOpen) return xl6PersistenceProjectOpenV1(open,next,result);
    assert(mutationFields && !xrtDynamicFieldsRemove(mutationFields,XRT_STR_LITERAL("victim")));
    assert(xrtGetError() && xrtErrorKind(xrtGetError())==XERR_STATE);
    return false;
}
#endif
static const XL6_PersistenceSchemaV1 schemas[]={
    {7001,0,sizeof(projectedFields)/sizeof(*projectedFields),projectedFields,NULL},
    {7002,XL6_PERSIST_SCHEMA_DISABLED_V1,0,NULL,NULL},
#if XL6_PROJECT_OPEN
    {7003,0,sizeof(projectedFields)/sizeof(*projectedFields),projectedFields,&openFields}
#endif
};
static const xvaluegraphsequencev1 sequences[]={{7006,XVALUE_INT_MAP},{7007,XVALUE_SET}};
static const XL6_PersistencePlanV1 plan={sizeof(plan),sizeof(schemas)/sizeof(*schemas),schemas,
#if XL6_PROJECT_OPEN
    xl6PersistenceProjectOpenV1,
#else
    NULL,
#endif
    sequences,sizeof(sequences)/sizeof(*sequences)
};

static xvalue* nominal(uint64_t identity)
{
    xvalue* result=xrtValueObject(); assert(result && xrtValueTypeIdBind(result,identity));
    assert(xrtValueObjectSetNew(result,XRT_STR_LITERAL("$xlir.class.runtime-type"),xrtValueInt(42)));
    return result;
}
static xvalue* projectSource(xvalue* native, bool open)
{
    xvalue* result=nominal(open ? 7003 : 7001),*hidden=nominal(7002),*empty=seed(true);
    assert(hidden && empty);
    assert(xrtValueObjectSet(result,XRT_STR_LITERAL("native"),native));
    assert(xrtValueObjectSet(result,XRT_STR_LITERAL("again"),native));
    assert(xrtValueObjectSetNew(result,XRT_STR_LITERAL("text"),xrtValueString(XRT_STR_LITERAL("a\0你"))));
    assert(xrtValueObjectSet(result,XRT_STR_LITERAL("null"),xrtValueNull()));
    assert(xrtValueObjectSet(result,XRT_STR_LITERAL("empty"),empty));
    assert(xrtValueObjectSetNew(result,XRT_STR_LITERAL("zero"),xrtValueInt(0)));
    assert(xrtValueObjectSet(result,XRT_STR_LITERAL("private"),hidden));
    xrtValueRelease(hidden); xrtValueRelease(empty);
#if XL6_PROJECT_OPEN
    if (open) {
        xrtdynamicfields* fields=xrtDynamicFieldsCreate(); assert(fields);
        assert(xrtDynamicFieldsSetRef(fields,XRT_STR_LITERAL("extra"),native));
        /* Declared fields win after extras, including renamed target keys. */
        assert(xrtDynamicFieldsSetRefNew(fields,XRT_STR_LITERAL("text"),xrtValueInt(99)));
        assert(xrtDynamicFieldsSetRefNew(fields,XRT_STR_LITERAL("e\0xtra"),xrtValueString(XRT_STR_LITERAL("\0你"))));
        ptr raw=fields; xvalue* handle=xrtValueHandleTake(&raw,&fieldsOps,NULL),*storage=xrtValueObject();
        assert(handle && !raw && storage);
        assert(xrtValueObjectSetTake(storage,XRT_STR_LITERAL("handle"),&handle));
        assert(xrtValueObjectSetTake(result,XRT_STR_LITERAL("storage"),&storage));
    }
#else
    assert(!open);
#endif
    return result;
}
static xvalue* projectRoot(xvalue* native, bool open, bool wide)
{
    xvalue* result=xrtValueObject(),*child=projectSource(native,open); assert(result);
    assert(xrtValueObjectSet(result,XRT_STR_LITERAL("class"),child));
    assert(xrtValueObjectSet(result,XRT_STR_LITERAL("same"),child));
    assert(xrtValueObjectSet(result,XRT_STR_LITERAL("native"),native));
    xvalue* list=xrtValueIntMap(),*set=xrtValueSet(),*plain=xrtValueIntMap();
    assert(list && set && plain && xrtValueTypeIdBind(list,7006) && xrtValueTypeIdBind(set,7007));
    assert(xrtValueIntMapSet(list,-3,native) && xrtValueIntMapSet(list,9,child));
    assert(xrtValueSetAddNew(set,xrtValueInt(1)) && xrtValueSetAddNew(set,xrtValueInt(2)));
    assert(xrtValueIntMapSet(plain,4,native));
    assert(xrtValueObjectSetTake(result,XRT_STR_LITERAL("list"),&list));
    assert(xrtValueObjectSetTake(result,XRT_STR_LITERAL("set"),&set));
    assert(xrtValueObjectSetTake(result,XRT_STR_LITERAL("intMap"),&plain));
    xrtValueRelease(child);
    if (wide) {
        xvalue* values=xrtValueArray(); assert(values);
        for (size_t i=0;i<70;++i) {
            child=projectSource(native,open);
            assert(xrtValueArrayAppend(values,child) && xrtValueArrayAppend(values,child)); xrtValueRelease(child);
        }
        assert(xrtValueObjectSetTake(result,XRT_STR_LITERAL("wide"),&values));
    }
    return result;
}
static void projectionCheck(const xvalue* result,bool open,bool wide)
{
    const xvalue* object=xrtValueObjectGet(result,XRT_STR_LITERAL("class"));
    assert(object && object==xrtValueObjectGet(result,XRT_STR_LITERAL("same")) && xrtValueTypeId(object)==0);
    const xvalue* native=xrtValueObjectGet(result,XRT_STR_LITERAL("native"));
    assert(native && xrtValueType(native)==XVALUE_ARRAY && xrtValueCount(native)==2);
    assert(native==xrtValueObjectGet(object,XRT_STR_LITERAL("n\0ative")) && native==xrtValueObjectGet(object,XRT_STR_LITERAL("alias")));
    const xvalue* list=xrtValueObjectGet(result,XRT_STR_LITERAL("list"));
    const xvalue* set=xrtValueObjectGet(result,XRT_STR_LITERAL("set"));
    const xvalue* plain=xrtValueObjectGet(result,XRT_STR_LITERAL("intMap"));
    assert(xrtValueType(list)==XVALUE_ARRAY && xrtValueCount(list)==2 &&
        xrtValueArrayAt(list,0)==native && xrtValueArrayAt(list,1)==object);
    assert(xrtValueType(set)==XVALUE_ARRAY && xrtValueCount(set)==2);
    assert(xrtValueType(plain)==XVALUE_INT_MAP && xrtValueIntMapGet(plain,4)==native);
    assert(xrtValueObjectGet(object,XRT_STR_LITERAL("missing"))==xrtValueNull());
    assert(!xrtValueObjectGet(object,XRT_STR_LITERAL("private")) && !xrtValueObjectGet(object,XRT_STR_LITERAL("$xlir.class.runtime-type")));
    assert(!xrtValueObjectGet(object,XRT_STR_LITERAL("skipNull")) && !xrtValueObjectGet(object,XRT_STR_LITERAL("skipEmpty")) && !xrtValueObjectGet(object,XRT_STR_LITERAL("skipZero")));
    xstrview text={0}; assert(xrtValueGetString(xrtValueObjectGet(object,XRT_STR_LITERAL("text")),&text));
    assert(text.Size==5 && !memcmp(text.Data,"a\0你",5));
#if XL6_PROJECT_OPEN
    if (open) {
        assert(native==xrtValueObjectGet(object,XRT_STR_LITERAL("extra")));
        assert(xrtValueGetString(xrtValueObjectGet(object,XRT_STR_LITERAL("e\0xtra")),&text));
        assert(text.Size==4 && !memcmp(text.Data,"\0你",4));
    }
#else
    assert(!open);
#endif
    if (wide) {
        const xvalue* values=xrtValueObjectGet(result,XRT_STR_LITERAL("wide")); assert(xrtValueCount(values)==140);
        for (size_t i=0;i<140;i+=2) {
            const xvalue* child=xrtValueArrayAt(values,i);
            assert(child==xrtValueArrayAt(values,i+1) && native==xrtValueObjectGet(child,XRT_STR_LITERAL("n\0ative")));
        }
    }
}
static void projectVectors(void)
{
    xvalue* native=graphSource(),*source=projectRoot(native,XL6_PROJECT_OPEN,false);
    xrtSetErrorKind(XERR_IO); const xerror* prior=xrtGetError(); assert(prior);
    xvalue* result=xl6PersistenceProjectV1(source,&plan); assert(result && xrtGetError()==prior); xrtClearError();
    projectionCheck(result,XL6_PROJECT_OPEN,false); observed(native); xrtValueRelease(result);
#if XL6_FACTORY_PROFILE != 7 && XL6_FACTORY_PROFILE != 8
    result=xl6PersistenceProjectV1(source,&plan); assert(result);
    size_t length=0; char* text=xrtJsonStringify(xrtValueObjectGet(result,XRT_STR_LITERAL("list")),false,&length);
    assert(text && length); xrtFree(text);
    /* An ordinary IntMap remains an extended Value container, not a List.
     * Default JSON refuses it; explicit compatibility mapping accepts it. */
    assert(!xrtJsonStringify(result,false,&length)); error(XERR_UNSUPPORTED);
    xjsonwriteconfig config; xrtJsonWriteConfigInit(&config); config.Flags|=XJSON_WRITE_CONTAINER_COMPAT;
    xjsonwriter* writer=xrtJsonWriterCreate(&config); assert(writer);
    assert(xrtJsonWriterValue(writer,result) && xrtJsonWriterFinish(writer));
    text=xrtJsonWriterTake(writer,&length); assert(text && length); xrtFree(text); xrtJsonWriterFree(writer);
    text=xrtXlonStringify(result,false,&length); assert(text && length); xrtFree(text); xrtValueRelease(result);
#endif
    xvalue* bad=nominal(7002); assert(!xl6PersistenceProjectV1(bad,&plan)); error(XERR_UNSUPPORTED);
    xrtValueRelease(bad); bad=nominal(7004); assert(!xl6PersistenceProjectV1(bad,&plan)); error(XERR_UNSUPPORTED); xrtValueRelease(bad);
    /* Exclusion is semantic, not a whole-graph pre-copy. Including the same
     * formerly private node must now reject its explicitly disabled schema. */
    XL6_PersistenceFieldV1 field={XRT_STR_LITERAL("private"),XRT_STR_LITERAL("private"),0};
    XL6_PersistenceSchemaV1 included[]={schemas[0],schemas[1]}; included[0].identity=XL6_PROJECT_OPEN ? 7003 : 7001; included[0].fieldCount=1; included[0].fields=&field;
    XL6_PersistencePlanV1 includePlan={sizeof(includePlan),2,included,NULL,NULL,0};
    assert(!xl6PersistenceProjectV1(xrtValueObjectGet(source,XRT_STR_LITERAL("class")),&includePlan)); error(XERR_UNSUPPORTED);
    /* Descriptor corruption is refused before interpreting any opaque payload. */
    XL6_PersistencePlanV1 corrupt=plan; corrupt.size--; assert(!xl6PersistenceProjectV1(source,&corrupt)); error(XERR_ARGUMENT);
    corrupt=plan; corrupt.schemas=NULL; assert(!xl6PersistenceProjectV1(source,&corrupt)); error(XERR_ARGUMENT);
    included[1]=included[0]; corrupt=includePlan; assert(!xl6PersistenceProjectV1(source,&corrupt)); error(XERR_ARGUMENT);
    included[1]=schemas[1]; field.flags=8; assert(!xl6PersistenceProjectV1(source,&corrupt)); error(XERR_ARGUMENT);
    field.flags=0; included[0].fieldCount=SIZE_MAX;
    assert(!xl6PersistenceProjectV1(source,&corrupt)); error(XERR_ARGUMENT);
    included[0].fieldCount=1;
    xvaluegraphsequencev1 invalid[2]={{7006,XVALUE_INT_MAP},{7007,XVALUE_SET}};
    for(int mode=0;mode<5;++mode) {
        corrupt=plan; corrupt.sequences=invalid;
        if(mode==0) corrupt.sequences=NULL;
        if(mode==1) invalid[0].TypeId=7001;
        if(mode==2) invalid[1].TypeId=7006;
        if(mode==3) invalid[0].Type=XVALUE_OBJECT;
        if(mode==4) corrupt.sequenceCount=SIZE_MAX;
        assert(!xl6PersistenceProjectV1(source,&corrupt)); error(XERR_ARGUMENT);
        invalid[0]=(xvaluegraphsequencev1){7006,XVALUE_INT_MAP};
        invalid[1]=(xvaluegraphsequencev1){7007,XVALUE_SET};
    }
#if XL6_PROJECT_OPEN
    /* A forged opaque address with unrelated Ops is not a DynamicFields. */
    bad=nominal(7003); ptr raw=(ptr)(uintptr_t)1;
    xvalue* opaque=xrtValueHandleTake(&raw,&opaqueOps,NULL),*storage=xrtValueObject(); assert(opaque && storage);
    assert(xrtValueObjectSetTake(storage,XRT_STR_LITERAL("handle"),&opaque));
    assert(xrtValueObjectSetTake(bad,XRT_STR_LITERAL("storage"),&storage));
    assert(!xl6PersistenceProjectV1(bad,&plan)); error(XERR_TYPE); xrtValueRelease(bad);
    /* Dynamic graph edges take part in cycle detection, not an independent
     * deep-clone pass which would silently duplicate aliases or leak refs. */
    xvalue* object=xrtValueObjectGet(source,XRT_STR_LITERAL("class"));
    storage=xrtValueObjectGet(object,XRT_STR_LITERAL("storage"));
    opaque=xrtValueObjectGet(storage,XRT_STR_LITERAL("handle")); assert(xrtValueGetHandle(opaque,&raw,NULL,NULL));
    assert(xrtDynamicFieldsSetRef(raw,XRT_STR_LITERAL("cycle"),object));
    assert(!xl6PersistenceProjectV1(source,&plan)); error(XERR_VALUE);
    assert(xrtDynamicFieldsRemove(raw,XRT_STR_LITERAL("cycle")));
    /* Ordinary iteration permits an erase which loses the busy child's owner
     * reference. The protected visitor must refuse BEFORE either key or child
     * is retired, preserving the complete source and balancing both memos. */
    XL6_PersistenceSchemaV1 mutationSchemas[]={schemas[0],schemas[1],schemas[2],{7005,0,0,NULL,&mutateOpen}};
    XL6_PersistencePlanV1 mutationPlan={sizeof(mutationPlan),4,mutationSchemas,projectMutating,NULL,0};
    xvalue* victim=nominal(7005); mutationFields=raw;
    assert(xrtDynamicFieldsSetRefNew(raw,XRT_STR_LITERAL("victim"),victim));
    assert(!xl6PersistenceProjectV1(source,&mutationPlan)); error(XERR_STATE);
    assert(xrtDynamicFieldsGet(raw,XRT_STR_LITERAL("victim"))==victim);
    assert(xrtDynamicFieldsRemove(raw,XRT_STR_LITERAL("victim"))); mutationFields=NULL;
#endif
    xrtValueRelease(source); xrtValueRelease(native);
}
static int projectFaults(const char* operation,size_t point)
{
    assert(!strcmp(operation,"project") || !strcmp(operation,"wide"));
    bool wide=!strcmp(operation,"wide"); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* native=graphSource(),*source=projectRoot(native,XL6_PROJECT_OPEN,wide);
    assert(xrtMemDebugFailAfter(point)); xvalue* result=xl6PersistenceProjectV1(source,&plan);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    if (hit) { assert(!result); error(XERR_MEMORY); }
    else { assert(result); projectionCheck(result,XL6_PROJECT_OPEN,wide); }
    observed(native); xrtValueRelease(result); xrtValueRelease(source); xrtValueRelease(native); balanced(&before);
    printf("persistence-project profile=%u open=%u operation=%s point=%zu hit=%u balanced\n",XL6_FACTORY_PROFILE,XL6_PROJECT_OPEN,operation,point,hit); return 0;
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true));
    if (argc==3) return projectFaults(argv[1],(size_t)strtoull(argv[2],NULL,10));
    assert(argc==1); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    for (size_t i=0;i<25;++i) { projectVectors(); balanced(&before); }
    printf("persistence-project profile=%u open=%u: 25 projected graph cycles balanced\n",XL6_FACTORY_PROFILE,XL6_PROJECT_OPEN); return 0;
}
