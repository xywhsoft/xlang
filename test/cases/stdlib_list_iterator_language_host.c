/* Actual generated functions and TypeIds extracted by the real IR contract. */
#include "src/runtime/xl6_list.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
static XL6_ListLayoutV1 layout(void)
{ return (XL6_ListLayoutV1){XL6_ITERATE_OWNER,{XL6_ITERATE_WITNESS,XL6_ITERATE_ELEMENT,2,sizeof(xstrview),xl6StringTypeV1()}}; }
static xvalue* create(void)
{
    XL6_ListLayoutV1 t=layout();
    xvalue* witness=xl6SequencePrimitiveCreateV1(1,t.value.identity,t.value.elementIdentity,
        t.value.transfer,t.value.size,t.value.type,XL6_VALUE_ADMIT_STRING,false,t.value.elementIdentity); assert(witness);
    xvalue* value=xl6ListProjectionTakeV1(xl6ListCreateTakeV1(&t,witness),&t,xl6StringConverterV1()); assert(value);
    xstrview text={"a\0\xE4\xBD\xA0",5}; const int64_t keys[]={INT64_MIN,0,INT64_MAX};
    for (unsigned i=0;i<3;++i) assert(xl6ListSetV1(value,&t,keys[i],&text));
    return value;
}
static int scopeCountdown,scopeSeen;
extern int __real_xl6ListIteratorSnapshotErasedV1(const xvalue*,xvalue**,XL6_ListLayoutV1*,uint64_t*);
int __wrap_xl6ListIteratorSnapshotErasedV1(const xvalue* value,xvalue** output,
    XL6_ListLayoutV1* layout,uint64_t* identity)
{
    if (scopeCountdown && ++scopeSeen==scopeCountdown) { xrtClearError(); return -1; }
    return __real_xl6ListIteratorSnapshotErasedV1(value,output,layout,identity);
}
extern bool __real_xl6ListIteratorAdvanceValueV1(const xvalue*,const XL6_ListLayoutV1*,uint64_t,bool*,xvalue**,int64_t*);
bool __wrap_xl6ListIteratorAdvanceValueV1(const xvalue* value,const XL6_ListLayoutV1* layout,
    uint64_t identity,bool* found,xvalue** output,int64_t* key)
{
    if (scopeCountdown && ++scopeSeen==scopeCountdown) { xrtClearError(); return false; }
    return __real_xl6ListIteratorAdvanceValueV1(value,layout,identity,found,output,key);
}
static bool attempt(unsigned function,size_t point,bool scopes)
{
    xl6ProductMemoryBegin(SIZE_MAX); assert(xlir_module_init()); xvalue* list=create();
    int64_t before=XL6_ITERATE_CLEANUPS; xl6_error_scope boundary={0}; assert(xl6ErrorScopeBegin(&boundary));
    if (scopes) { scopeSeen=0; scopeCountdown=(int)point+1; } else assert(xrtMemDebugFailAfter(point));
    bool ok; int64_t count;
    if (!function) { XL6_ITERATE_LIST_RESULT result=XL6_ITERATE_LIST(list); ok=result.ok; count=result.value; }
    else if (function==1) { XL6_ITERATE_ERASED_RESULT result=XL6_ITERATE_ERASED(list); ok=result.ok; count=result.value; }
    else { XL6_ITERATE_BREAK_RESULT result=XL6_ITERATE_BREAK(list); ok=result.ok; count=result.value; }
    bool hit=scopes ? scopeSeen>=scopeCountdown : xrtMemDebugFailTriggered(); scopeCountdown=0; xrtMemDebugFailClear();
    if (!ok) xl6ErrorScopeCaptureFailure(&boundary);
    xl6_error_result outcome=xl6ErrorScopeEnd(&boundary);
    assert(ok==!hit && outcome.ok==!hit && XL6_ITERATE_CLEANUPS==before+1);
    if (ok) assert(count==(function==2 ? 1 : 3));
    xrtErrorFree(outcome.error); xrtValueRelease(list); xrtClearError();
    xlir_module_fini(); assert(!xrtGetError()); assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    printf("Generated List foreach function=%u point=%zu hit=%d balanced\n",function,point,hit); return hit;
}
int main(int argc,char** argv)
{
    assert(argc==1 || argc==2); bool scopes=argc==2 && !strcmp(argv[1],"scopes");
    for (unsigned function=0;function<3;++function) {
        bool complete=false;
        for (size_t point=0;point<4096;++point) if (!attempt(function,point,scopes)) {
            printf("Generated List closed-batch function=%u runs=%zu first-no-hit=%zu\n",function,point+1,point);
            complete=true; break;
        }
        assert(complete);
    }
    puts("Generated/native List foreach complete and balanced"); return 0;
}
