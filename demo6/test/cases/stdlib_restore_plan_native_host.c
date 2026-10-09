#include "src/runtime/xl6_persistence_restore_plan.h"
#include "src/runtime/xl6_list_import_chain.h"
#include "src/runtime/xl6_list_schema.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_code_lease.h"
#include "src/runtime/xl6_restore_class.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define OBJECT_ID UINT64_C(0xED00)
#define LIST_ID UINT64_C(0xED01)
#define ARRAY_ID UINT64_C(0xED02)
#define LEAF_ID UINT64_C(0xED03)
#define TAG INT64_C(119)
extern void xl6TestListGraphBoundaryFailureV1(int,int);
static unsigned factories,drops,factoryFailure,finalizerFailure,admissionFailure;
static bool checkedAdmission(const xvalue* value,uint64_t identity,int64_t tag)
{
    bool ok=xl6RestoreClassProviderV1()->Admits(value,identity,tag);
    if (ok && admissionFailure==1) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    if (ok && admissionFailure==2) { xrtSetErrorKind(XERR_MEMORY); return false; }
    return ok;
}
static bool finalize(xvalue* value,void* unused)
{
    (void)value; (void)unused; ++drops;
    if (finalizerFailure==2) xrtSetErrorKind(XERR_VALUE);
    return !finalizerFailure;
}
static xvalue* objectDefault(void)
{
    ++factories;
    xvalue* value=xl6ClassRuntimeV2Get()->Create(NULL,finalize);
    if (!value) return NULL;
    if (!xrtValueTypeIdBind(value,OBJECT_ID) ||
        !xrtValueObjectSetNew(value,XRT_STR_LITERAL("$xlir.class.runtime-type"),xrtValueInt(TAG)) ||
        !xrtValueObjectSetNew(value,XRT_STR_LITERAL("keep"),xrtValueString(XRT_STR_LITERAL("default")))) {
        xerror* error=xrtTakeError(); xrtValueRelease(value); xrtClearError(); xrtSetErrorTake(error); return NULL;
    }
    value=xl6ClassRuntimeV2Get()->CommitTake(value);
    if (value && factoryFailure) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    return value;
}
static xvalue* rootDefault(void)
{
    xvalue* value=objectDefault();
    if (value && !xrtValueObjectSetNew(value,XRT_STR_LITERAL("child"),objectDefault())) {
        xerror* error=xrtTakeError(); xrtValueRelease(value); xrtClearError(); xrtSetErrorTake(error); return NULL;
    }
    return value;
}
static xvalue* seed(void)
{
    XL6_ListLayoutV1 layout={LIST_ID,{ARRAY_ID,LEAF_ID,2,sizeof(xstrview),xl6StringTypeV1()}};
    xvalue* value=xl6SequencePrimitiveCreateV1(1,ARRAY_ID,LEAF_ID,2,sizeof(xstrview),xl6StringTypeV1(),
        XL6_VALUE_ADMIT_STRING,false,LEAF_ID);
    return xl6ListProjectionTakeV1(xl6ListCreateTakeV1(&layout,value),&layout,xl6StringConverterV1());
}
static XL6_RestorePlanV1 plan(XL6_RestoreNodeV1* nodes,XL6_RestoreFieldV1* fields,bool json)
{
    nodes[0]=(XL6_RestoreNodeV1){.kind=XL6_RESTORE_OBJECT_V1,.identity=OBJECT_ID,
        .as.object={objectDefault,TAG,0,3,xl6RestoreClassProviderV1()}};
    nodes[1]=(XL6_RestoreNodeV1){.kind=XL6_RESTORE_SEQUENCE_V1,.identity=LIST_ID,
        .as.sequence={0,2,xl6ListImportProviderV1()}};
    nodes[2]=(XL6_RestoreNodeV1){.kind=XL6_RESTORE_SCALAR_V1,.identity=LEAF_ID,.as.scalar=XL6_VALUE_ADMIT_STRING};
    fields[0]=(XL6_RestoreFieldV1){XRT_STR_INIT("a\0x"),XRT_STR_INIT("left"),1,XL6_RESTORE_REQUIRED_V1};
    fields[1]=(XL6_RestoreFieldV1){XRT_STR_INIT("b"),XRT_STR_INIT("right"),1,XL6_RESTORE_REQUIRED_V1};
    fields[2]=(XL6_RestoreFieldV1){XRT_STR_INIT("missing"),XRT_STR_INIT("keep"),2,0};
    return (XL6_RestorePlanV1){sizeof(XL6_RestorePlanV1),3,3,0,nodes,fields,json};
}
static xvalue* wire(void)
{
    xvalue* object=xrtValueObject(),*array=xrtValueArray(); assert(object && array);
    assert(xrtValueArrayAppendNew(array,xrtValueString((xstrview){"a\0\xE4\xBD\xA0",5})) &&
        xrtValueObjectSet(object,(xstrview)XRT_STR_INIT("a\0x"),array) &&
        xrtValueObjectSet(object,XRT_STR_LITERAL("b"),array)); xrtValueRelease(array); return object;
}
static void observed(const xvalue* value,bool shared)
{
    assert(xrtValueTypeId(value)==OBJECT_ID);
    xvalue* left=xrtValueObjectGet(value,XRT_STR_LITERAL("left")),*right=xrtValueObjectGet(value,XRT_STR_LITERAL("right"));
    assert(left && right && (shared ? left==right : left!=right));
    xvalue* actual=NULL; bool found=false; xstrview text={0};
    assert(xl6ListErasedReadV1(left,0,false,&actual,&found) && found && xrtValueGetString(actual,&text));
    assert(text.Size==5 && !memcmp(text.Data,"a\0\xE4\xBD\xA0",5) && xrtValueTypeId(actual)==LEAF_ID);
    xrtValueRelease(actual);
    assert(xrtValueGetString(xrtValueObjectGet(value,XRT_STR_LITERAL("keep")),&text) &&
        text.Size==7 && !memcmp(text.Data,"default",7));
}
static void shared(void)
{
    XL6_RestoreNodeV1 nodes[4]; XL6_RestoreFieldV1 fields[3]; XL6_RestorePlanV1 p=plan(nodes,fields,false);
    xvalue* seeds[2]={seed(),NULL},*source=wire(); assert(seeds[0]);
    unsigned before=factories; xrtSetErrorKind(XERR_TIMEOUT); const xerror* incoming=xrtGetError();
    xvalue* result=xl6PersistenceRestorePlanTakeV1(seeds,1,NULL,source,&p);
    assert(result && !seeds[0] && factories==before+1 && xrtGetError()==incoming); xrtClearError();
    observed(result,true); assert(!xrtValueTypeId(xrtValueObjectGet(source,(xstrview)XRT_STR_INIT("a\0x"))));
    xrtValueRelease(result);
    /* A different explicit node is a different conversion domain, even with
     * exactly the same nominal identity and native schema. */
    seeds[0]=seed(); seeds[1]=seed(); assert(seeds[0] && seeds[1]);
    nodes[3]=nodes[1]; nodes[3].as.sequence.seed=1; fields[1].node=3; p.nodeCount=4;
    result=xl6PersistenceRestorePlanTakeV1(seeds,2,NULL,source,&p); assert(result && !seeds[0] && !seeds[1]);
    observed(result,false); xrtValueRelease(result); xrtValueRelease(source);
}
static void format(void)
{
    for (unsigned json=0;json<2;++json) {
        XL6_RestoreNodeV1 nodes[3]; XL6_RestoreFieldV1 fields[3]; XL6_RestorePlanV1 p=plan(nodes,fields,json);
        xvalue* source=wire(); size_t size=0;
        char* text=json ? xrtJsonStringify(source,false,&size) : xrtXlonStringify(source,false,&size); assert(text && size);
        xvalue* parsed=json ? xrtJsonParse((xstrview){text,size}) : xrtXlonParse((xstrview){text,size}); assert(parsed);
        xrtFree(text); xvalue* seeds[]={seed()}; assert(seeds[0]);
        xvalue* result=xl6PersistenceRestorePlanTakeV1(seeds,1,NULL,parsed,&p); assert(result);
        observed(result,false); xrtValueRelease(result); xrtValueRelease(parsed); xrtValueRelease(source);
    }
}
static xvalue* nestedFixture(XL6_RestoreNodeV1* nodes,XL6_RestoreFieldV1* fields,XL6_RestorePlanV1* output)
{
    XL6_RestorePlanV1 p=plan(nodes,fields,false);
    nodes[0].as.object.factory=rootDefault; nodes[0].as.object.count=2;
    nodes[3]=nodes[0]; nodes[3].as.object.factory=objectDefault; nodes[3].as.object.first=2; nodes[3].as.object.count=3;
    fields[2]=fields[0]; fields[3]=fields[1]; fields[4]=fields[2]; fields[4].source=XRT_STR_LITERAL("missing");
    fields[4].target=XRT_STR_LITERAL("keep"); fields[4].node=2; fields[4].flags=0;
    fields[0]=(XL6_RestoreFieldV1){XRT_STR_INIT("child"),XRT_STR_INIT("child"),3,XL6_RESTORE_IN_PLACE_V1};
    fields[1]=(XL6_RestoreFieldV1){XRT_STR_INIT("child"),XRT_STR_INIT("alias"),3,XL6_RESTORE_REQUIRED_V1};
    p.nodeCount=4; p.fieldCount=5; *output=p;
    xvalue* child=wire(),*source=xrtValueObject(); assert(source && xrtValueObjectSetNew(source,XRT_STR_LITERAL("child"),child));
    return source;
}
static void nested(void)
{
    XL6_RestoreNodeV1 nodes[4]; XL6_RestoreFieldV1 fields[5]; XL6_RestorePlanV1 p;
    xvalue* source=nestedFixture(nodes,fields,&p),*child;
    xvalue* seeds[]={seed()}; assert(seeds[0]); unsigned before=factories;
    xvalue* result=xl6PersistenceRestorePlanTakeV1(seeds,1,NULL,source,&p);
    assert(result && factories==before+2);
    child=xrtValueObjectGet(result,XRT_STR_LITERAL("child")); assert(child==xrtValueObjectGet(result,XRT_STR_LITERAL("alias")));
    observed(child,true); xrtValueRelease(result); xrtValueRelease(source);
}
static void rejected(void)
{
    for (unsigned test=0;test<12;++test) {
        XL6_RestoreNodeV1 nodes[3]; XL6_RestoreFieldV1 fields[3]; XL6_RestorePlanV1 p=plan(nodes,fields,false);
        xvalue* seeds[]={seed()},*source=wire(); assert(seeds[0]);
        if (test==0) p.size--;
        else if (test==1) fields[1].node=3;
        else if (test==2) fields[1].target=fields[0].target;
        else if (test==3) nodes[1].as.sequence.seed=1;
        else if (test==4) nodes[0].as.object.runtimeTag++;
        else if (test==5) fields[2].flags=XL6_RESTORE_REQUIRED_V1;
        else if (test==6) assert(xrtValueObjectSetNew(source,XRT_STR_LITERAL("b"),xrtValueNull()));
        else if (test==7) { p.json=true; nodes[2].as.scalar=XL6_VALUE_ADMIT_BYTES; }
        else if (test==8) assert(xrtValueTypeIdBind(source,OBJECT_ID+1));
        else if (test==9) { xrtValueRelease(source); source=objectDefault(); assert(source); }
        else if (test==10) fields[1].flags=XL6_RESTORE_IN_PLACE_V1;
        else nodes[0].as.object.provider=NULL;
        assert(!xl6PersistenceRestorePlanTakeV1(seeds,1,NULL,source,&p) && !seeds[0] && xrtGetError());
        xrtClearError(); xrtValueRelease(source);
    }
    /* A spoofed ordinary Object has neither the resident lifetime nor class
     * finalization duty, even with the exact nominal ID and runtime tag. */
    XL6_RestoreNodeV1 nodes[3]; XL6_RestoreFieldV1 fields[3]; XL6_RestorePlanV1 p=plan(nodes,fields,false);
    xvalue* seeds[]={seed()},*source=wire(),*target=xrtValueObject(); assert(seeds[0] && target);
    assert(xrtValueTypeIdBind(target,OBJECT_ID) &&
        xrtValueObjectSetNew(target,XRT_STR_LITERAL("$xlir.class.runtime-type"),xrtValueInt(TAG)));
    assert(!xl6PersistenceRestorePlanTakeV1(seeds,1,target,source,&p) && !seeds[0] && xrtGetError());
    xrtClearError(); xrtValueRelease(source);
}
static void depth(void)
{
    for (size_t count=65;count<=66;++count) {
        XL6_RestoreNodeV1 node={.kind=XL6_RESTORE_OBJECT_V1,.identity=OBJECT_ID,
            .as.object={objectDefault,TAG,0,1,xl6RestoreClassProviderV1()}};
        XL6_RestoreFieldV1 field={XRT_STR_INIT("next"),XRT_STR_INIT("next"),0,0};
        XL6_RestorePlanV1 p={sizeof(p),1,1,0,&node,&field,false};
        xvalue* source=xrtValueObject(); assert(source);
        for (size_t i=1;i<count;++i) {
            xvalue* parent=xrtValueObject(); assert(parent && xrtValueObjectSetNew(parent,XRT_STR_LITERAL("next"),source)); source=parent;
        }
        xvalue* result=xl6PersistenceRestorePlanTakeV1(NULL,0,NULL,source,&p);
        if (count==65) {
            assert(result); const xvalue* at=result;
            for (size_t i=0;i<count;++i) { assert(at && xrtValueTypeId(at)==OBJECT_ID); at=xrtValueObjectGet(at,XRT_STR_LITERAL("next")); }
            assert(!at);
        } else { assert(!result && xrtGetError() && xrtErrorKind(xrtGetError())==XERR_RANGE); xrtClearError(); }
        xrtValueRelease(result); xrtValueRelease(source);
    }
}
static void wide(size_t point,bool fault)
{
    enum { COUNT=70 }; XL6_RestoreNodeV1 nodes[COUNT+2]; XL6_RestoreFieldV1 fields[COUNT];
    char names[COUNT][16]; xvalue* seeds[COUNT]={0};
    nodes[0]=(XL6_RestoreNodeV1){.kind=XL6_RESTORE_OBJECT_V1,.identity=OBJECT_ID,
        .as.object={objectDefault,TAG,0,COUNT,xl6RestoreClassProviderV1()}};
    nodes[COUNT+1]=(XL6_RestoreNodeV1){.kind=XL6_RESTORE_SCALAR_V1,.identity=LEAF_ID,.as.scalar=XL6_VALUE_ADMIT_STRING};
    xvalue* source=xrtValueObject(),*array=xrtValueArray(); assert(source && array);
    assert(xrtValueArrayAppendNew(array,xrtValueString(XRT_STR_LITERAL("wide"))));
    for (size_t i=0;i<COUNT;++i) {
        int length=snprintf(names[i],sizeof(names[i]),"field%zu",i); assert(length>0 && (size_t)length<sizeof(names[i]));
        seeds[i]=seed(); assert(seeds[i]);
        nodes[i+1]=(XL6_RestoreNodeV1){.kind=XL6_RESTORE_SEQUENCE_V1,.identity=LIST_ID,
            .as.sequence={i,COUNT+1,xl6ListImportProviderV1()}};
        fields[i]=(XL6_RestoreFieldV1){{names[i],(size_t)length},{names[i],(size_t)length},i+1,XL6_RESTORE_REQUIRED_V1};
        assert(xrtValueObjectSet(source,fields[i].source,array));
    }
    xrtValueRelease(array); XL6_RestorePlanV1 p={sizeof(p),COUNT+2,COUNT,0,nodes,fields,false};
    xl6_error_scope scope={0};
    if (fault) { assert(xl6ErrorScopeBegin(&scope)); assert(xrtMemDebugFailAfter(point)); }
    xvalue* result=xl6PersistenceRestorePlanTakeV1(seeds,COUNT,NULL,source,&p);
    bool hit=false;
    if (fault) {
        hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
        assert(hit ? !result && !outcome.ok : result && outcome.ok); xrtErrorFree(outcome.error);
    } else assert(result);
    for (size_t i=0;i<COUNT;++i) {
        assert(!seeds[i]); if (!result) continue;
        const xvalue* current=xrtValueObjectGet(result,fields[i].target); assert(current && xrtValueTypeId(current)==LIST_ID);
        for (size_t j=0;j<i;++j) assert(current!=xrtValueObjectGet(result,fields[j].target));
    }
    assert(!xrtValueTypeId(source) && xrtValueCount(source)==COUNT);
    xrtValueRelease(result); xrtValueRelease(source); xrtClearError();
    if (fault) printf("native plan plan-wide point=%zu hit=%d complete-prefix balanced\n",point,hit);
}
static void objects(void)
{
    XL6_RestoreNodeV1 nodes[4]; XL6_RestoreFieldV1 fields[3]; XL6_RestorePlanV1 p=plan(nodes,fields,false);
    nodes[3]=nodes[0]; nodes[3].as.object.first=3; nodes[3].as.object.count=0;
    nodes[1].as.sequence.element=3;
    uint64_t id=OBJECT_ID; XL6_NominalAdmissionV1 nominal={XL6_NOMINAL_ADMIT_EXACT_ID,0,&id,1};
    xvalue* witness=xl6SequenceNominalCreateV1(1,ARRAY_ID,OBJECT_ID,&nominal,false,OBJECT_ID); assert(witness);
    XL6_ListLayoutV1 layout={LIST_ID,{ARRAY_ID,OBJECT_ID,3,sizeof(xvalue*),xrtTypeValue()}};
    xvalue* seeds[]={xl6ListProjectionTakeV1(xl6ListCreateTakeV1(&layout,witness),&layout,NULL)}; assert(seeds[0]);
    xvalue* child=xrtValueObject(),*array=xrtValueArray(),*source=xrtValueObject(); assert(child && array && source);
    assert(xrtValueArrayAppend(array,child) && xrtValueArrayAppend(array,child) &&
        xrtValueObjectSet(source,(xstrview)XRT_STR_INIT("a\0x"),array) && xrtValueObjectSet(source,XRT_STR_LITERAL("b"),array));
    xrtValueRelease(child); xrtValueRelease(array); p.nodeCount=4; unsigned before=factories;
    xvalue* result=xl6PersistenceRestorePlanTakeV1(seeds,1,NULL,source,&p); assert(result && factories==before+2);
    array=xrtValueObjectGet(result,XRT_STR_LITERAL("left")); assert(array==xrtValueObjectGet(result,XRT_STR_LITERAL("right")));
    xvalue* a=NULL,*b=NULL; bool found=false;
    assert(xl6ListErasedReadV1(array,0,false,&a,&found) && found && xl6ListErasedReadV1(array,1,false,&b,&found) && found);
    assert(a==b && xrtValueTypeId(a)==OBJECT_ID); xrtValueRelease(a); xrtValueRelease(b);
    xrtValueRelease(result); xrtValueRelease(source);
}
static void mixed(size_t point,bool fault)
{
    XL6_RestoreNodeV1 nodes[4]; XL6_RestoreFieldV1 fields[3]; XL6_RestorePlanV1 p=plan(nodes,fields,false);
    nodes[3]=nodes[2]; nodes[1].as.sequence.element=2;
    fields[2].node=3;
    nodes[2]=(XL6_RestoreNodeV1){.kind=XL6_RESTORE_SEQUENCE_V1,.identity=ARRAY_ID,.as.sequence={1,3,NULL}};
    p.nodeCount=4;
    xvalue* child=xl6SequencePrimitiveCreateV1(1,ARRAY_ID,LEAF_ID,2,sizeof(xstrview),xl6StringTypeV1(),
        XL6_VALUE_ADMIT_STRING,false,LEAF_ID); assert(child);
    child=xl6ArrayProjectionTakeV1(child,ARRAY_ID,LEAF_ID,2,sizeof(xstrview),xl6StringTypeV1(),xl6StringConverterV1()); assert(child);
    xvalue* witness=xl6SequenceNestedCreateTakeV2(1,ARRAY_ID+16,ARRAY_ID,xrtValueRetain(child),false); assert(witness);
    XL6_ListLayoutV1 layout={LIST_ID,{ARRAY_ID+16,ARRAY_ID,3,sizeof(xvalue*),xrtTypeValue()}};
    xvalue* seeds[]={xl6ListProjectionTakeV1(xl6ListCreateTakeV1(&layout,witness),&layout,NULL),child}; assert(seeds[0]);
    xvalue* leaf=xrtValueArray(),*outer=xrtValueArray(),*source=xrtValueObject(); assert(leaf && outer && source);
    assert(xrtValueArrayAppendNew(leaf,xrtValueString((xstrview){"a\0b",3})) &&
        xrtValueArrayAppend(outer,leaf) && xrtValueArrayAppend(outer,leaf) &&
        xrtValueObjectSet(source,(xstrview)XRT_STR_INIT("a\0x"),outer) && xrtValueObjectSet(source,XRT_STR_LITERAL("b"),outer));
    xrtValueRelease(leaf); xrtValueRelease(outer);
    xl6_error_scope scope={0}; if (fault) { assert(xl6ErrorScopeBegin(&scope)); assert(xrtMemDebugFailAfter(point)); }
    xvalue* result=xl6PersistenceRestorePlanTakeV1(seeds,2,NULL,source,&p); bool hit=false;
    if (fault) {
        hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
        assert(hit ? !result && !outcome.ok : result && outcome.ok); xrtErrorFree(outcome.error);
    } else assert(result);
    assert(!seeds[0] && !seeds[1]);
    if (result) {
        outer=xrtValueObjectGet(result,XRT_STR_LITERAL("left")); assert(outer==xrtValueObjectGet(result,XRT_STR_LITERAL("right")));
        xvalue* a=NULL,*b=NULL; bool found=false;
        assert(xl6ListErasedReadV1(outer,0,false,&a,&found) && found && xl6ListErasedReadV1(outer,1,false,&b,&found) && found);
        assert(a==b && xrtValueTypeId(a)==ARRAY_ID);
        xvalue* item=xl6ArrayErasedReadV1(a,0); xstrview text={0};
        assert(item && xrtValueTypeId(item)==LEAF_ID && xrtValueGetString(item,&text) && text.Size==3 && !memcmp(text.Data,"a\0b",3));
        xrtValueRelease(item); xrtValueRelease(a); xrtValueRelease(b);
    }
    assert(!xrtValueTypeId(source) && xrtValueCount(source)==2);
    xrtValueRelease(result); xrtValueRelease(source); xrtClearError();
    if (fault) printf("native plan plan-mixed-restore point=%zu hit=%d complete-prefix balanced\n",point,hit);
}
static void boundaries(void)
{
    for (int injection=-1;injection<=1;injection+=2) for (int cleanup=-1;cleanup<=1;++cleanup) {
        XL6_RestoreNodeV1 nodes[3]; XL6_RestoreFieldV1 fields[3]; XL6_RestorePlanV1 p=plan(nodes,fields,false);
        xvalue* source=wire(),*seeds[]={seed()},*target=objectDefault(); assert(seeds[0] && target);
        xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope)); unsigned before=drops;
        xl6TestListGraphBoundaryFailureV1(injection,cleanup);
        assert(!xl6PersistenceRestorePlanTakeV1(seeds,1,target,source,&p) && !seeds[0] && drops==before+1);
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
        assert(!outcome.ok && (injection<0 ? !outcome.error : outcome.error && xrtErrorKind(outcome.error)==XERR_MEMORY));
        xrtErrorFree(outcome.error); xrtClearError(); xrtValueRelease(source); xl6TestListGraphBoundaryFailureV1(0,0);
    }
    for (finalizerFailure=0;finalizerFailure<3;++finalizerFailure) {
        XL6_RestoreNodeV1 nodes[3]; XL6_RestoreFieldV1 fields[3]; XL6_RestorePlanV1 p=plan(nodes,fields,false);
        xvalue* source=wire(),*seeds[]={seed()}; assert(seeds[0]); factoryFailure=1;
        xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope)); unsigned before=drops;
        assert(!xl6PersistenceRestorePlanTakeV1(seeds,1,NULL,source,&p) && !seeds[0] && drops==before+1);
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope); assert(!outcome.ok && !outcome.error);
        factoryFailure=0; xrtClearError(); xrtValueRelease(source);
    }
    finalizerFailure=0;
    const XL6_RestoreObjectProviderV1 delegate={xl6RestoreClassProviderV1()->IsNative,checkedAdmission};
    for (admissionFailure=1;admissionFailure<=2;++admissionFailure) {
        XL6_RestoreNodeV1 nodes[3]; XL6_RestoreFieldV1 fields[3]; XL6_RestorePlanV1 p=plan(nodes,fields,false);
        nodes[0].as.object.provider=&delegate;
        xvalue* source=wire(),*seeds[]={seed()}; assert(seeds[0]);
        xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope)); unsigned before=drops;
        assert(!xl6PersistenceRestorePlanTakeV1(seeds,1,NULL,source,&p) && !seeds[0] && drops==before+1);
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
        assert(!outcome.ok && (admissionFailure==1 ? !outcome.error : outcome.error && xrtErrorKind(outcome.error)==XERR_MEMORY));
        xrtErrorFree(outcome.error); xrtClearError(); xrtValueRelease(source);
    }
    admissionFailure=0;
    /* A literal wire key matching the runtime marker is ordinary DATA, not
     * evidence of native class storage. Exact source-key renames stay legal. */
    XL6_RestoreNodeV1 nodes[3]; XL6_RestoreFieldV1 fields[3]; XL6_RestorePlanV1 p=plan(nodes,fields,false);
    fields[0].source=XRT_STR_LITERAL("$xlir.class.runtime-type"); fields[0].node=2;
    xvalue* source=wire(),*seeds[]={seed()}; assert(seeds[0]);
    assert(xrtValueObjectSetNew(source,fields[0].source,xrtValueString(XRT_STR_LITERAL("data"))));
    xvalue* result=xl6PersistenceRestorePlanTakeV1(seeds,1,NULL,source,&p); assert(result);
    xstrview text={0}; assert(xrtValueGetString(xrtValueObjectGet(result,XRT_STR_LITERAL("left")),&text) && text.Size==4);
    xrtValueRelease(result); xrtValueRelease(source);
}
void xl6TestRestorePlanVectorsV1(const char* name)
{
    if (!strcmp(name,"plan-shared")) shared();
    else if (!strcmp(name,"plan-format")) format();
    else if (!strcmp(name,"plan-nested")) nested();
    else if (!strcmp(name,"plan-rejected")) rejected();
    else if (!strcmp(name,"plan-boundaries")) boundaries();
    else if (!strcmp(name,"plan-depth")) depth();
    else if (!strcmp(name,"plan-wide-domains")) wide(0,false);
    else if (!strcmp(name,"plan-objects")) objects();
    else if (!strcmp(name,"plan-mixed")) mixed(0,false);
    else assert(0);
}
bool xl6TestRestorePlanFaultV1(const char* name,size_t point)
{
    if (strncmp(name,"plan-",5)) return false;
    if (!strcmp(name,"plan-wide")) { wide(point,true); return true; }
    if (!strcmp(name,"plan-mixed-restore")) { mixed(point,true); return true; }
    bool target=!strcmp(name,"plan-target"),nested=!strcmp(name,"plan-nested-restore");
    assert(target || nested || !strcmp(name,"plan-restore"));
    XL6_RestoreNodeV1 nodes[4]; XL6_RestoreFieldV1 fields[5]; XL6_RestorePlanV1 p=plan(nodes,fields,false);
    xvalue* source=nested ? nestedFixture(nodes,fields,&p) : wire(),*seeds[]={seed()},*owned=target ? objectDefault() : NULL;
    assert(seeds[0] && (!target || owned));
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope)); assert(xrtMemDebugFailAfter(point));
    xvalue* result=xl6PersistenceRestorePlanTakeV1(seeds,1,owned,source,&p);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(hit ? !result && !outcome.ok : result && outcome.ok); assert(!seeds[0]);
    if (!hit) observed(nested ? xrtValueObjectGet(result,XRT_STR_LITERAL("child")) : result,true);
    assert(!xrtValueTypeId(source) && xrtValueCount(source)==(nested ? 1 : 2));
    xrtErrorFree(outcome.error); xrtValueRelease(result); xrtValueRelease(source); xrtClearError();
    printf("native plan %s point=%zu hit=%d complete-prefix balanced\n",name,point,hit); return true;
}
