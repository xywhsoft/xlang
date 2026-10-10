/* Genuine native ANY List inputs to actual generated checked functions. */
#include "src/runtime/xl6_list.h"
#include "src/runtime/xl6_dynamic_collection.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
static XL6_ListLayoutV1 layout(void)
{ return (XL6_ListLayoutV1){XL6_REFINED_OWNER,{XL6_REFINED_WITNESS,XL6_REFINED_ELEMENT,3,sizeof(xvalue*),xrtTypeValue()}}; }
static xvalue* create(bool projected)
{
    XL6_ListLayoutV1 t=layout();
    xvalue* witness=xl6SequenceReferenceCreateV1(1,t.value.identity,t.value.elementIdentity,
        XL6_VALUE_ADMIT_ANY,false,t.value.elementIdentity); assert(witness);
    xvalue* value=xl6ListCreateTakeV1(&t,witness); assert(value);
    if (projected) { value=xl6ListProjectionTakeV1(value,&t,NULL); assert(value); }
    const int64_t keys[]={INT64_MIN,0,INT64_MAX};
    for (unsigned i=0;i<3;++i) {
        xvalue* item=xrtValueString((xstrview){"a\0\xE4\xBD\xA0",5}); assert(item);
        assert(xl6ListSetV1(value,&t,keys[i],&item)); xrtValueRelease(item);
    }
    return value;
}
static int scopeCountdown,scopeSeen;
static bool fail(void)
{
    if (!scopeCountdown || ++scopeSeen!=scopeCountdown) return false;
    xrtClearError(); return true;
}
extern int __real_xl6ListIteratorSnapshotErasedV1(const xvalue*,xvalue**,XL6_ListLayoutV1*,uint64_t*);
int __wrap_xl6ListIteratorSnapshotErasedV1(const xvalue* value,xvalue** output,XL6_ListLayoutV1* t,uint64_t* identity)
{ return fail() ? -1 : __real_xl6ListIteratorSnapshotErasedV1(value,output,t,identity); }
extern bool __real_xl6ListIteratorAdvanceValueV1(const xvalue*,const XL6_ListLayoutV1*,uint64_t,bool*,xvalue**,int64_t*);
bool __wrap_xl6ListIteratorAdvanceValueV1(const xvalue* value,const XL6_ListLayoutV1* t,
    uint64_t identity,bool* found,xvalue** output,int64_t* key)
{ return !fail() && __real_xl6ListIteratorAdvanceValueV1(value,t,identity,found,output,key); }
extern bool __real_xl6ListErasedContainsV1(const xvalue*,const xvalue*,bool*);
bool __wrap_xl6ListErasedContainsV1(const xvalue* source,const xvalue* item,bool* found)
{ return !fail() && __real_xl6ListErasedContainsV1(source,item,found); }
extern bool __real_xl6ListErasedKeyV1(const xvalue*,int64_t,bool,bool*);
bool __wrap_xl6ListErasedKeyV1(const xvalue* source,int64_t key,bool remove,bool* found)
{ return !fail() && __real_xl6ListErasedKeyV1(source,key,remove,found); }
extern bool __real_xl6ListErasedReadV1(const xvalue*,int64_t,bool,xvalue**,bool*);
bool __wrap_xl6ListErasedReadV1(const xvalue* source,int64_t key,bool take,xvalue** output,bool* found)
{ return !fail() && __real_xl6ListErasedReadV1(source,key,take,output,found); }
extern bool __real_xl6ListErasedClearV1(const xvalue*);
bool __wrap_xl6ListErasedClearV1(const xvalue* source)
{ return !fail() && __real_xl6ListErasedClearV1(source); }
static bool attempt(unsigned function,size_t point,bool scopes)
{
    xl6ProductMemoryBegin(SIZE_MAX); assert(xlir_module_init()); xvalue* list=create(true);
    int64_t before=XL6_REFINED_CLEANUPS; xl6_error_scope boundary={0}; assert(xl6ErrorScopeBegin(&boundary));
    if (scopes) { scopeSeen=0; scopeCountdown=(int)point+1; } else assert(xrtMemDebugFailAfter(point));
    bool ok; int64_t count;
    if (!function) { XL6_REFINED_REFINED_RESULT result=XL6_REFINED_REFINED(list); ok=result.ok; count=result.value; }
    else { XL6_REFINED_DYNAMIC_RESULT result=XL6_REFINED_DYNAMIC(list); ok=result.ok; count=result.value; }
    bool hit=scopes ? scopeSeen>=scopeCountdown : xrtMemDebugFailTriggered(); scopeCountdown=0; xrtMemDebugFailClear();
    if (!ok) xl6ErrorScopeCaptureFailure(&boundary);
    xl6_error_result outcome=xl6ErrorScopeEnd(&boundary);
    assert(ok==!hit && outcome.ok==!hit && XL6_REFINED_CLEANUPS==before+1);
    if (ok) assert(count==(function ? 1 : 3));
    xrtErrorFree(outcome.error); xrtValueRelease(list); xrtClearError();
    xlir_module_fini(); assert(!xrtGetError()); assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    printf("Refined native List function=%u point=%zu hit=%d balanced\n",function,point,hit); return hit;
}
static void foreignDrop(void* value,void* user) { (void)value; (void)user; }
static const xvaluehandleops foreignOps={.Drop=foreignDrop};
static void rejects(void)
{
    xl6ProductMemoryBegin(SIZE_MAX); assert(xlir_module_init());
    xvalue* list=create(true); XL6_ListLayoutV1 t=layout();
    for (unsigned op=1;op<=12;++op) {
        if (op==XL6_DYNAMIC_COLLECTION_REMOVE || op==XL6_DYNAMIC_COLLECTION_TAKE ||
            op==XL6_DYNAMIC_COLLECTION_CLEAR || op==XL6_DYNAMIC_COLLECTION_CONTAINS) continue;
        size_t arity=op==XL6_DYNAMIC_COLLECTION_POP ? 0 :
            op==XL6_DYNAMIC_COLLECTION_INSERT || op==XL6_DYNAMIC_COLLECTION_SWAP ? 2 : 1;
        assert(!xl6DynamicCollectionCallTakeV1(list,op,arity ? xrtValueInt(0) : NULL,arity==2 ? xrtValueInt(1) : NULL));
        assert(xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError();
        assert(xl6ListCountV1(list,&t)==3 && !xrtGetError());
    }
    assert(!xl6DynamicCollectionCallTakeV1(list,XL6_DYNAMIC_COLLECTION_REMOVE,xrtValueInt(0),xrtValueInt(1)));
    assert(xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError();
    xvalue* unprojected=create(false);
    assert(!xl6DynamicCollectionCallTakeV1(unprojected,XL6_DYNAMIC_COLLECTION_CLEAR,NULL,NULL));
    assert(xrtGetError()); xrtClearError(); xrtValueRelease(unprojected);
    void* raw=(void*)1; xvalue* foreign=xrtValueHandleTake(&raw,&foreignOps,NULL); assert(foreign && !raw);
    assert(xrtValueTypeIdBind(foreign,t.identity));
    assert(!xl6DynamicCollectionCallTakeV1(foreign,XL6_DYNAMIC_COLLECTION_CLEAR,NULL,NULL));
    assert(xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError(); xrtValueRelease(foreign);
    xvalue* stack=xl6SequenceReferenceCreateV1(XL6_SEQUENCE_STACK_V1,t.value.identity,t.value.elementIdentity,
        XL6_VALUE_ADMIT_ANY,false,t.value.elementIdentity); assert(stack);
    assert(!xl6DynamicCollectionCallTakeV1(stack,XL6_DYNAMIC_COLLECTION_CLEAR,NULL,NULL));
    assert(xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError(); xrtValueRelease(stack);
    xrtValueRelease(list); xlir_module_fini(); assert(!xrtGetError());
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify(); puts("Native List unsupported/foreign/unprojected/Stack requests rejected and balanced");
}
int main(int argc,char** argv)
{
    assert(argc==1 || argc==2);
    if (argc==2 && !strcmp(argv[1],"rejects")) { rejects(); return 0; }
    bool scopes=argc==2 && !strcmp(argv[1],"scopes");
    for (unsigned function=0;function<2;++function) {
        bool complete=false;
        for (size_t point=0;point<4096;++point) if (!attempt(function,point,scopes)) {
            printf("Refined closed-batch function=%u runs=%zu first-no-hit=%zu\n",function,point+1,point);
            complete=true; break;
        }
        assert(complete);
    }
    puts("Refined/native List language consumers complete and balanced"); return 0;
}
