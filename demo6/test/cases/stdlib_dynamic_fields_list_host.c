/* Genuine native List inputs: source language factories are still migrating. */
#define XL6_VALUE_COPY_NO_MAIN
#include "stdlib_value_copy_native_host.c"
#include "src/runtime/xl6_dynamic_fields.h"

static const xstrview names[4]={{"list\0a",6},{"list\0b",6},{"array\0a",7},{"array\0b",7}};
extern bool __real_xrtDynamicFieldsMergeRefV1(xrtdynamicfields*,const xrtdynamicfields*,bool);
bool __wrap_xrtDynamicFieldsMergeRefV1(xrtdynamicfields* target,const xrtdynamicfields* source,bool replace)
{ return providerFails() ? false : __real_xrtDynamicFieldsMergeRefV1(target,source,replace); }

static xvalue* fieldsGraph(void)
{
    xvalue* object=xrtValueObject(),*list=create(true),*array=xrtValueArray();
    set(list,INT64_MIN); set(list,INT64_MAX);
    assert(object && array && xrtValueArrayAppendNew(array,xrtValueInt(17)));
    for (unsigned i=0;i<4;++i) {
        xvalue* owned=xrtValueRetain(i<2 ? list : array);
        assert(owned && xrtValueObjectSetTake(object,names[i],&owned)); assert(!owned);
    }
    xrtValueRelease(list); xrtValueRelease(array); return object;
}
static void* fieldsFromRefs(const xvalue* object)
{
    void* fields=xl6_xrt_dynamic_fields_create(); assert(fields);
    for (unsigned i=0;i<4;++i) assert(xrtDynamicFieldsSetRef(fields,names[i],xrtValueObjectGet(object,names[i])));
    return fields;
}
static xvalue* field(const xvalue* object,void* fields,unsigned i)
{ return fields ? xrtDynamicFieldsGetRef(fields,names[i]) : xrtValueRetain(xrtValueObjectGet(object,names[i])); }
static void fieldsObserved(unsigned op,const xvalue* original,xvalue* value,void* fields)
{
    xvalue* a=NULL,*b=NULL,*c=NULL,*d=NULL;
    if (op==1) a=xrtValueRetain(value);
    else if (op==2 || op==3) {
        assert(xrtValueCount(value)==4);
        const xvalue* entries[4];
        for (unsigned i=0;i<4;++i) {
            entries[i]=xrtValueArrayAt(value,i);
            if (op==3) {
                xstrview key={0}; assert(xrtValueGetString(xrtValueArrayAt(entries[i],0),&key) &&
                    key.Size==names[i].Size && !memcmp(key.Data,names[i].Data,key.Size));
                entries[i]=xrtValueArrayAt(entries[i],1);
            }
        }
        a=xrtValueRetain(entries[0]); b=xrtValueRetain(entries[1]);
        c=xrtValueRetain(entries[2]); d=xrtValueRetain(entries[3]);
    } else { a=field(value,fields,0); b=field(value,fields,1); c=field(value,fields,2); d=field(value,fields,3); }
    const xvalue* source=xrtValueObjectGet(original,names[0]);
    assert(a && !sameOwner(a,source)); textAt(a,INT64_MIN); textAt(a,INT64_MAX);
    if (op!=1) {
        assert(b && sameOwner(a,b) && c && d && c==d && c!=xrtValueObjectGet(original,names[2]));
        assert(xrtValueArrayAppendNew(c,xrtValueInt(19)) && xrtValueCount(d)==2 &&
            xrtValueCount(xrtValueObjectGet(original,names[2]))==1);
    }
    assert(xl6ListErasedClearV1(a)); textAt(source,INT64_MIN); textAt(source,INT64_MAX);
    xrtValueRelease(d); xrtValueRelease(c); xrtValueRelease(b); xrtValueRelease(a);
}
static bool fieldsAttempt(unsigned op,size_t point,unsigned mode)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    xvalue* original=fieldsGraph(); void* source=fieldsFromRefs(original),*target=xl6_xrt_dynamic_fields_create();
    xvalue* keep=xrtValueArray(); assert(target && keep && xrtDynamicFieldsSetRef(target,(xstrview){"keep",4},keep));
    xl6_error_scope boundary={0}; assert(xl6ErrorScopeBegin(&boundary));
    if (mode==0) assert(xrtMemDebugFailAfter(point));
    if (mode==1) { scopeSeen=0; scopeCountdown=(int)point+1; }
    if (mode==2) { providerSeen=0; providerCountdown=(unsigned)point+1; }
    xvalue* value=NULL; void* fields=NULL; bool ok=false;
    switch (op) {
    case 0: value=xl6_xrt_dynamic_fields_to_value(source); break;
    case 1: value=xl6_xrt_dynamic_fields_copy(source,names[0]); break;
    case 2: value=xl6_xrt_dynamic_fields_values(source); break;
    case 3: value=xl6_xrt_dynamic_fields_items(source); break;
    case 4: fields=xl6_xrt_dynamic_fields_from_value(original); break;
    case 5: fields=xl6_xrt_dynamic_fields_clone(source); break;
    case 6: ok=xl6_xrt_dynamic_fields_merge(target,source,true); break;
    default: assert(false);
    }
    if (op<4) ok=value!=NULL; else if (op<6) ok=fields!=NULL;
    bool hit=mode==0 ? xrtMemDebugFailTriggered() : mode==1 ? scopeSeen>=scopeCountdown :
        mode==2 ? providerSeen>=providerCountdown : false;
    scopeCountdown=0; providerCountdown=0; xrtMemDebugFailClear();
    if (!ok) xl6ErrorScopeCaptureFailure(&boundary);
    xl6_error_result outcome=xl6ErrorScopeEnd(&boundary);
    assert(ok==!hit && outcome.ok==!hit); xrtErrorFree(outcome.error);
    assert(xrtDynamicFieldsGet(target,(xstrview){"keep",4})==keep);
    if (op==6 && !ok) assert(xrtDynamicFieldsCount(target)==1);
    if (ok) fieldsObserved(op,original,value,op==6 ? target : fields);
    xl6_xrt_dynamic_fields_destroy(fields); xrtValueRelease(value); xrtValueRelease(keep);
    xl6_xrt_dynamic_fields_destroy(target); xl6_xrt_dynamic_fields_destroy(source); xrtValueRelease(original);
    xrtClearError(); assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    printf("DynamicFields List operation=%u point=%zu hit=%d balanced\n",op,point,hit); return hit;
}
static unsigned retiredDrops;
static void retiredDrop(void* raw,void* user)
{
    assert(raw==(void*)1); ++retiredDrops;
    if (user) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    else xrtSetErrorKind(XERR_IO);
}
static void cleanupVectors(void)
{
    for (unsigned silent=0;silent<2;++silent) {
        xl6ProductMemoryBegin(SIZE_MAX); retiredDrops=0;
        static const xvaluehandleops ops={NULL,retiredDrop,NULL,NULL};
        void* raw=(void*)1,*target=xl6_xrt_dynamic_fields_create();
        xvalue* owner=xrtValueHandleTake(&raw,&ops,silent ? (void*)1 : NULL); assert(owner && !raw);
        assert(xrtDynamicFieldsSetRef(target,names[0],owner)); xrtValueRelease(owner);
        xvalue* original=fieldsGraph(); void* source=fieldsFromRefs(original);
        xl6_error_scope boundary={0}; assert(xl6ErrorScopeBegin(&boundary));
        assert(!xl6_xrt_dynamic_fields_merge(target,source,true));
        xl6_error_result outcome=xl6ErrorScopeEnd(&boundary);
        assert(!outcome.ok && retiredDrops==1);
        assert(silent ? outcome.error==NULL : outcome.error && xrtErrorKind(outcome.error)==XERR_IO);
        xrtErrorFree(outcome.error);
        /* Commit preceded retirement: isolate source and retain result aliases,
         * do not claim rollback after arbitrary destructor effects. */
        assert(xrtDynamicFieldsCount(target)==4); fieldsObserved(6,original,NULL,target);
        xl6_xrt_dynamic_fields_destroy(source); xl6_xrt_dynamic_fields_destroy(target); xrtValueRelease(original);
        xrtClearError(); assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    }
    xl6ProductMemoryBegin(SIZE_MAX);
    xvalue* original=fieldsGraph(); void* source=fieldsFromRefs(original);
    xrtSetErrorKind(XERR_TIMEOUT); const xerror* prior=xrtGetError();
    xvalue* copy=xl6_xrt_dynamic_fields_to_value(source); assert(copy && xrtGetError()==prior);
    xrtClearError(); fieldsObserved(0,original,copy,NULL); xrtValueRelease(copy);
    xl6_xrt_dynamic_fields_destroy(source); xrtValueRelease(original);
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
}
int main(int argc,char** argv)
{
    assert(argc==2); unsigned mode=!strcmp(argv[1],"scopes") ? 1 : !strcmp(argv[1],"providers") ? 2 :
        !strcmp(argv[1],"vectors") ? 3 : 0; assert(mode || !strcmp(argv[1],"oom"));
    if (mode==3) cleanupVectors();
    for (unsigned op=0;op<7;++op) {
        bool closed=false;
        for (size_t point=0;point<4096;++point) {
            if (!fieldsAttempt(op,point,mode)) {
                printf("DynamicFields List closed-batch operation=%u runs=%zu first-no-hit=%zu\n",op,point+1,point);
                closed=true; break;
            }
        }
        assert(closed);
    }
    puts("DynamicFields mixed native List/wire graph: balanced"); return 0;
}
