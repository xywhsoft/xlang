#include "src/runtime/xl6_set.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static xvalue* create(void)
{
    XL6_SetLayoutV1 t={XL6_SET_ITERATE_OWNER,{XL6_SET_ITERATE_WITNESS,XL6_SET_ITERATE_ELEMENT,
        2,sizeof(xstrview),xl6StringTypeV1()}};
    xvalue* w=xl6SequencePrimitiveCreateV1(1,t.value.identity,t.value.elementIdentity,2,
        t.value.size,t.value.type,XL6_VALUE_ADMIT_STRING,false,t.value.elementIdentity); assert(w);
    xvalue* result=xl6SetProjectionTakeV1(xl6SetCreateTakeV1(&t,w),&t,xl6StringConverterV1()); assert(result);
    const xstrview items[]={{"a\0\xE4\xBD\xA0",5},{"second",6}};
    for (unsigned i=0;i<2;++i) { bool added=false; assert(xl6SetAddV1(result,&t,&items[i],&added) && added); }
    return result;
}
static size_t providerPoint,providerSeen;
static bool armed;
extern int __real_xl6SetIteratorSnapshotErasedV1(const xvalue*,bool,xvalue**,XL6_SetLayoutV1*,uint64_t*);
int __wrap_xl6SetIteratorSnapshotErasedV1(const xvalue* value,bool reverse,xvalue** out,XL6_SetLayoutV1* layout,uint64_t* identity)
{
    if (armed && providerSeen++==providerPoint) { xrtClearError(); return -1; }
    return __real_xl6SetIteratorSnapshotErasedV1(value,reverse,out,layout,identity);
}
extern bool __real_xl6SetIteratorAdvanceValueV1(const xvalue*,const XL6_SetLayoutV1*,uint64_t,bool*,xvalue**);
bool __wrap_xl6SetIteratorAdvanceValueV1(const xvalue* value,const XL6_SetLayoutV1* layout,uint64_t identity,bool* found,xvalue** out)
{
    if (armed && providerSeen++==providerPoint) { xrtClearError(); return false; }
    return __real_xl6SetIteratorAdvanceValueV1(value,layout,identity,found,out);
}
static bool attempt(unsigned function,size_t point,bool providers)
{
    xl6ProductMemoryBegin(SIZE_MAX); assert(xlir_module_init()); xvalue* set=create();
    int64_t before=XL6_SET_ITERATE_CLEANUPS;
    xl6_error_scope boundary={0}; assert(xl6ErrorScopeBegin(&boundary));
    providerPoint=point; providerSeen=0; armed=providers;
    if (!providers) assert(xrtMemDebugFailAfter(point));
    bool ok; int64_t count;
    if (!function) { XL6_SET_ITERATE_TYPED_RESULT r=XL6_SET_ITERATE_TYPED(set); ok=r.ok; count=r.value; }
    else { XL6_SET_ITERATE_ERASED_RESULT r=XL6_SET_ITERATE_ERASED(set); ok=r.ok; count=r.value; }
    bool hit=providers ? providerSeen>point : xrtMemDebugFailTriggered(); armed=false; xrtMemDebugFailClear();
    if (!ok) xl6ErrorScopeCaptureFailure(&boundary);
    xl6_error_result outcome=xl6ErrorScopeEnd(&boundary);
    assert(ok==!hit && outcome.ok==!hit && XL6_SET_ITERATE_CLEANUPS==before+1);
    if (ok) assert(count==2);
    xrtErrorFree(outcome.error); xrtValueRelease(set); xrtClearError(); xlir_module_fini();
    assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    printf("Set generated function=%u mode=%s point=%zu hit=%d balanced\n",function,providers ? "providers" : "oom",point,hit);
    return hit;
}
int main(int argc,char** argv)
{
    assert(argc==2); bool providers=!strcmp(argv[1],"providers"); assert(providers || !strcmp(argv[1],"oom"));
    for (unsigned function=0;function<2;++function) {
        size_t point=0; while(point<4096 && attempt(function,point,providers)) ++point;
        assert(point<4096); printf("Set generated closed function=%u mode=%s runs=%zu first-no-hit=%zu\n",
            function,providers ? "providers" : "oom",point+1,point);
    }
    puts("Generated native Set iteration balanced"); return 0;
}
