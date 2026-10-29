#include "src/runtime/xl6_dict_import_chain.h"
#include "src/runtime/xl6_dict_graph_access.h"
#include "src/runtime/xl6_dict_schema.h"
#include "src/runtime/xl6_list_import_chain.h"
#include "src/runtime/xl6_list_schema.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_persistence_restore_plan.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { ARRAY,LIST,DICT,WIDTH=3,DEPTH=3 };
#define OWNER(i) (UINT64_C(0xB2000)+(i))
#define DOMAIN(i) (UINT64_C(0xB2100)+(i))
#define LEAF UINT64_C(0xB2200)
#define TUPLE(t) (t).identity,(t).elementIdentity,(t).transfer,(t).size,(t).type
static const unsigned families[][DEPTH]={{DICT,DICT,DICT},{DICT,LIST,ARRAY},{ARRAY,DICT,LIST},{LIST,ARRAY,DICT}};
static const xstrview keys[]={{"",0},{"a",1},{"a\0b\xE4\xBD\xA0",6}};
static const int64_t sparse[]={INT64_MIN,0,INT64_MAX};
static int64_t integer(const xvalue* value)
{ int64_t output=0; assert(xrtValueGetInt(value,&output)); return output; }
static bool scopeArmed,cleanupRefused,iteratorRefused;
static bool wideLeaf;
static size_t scopePoint,scopeSeen;
extern bool __real_xl6ErrorScopeBegin(xl6_error_scope*);
extern bool __real_xl6ErrorScopeBeginCleanup(xl6_error_scope*,bool);
extern ptr __real_xrtTypedDictIterNext(xtypeddictiter*,xstrview*);
bool __wrap_xl6ErrorScopeBegin(xl6_error_scope* scope)
{
    if (scopeArmed && scopeSeen++==scopePoint) { xrtClearError(); return false; }
    return __real_xl6ErrorScopeBegin(scope);
}
bool __wrap_xl6ErrorScopeBeginCleanup(xl6_error_scope* scope,bool failed)
{
    if (cleanupRefused) { cleanupRefused=false; xrtClearError(); return false; }
    return __real_xl6ErrorScopeBeginCleanup(scope,failed);
}
ptr __wrap_xrtTypedDictIterNext(xtypeddictiter* iterator,xstrview* key)
{
    if (iteratorRefused) { iteratorRefused=false; xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL}); return NULL; }
    return __real_xrtTypedDictIterNext(iterator,key);
}
static const XL6_CollectionImportProviderV1* provider(unsigned family)
{ return family==DICT ? xl6DictImportProviderV1() : family==LIST ? xl6ListImportProviderV1() : NULL; }
static bool refuseAppend(xvalue* target,xstrview key,const xvalue* item)
{ (void)target; (void)key; (void)item; return false; }
static void retireWithError(ptr context)
{ provider(DICT)->End(context); xrtSetErrorKind(XERR_TIMEOUT); }
static xvalue* factory(unsigned family,size_t level,xvalue* child,unsigned childFamily,bool erased)
{
    uint64_t owner=OWNER(level)+(erased ? 128 : 0),domain=family==ARRAY ? owner : DOMAIN(level)+(erased ? 128 : 0);
    xvalue* witness;
    if (erased) witness=xl6SequenceReferenceCreateV1(1,domain,LEAF,XL6_VALUE_ADMIT_ANY,false,LEAF);
    else if (child) {
        xvalue* owned=xrtValueRetain(child);
        witness=childFamily==DICT ? xl6SequenceDictCreateTakeV1(1,domain,OWNER(level+1),owned,false) :
            childFamily==LIST ? xl6SequenceListCreateTakeV1(1,domain,OWNER(level+1),owned,false) :
                xl6SequenceNestedCreateTakeV2(1,domain,OWNER(level+1),owned,false);
    } else witness=xl6SequencePrimitiveCreateV1(1,domain,LEAF,1,sizeof(int64_t),xrtTypeInt64(),
        wideLeaf ? XL6_VALUE_ADMIT_I64 : XL6_VALUE_ADMIT_I8,false,LEAF);
    assert(witness);
    XL6_SequenceRoleV1 role; XL6_SequenceLayoutV1 layout={0}; XL6_SequenceAdmissionV3 policy={0};
    assert(xl6SequenceSchemaCopyV1(witness,&role,&layout,&policy)); xl6SequenceAdmissionDropV3(&policy);
    xvalue* value;
    if (family==DICT) {
        XL6_DictLayoutV1 t={owner,layout}; value=xl6DictProjectionTakeV1(xl6DictCreateTakeV1(&t,witness),&t,NULL);
    } else if (family==LIST) {
        XL6_ListLayoutV1 t={owner,layout}; value=xl6ListProjectionTakeV1(xl6ListCreateTakeV1(&t,witness),&t,NULL);
    } else value=xl6ArrayProjectionTakeV1(witness,TUPLE(layout),NULL);
    assert(value); return value;
}
static void seeds(xvalue** values,const unsigned* kinds,size_t depth)
{
    for (size_t at=depth;at;--at) {
        size_t i=at-1; values[i]=factory(kinds[i],i,i+1<depth ? values[i+1] : NULL,
            i+1<depth ? kinds[i+1] : ARRAY,false);
    }
}
static void writeNative(xvalue* parent,unsigned family,size_t at,const xvalue* item,bool language)
{
    if (family==DICT) assert(xl6DictErasedSetV1(parent,keys[at],item));
    else if (family==LIST) assert(xl6ListErasedSetV1(parent,language ? sparse[at] : (int64_t)at,item));
    else assert(xl6ArrayErasedPushV1(parent,item));
}
static xvalue* source(const unsigned* kinds,size_t depth,bool native,bool language)
{
    xvalue* child=NULL;
    for (size_t at=depth;at;--at) {
        size_t i=at-1;
        xvalue* value=native ? factory(kinds[i],i,NULL,ARRAY,true) : kinds[i]==DICT ? xrtValueObject() :
            language && kinds[i]==LIST ? xrtValueIntMap() : xrtValueArray(); assert(value);
        for (size_t n=0;n<WIDTH;++n) {
            xvalue* item=child ? xrtValueRetain(child) : xrtValueInt((int64_t)n+7); assert(item);
            if (native) writeNative(value,kinds[i],n,item,language);
            else if (kinds[i]==DICT) assert(xrtValueObjectSet(value,keys[n],item));
            else if (language && kinds[i]==LIST) assert(xrtValueIntMapSet(value,sparse[n],item));
            else assert(xrtValueArrayAppend(value,item));
            xrtValueRelease(item);
        }
        xrtValueRelease(child); child=value;
    }
    return child;
}
static xvalue* read(const xvalue* value,unsigned family,size_t at,bool language)
{
    xvalue* result=NULL; bool found=false;
    if (family==DICT) assert(xl6DictErasedReadV1(value,keys[at],false,&result,&found) && found);
    else if (family==LIST) assert(xl6ListErasedReadV1(value,language ? sparse[at] : (int64_t)at,false,&result,&found) && found);
    else result=xl6ArrayErasedReadV1(value,(int64_t)at);
    assert(result); return result;
}
static void observe(const xvalue* root,const unsigned* kinds,size_t depth,bool language)
{
    xvalue* value=xrtValueRetain(root);
    for (size_t i=0;i<depth;++i) {
        xvalue* first=read(value,kinds[i],0,language);
        xvalue* last=read(value,kinds[i],2,language);
        if (i+1<depth) assert(first==last);
        else assert(integer(first)==7 && integer(last)==9 && xrtValueTypeId(first)==LEAF);
        xrtValueRelease(last); xrtValueRelease(value); value=first;
    }
    xrtValueRelease(value);
}
static xvalue* import(xvalue** values,const unsigned* kinds,size_t depth,const xvalue* input,bool language,bool json)
{
    const XL6_CollectionImportProviderV1* providers[DEPTH]={0};
    for (size_t i=0;i<depth;++i) providers[i]=provider(kinds[i]);
    XL6_CollectionImportPlanV1 plan={.size=sizeof(plan),.providers=providers,
        .flags=language ? XL6_COLLECTION_IMPORT_LANGUAGE_INPUT_V1 : XL6_COLLECTION_IMPORT_RESTORE_V1|
            (json ? XL6_COLLECTION_IMPORT_JSON_V1 : 0),
        .leafRule=language ? 0 : XL6_VALUE_ADMIT_I8,.leafIdentity=language ? 0 : LEAF};
    xvalue* value=xl6CollectionImportChainTakeV1(values,depth,input,&plan);
    for (size_t i=0;i<depth;++i) assert(!values[i]);
    return value;
}
static bool restore(xvalue** values,const unsigned* kinds,size_t depth,const xvalue* input,bool json,xvalue** output)
{
    XL6_RestoreNodeV1 nodes[DEPTH+1]={0};
    for (size_t i=0;i<depth;++i) nodes[i]=(XL6_RestoreNodeV1){.kind=XL6_RESTORE_SEQUENCE_V1,
        .identity=OWNER(i),.as.sequence={i,i+1,provider(kinds[i])}};
    nodes[depth]=(XL6_RestoreNodeV1){.kind=XL6_RESTORE_SCALAR_V1,.identity=LEAF,.as.scalar=XL6_VALUE_ADMIT_I8};
    XL6_RestorePlanV1 plan={.size=sizeof(plan),.nodeCount=depth+1,.root=0,.nodes=nodes,.json=json};
    bool ok=xl6PersistenceRestorePlanReadTakeV1(values,depth,NULL,input,&plan,output);
    for (size_t i=0;i<depth;++i) assert(!values[i]);
    return ok;
}
static void vectors(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    for (size_t row=0;row<sizeof(families)/sizeof(*families);++row) for (unsigned mode=0;mode<4;++mode) {
        const unsigned* kinds=families[row]; bool language=mode<2,native=mode==0,json=mode==3;
        xvalue* input=source(kinds,DEPTH,native,language),*values[DEPTH]={0};
        seeds(values,kinds,DEPTH); xvalue* value=import(values,kinds,DEPTH,input,language,json);
        if (!value) fprintf(stderr,"import vector row=%zu mode=%u kind=%u\n",row,mode,(unsigned)xrtErrorKind(xrtGetError()));
        assert(value && value!=input); observe(value,kinds,DEPTH,language); xrtValueRelease(value);
        if (!language) {
            seeds(values,kinds,DEPTH); value=NULL; assert(restore(values,kinds,DEPTH,input,json,&value));
            assert(value && xrtValueType(value)!=XVALUE_NULL); observe(value,kinds,DEPTH,false); xrtValueRelease(value);
            size_t size=0; char* encoded=json ? xrtJsonStringify(input,false,&size) : xrtXlonStringify(input,false,&size); assert(encoded);
            xvalue* parsed=json ? xrtJsonParse((xstrview){encoded,size}) : xrtXlonParse((xstrview){encoded,size}); assert(parsed);
            seeds(values,kinds,DEPTH); value=NULL; assert(restore(values,kinds,DEPTH,parsed,json,&value));
            /* Serialized formats have no reference-identity token: aliases may
             * become independent values. Exact keys/scalars still round-trip. */
            xvalue* cursor=xrtValueRetain(value);
            for (size_t i=0;i<DEPTH;++i) { xvalue* next=read(cursor,kinds[i],2,false); xrtValueRelease(cursor); cursor=next; }
            assert(integer(cursor)==9); xrtValueRelease(cursor); xrtValueRelease(value);
            xrtValueRelease(parsed); xrtFree(encoded);
        }
        xrtValueRelease(input);
    }
    unsigned kind=DICT; xvalue* values[DEPTH]={0},*input=source(&kind,1,true,true);
    const XL6_CollectionImportProviderV1* p=provider(DICT); XL6_CollectionImportInputV1 snapshot={0};
    assert(p->InputBegin(input,&snapshot) && snapshot.count==WIDTH && snapshot.textKeys);
    assert(xl6DictErasedClearV1(input)); xrtValueRelease(input); input=NULL;
    assert(snapshot.textKeys[2].Size==keys[2].Size && !memcmp(snapshot.textKeys[2].Data,keys[2].Data,keys[2].Size));
    assert(integer(snapshot.items[2])==9); p->InputEnd(&snapshot); assert(!snapshot.context && !snapshot.textKeys);
    input=source(&kind,1,false,false); assert(xrtValueObjectSetNew(input,keys[2],xrtValueInt(500)));
    seeds(values,&kind,1); xvalue* value=NULL; assert(restore(values,&kind,1,input,false,&value));
    assert(xrtValueType(value)==XVALUE_NULL && !xrtGetError()); xrtValueRelease(value); xrtValueRelease(input);
    input=xrtValueArray(); seeds(values,&kind,1); assert(!import(values,&kind,1,input,true,false) && xrtGetError());
    xrtClearError(); xrtValueRelease(input);
    input=source(&kind,1,true,true); void* raw=NULL,*user=NULL; const xvaluehandleops* ops=NULL; XL6_DictGraphViewV1 pin={0};
    assert(xrtValueGetHandle(input,&raw,&ops,&user) && xl6DictGraphBeginV1(ops,raw,user,xrtValueTypeId(input),&pin)==1);
    seeds(values,&kind,1); assert(!import(values,&kind,1,input,true,false) && xrtGetError());
    xrtClearError(); xl6DictGraphEndV1(&pin);
    seeds(values,&kind,1); iteratorRefused=true; xl6_error_scope boundary={0}; assert(xl6ErrorScopeBegin(&boundary));
    assert(!import(values,&kind,1,input,true,false)); xl6_error_result outcome=xl6ErrorScopeEnd(&boundary);
    assert(!outcome.ok && !outcome.error && !iteratorRefused); xrtValueRelease(input);
    /* Matching BUSY child is a reference; a wrong typed policy is not an
     * ANY-erased conversion capability even when its values happen to fit. */
    unsigned pair[]={DICT,DICT}; seeds(values,pair,2);
    xvalue* child=factory(DICT,1,NULL,ARRAY,false),*leaf=xrtValueInt(7); assert(leaf);
    assert(xl6DictErasedSetV1(child,keys[0],leaf)); xrtValueRelease(leaf);
    input=factory(DICT,0,NULL,ARRAY,true); assert(xl6DictErasedSetV1(input,keys[0],child));
    assert(xrtValueGetHandle(child,&raw,&ops,&user) && xl6DictGraphBeginV1(ops,raw,user,xrtValueTypeId(child),&pin)==1);
    value=import(values,pair,2,input,true,false); assert(value);
    xvalue* retained=read(value,DICT,0,true); assert(retained==child); xrtValueRelease(retained);
    xl6DictGraphEndV1(&pin); xrtValueRelease(value); xrtValueRelease(input); xrtValueRelease(child);
    seeds(values,pair,2); wideLeaf=true; child=factory(DICT,1,NULL,ARRAY,false); wideLeaf=false;
    leaf=xrtValueInt(7); assert(leaf && xl6DictErasedSetV1(child,keys[0],leaf)); xrtValueRelease(leaf);
    input=factory(DICT,0,NULL,ARRAY,true); assert(xl6DictErasedSetV1(input,keys[0],child));
    assert(!import(values,pair,2,input,true,false) && xrtGetError()); xrtClearError(); xrtValueRelease(input); xrtValueRelease(child);
    /* Invalid resident plans consume all private seeds before provider calls. */
    input=source(&kind,1,false,false); seeds(values,&kind,1);
    XL6_CollectionImportProviderV1 invalid=*p; invalid.AppendText=NULL;
    const XL6_CollectionImportProviderV1* invalidProviders[]={&invalid};
    XL6_CollectionImportPlanV1 invalidPlan={.size=sizeof(invalidPlan),.providers=invalidProviders};
    assert(!xl6CollectionImportChainTakeV1(values,1,input,&invalidPlan) && !values[0] && xrtGetError());
    xrtClearError(); xrtValueRelease(input);
    /* Text-family cycles are traversed by the same active path, not hidden
     * in a side importer; rejection still consumes private seeds. */
    input=factory(DICT,0,NULL,ARRAY,true); assert(xl6DictErasedSetV1(input,keys[0],input));
    seeds(values,pair,2); assert(!import(values,pair,2,input,true,false) && xrtGetError()); xrtClearError();
    assert(xl6DictErasedClearV1(input)); xrtValueRelease(input);
    /* A seed/source alias is not a transfer capability. The borrowed source
     * survives refusal; the private table slot is consumed. */
    input=factory(DICT,0,NULL,ARRAY,false); values[0]=xrtValueRetain(input);
    assert(!import(values,&kind,1,input,true,false) && xrtGetError()); xrtClearError();
    assert(xl6DictErasedCountV1(input,false,&(int64_t){0})); xrtValueRelease(input);
    /* First diagnosticless refusal must survive a second cleanup refusal. */
    input=source(&kind,1,true,true); seeds(values,&kind,1); assert(xl6ErrorScopeBegin(&boundary));
    scopeArmed=true; scopeSeen=scopePoint=0; cleanupRefused=true;
    assert(!import(values,&kind,1,input,true,false)); scopeArmed=false;
    outcome=xl6ErrorScopeEnd(&boundary); assert(!outcome.ok && !outcome.error && !cleanupRefused);
    xrtValueRelease(input);
    input=source(&kind,1,false,false); seeds(values,&kind,1); assert(xl6ErrorScopeBegin(&boundary));
    invalid=*p; invalid.AppendText=refuseAppend; invalid.End=retireWithError;
    assert(!xl6CollectionImportChainTakeV1(values,1,input,&invalidPlan) && !values[0]);
    outcome=xl6ErrorScopeEnd(&boundary); assert(!outcome.ok && !outcome.error); xrtValueRelease(input);
    input=source(&kind,1,false,false); seeds(values,&kind,1); assert(xl6ErrorScopeBegin(&boundary));
    XL6_RestoreNodeV1 nodes[]={
        {.kind=XL6_RESTORE_SEQUENCE_V1,.identity=OWNER(0),.as.sequence={0,1,&invalid}},
        {.kind=XL6_RESTORE_SCALAR_V1,.identity=LEAF,.as.scalar=XL6_VALUE_ADMIT_I8}};
    XL6_RestorePlanV1 graph={.size=sizeof(graph),.nodeCount=2,.root=0,.nodes=nodes}; value=NULL;
    assert(!xl6PersistenceRestorePlanReadTakeV1(values,1,NULL,input,&graph,&value) && !value && !values[0]);
    outcome=xl6ErrorScopeEnd(&boundary); assert(!outcome.ok && !outcome.error); xrtValueRelease(input);
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify(); puts("[PASS] exact-key Dict shared import/restore vectors");
}
static bool attempt(unsigned lane,size_t point,bool scopes)
{
    const unsigned* kinds=families[lane%4]; bool native=lane<4,language=lane<8,graph=lane>=8;
    xl6ProductMemoryBegin(SIZE_MAX); xvalue* input=source(kinds,DEPTH,native,language),*values[DEPTH]={0};
    seeds(values,kinds,DEPTH); xl6_error_scope boundary={0}; assert(xl6ErrorScopeBegin(&boundary));
    scopeSeen=0; scopePoint=point; scopeArmed=scopes; if (!scopes) assert(xrtMemDebugFailAfter(point));
    xvalue* value=NULL; bool ok=graph ? restore(values,kinds,DEPTH,input,lane>=12,&value) :
        ((value=import(values,kinds,DEPTH,input,language,false))!=NULL);
    bool hit=scopes ? scopeSeen>point : xrtMemDebugFailTriggered(); scopeArmed=false; xrtMemDebugFailClear();
    if (!ok) xl6ErrorScopeCaptureFailure(&boundary);
    xl6_error_result outcome=xl6ErrorScopeEnd(&boundary);
    assert(ok==!hit && outcome.ok==!hit); xrtErrorFree(outcome.error);
    if (ok) observe(value,kinds,DEPTH,language);
    xrtValueRelease(value); xrtValueRelease(input); xrtClearError();
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    printf("Dict import lane=%u mode=%s point=%zu hit=%d balanced\n",lane,scopes ? "scopes" : "oom",point,hit); return hit;
}
int main(int argc,char** argv)
{
    assert(argc==2);
    if (!strcmp(argv[1],"vectors")) vectors();
    else {
        bool scopes=!strcmp(argv[1],"scopes"); assert(scopes || !strcmp(argv[1],"oom"));
        for (unsigned lane=0;lane<16;++lane) {
            size_t point=0; while(point<4096 && attempt(lane,point,scopes)) ++point;
            assert(point<4096); printf("Dict import closed lane=%u mode=%s runs=%zu first-no-hit=%zu\n",
                lane,scopes ? "scopes" : "oom",point+1,point);
        }
    }
    return 0;
}
