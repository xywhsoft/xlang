#include "src/runtime/xl6_dict.h"
#include "src/runtime/xl6_dict_methods.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t point,seen;
static bool armed;
static unsigned nativeDepth;
extern bool __real_xl6ErrorScopeBegin(xl6_error_scope*);
extern bool __real_xl6ErrorScopeBeginCleanup(xl6_error_scope*,bool);
static bool fail(void)
{ if (armed && nativeDepth && seen++==point) { xrtClearError(); return true; } return false; }
bool __wrap_xl6ErrorScopeBegin(xl6_error_scope* scope)
{ return fail() ? false : __real_xl6ErrorScopeBegin(scope); }
bool __wrap_xl6ErrorScopeBeginCleanup(xl6_error_scope* scope,bool failed)
{ return fail() ? false : __real_xl6ErrorScopeBeginCleanup(scope,failed); }
/* Inject checked native provider boundaries, not the generated unwind TLS
 * rebinding invariant. The latter is explicitly fatal and requires separate
 * OS-context recovery design; this gate never claims to certify it. */
#define NATIVE(Name,Args,Call) extern xvalue* __real_##Name Args; \
    xvalue* __wrap_##Name Args { ++nativeDepth; xvalue* result=__real_##Name Call; --nativeDepth; return result; }
NATIVE(xl6DictCreateTakeV1,(const XL6_DictLayoutV1* layout,xvalue* seed),(layout,seed))
NATIVE(xl6DictProjectionTakeV1,(xvalue* source,const XL6_DictLayoutV1* layout,const xvalueconverter* codec),(source,layout,codec))
NATIVE(xl6DictValueMergeRightV1,(const xvalue* left,const xvalue* right),(left,right))
NATIVE(xl6DictValueWithoutKeyV1,(const xvalue* left,xstrview text),(left,text))
#undef NATIVE
static bool attempt(size_t at,bool scopes)
{
    xl6ProductMemoryBegin(SIZE_MAX); assert(xlir_module_init());
    assert(xl6DictErasedProbeV1(XL6_DICT_FACTORY_NUMBERS)==1 &&
        xl6DictErasedProbeV1(XL6_DICT_FACTORY_NESTED)==1);
    int64_t size=0; assert(xl6DictErasedCountV1(XL6_DICT_FACTORY_NUMBERS,false,&size) && size==3);
    xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent));
    point=at; seen=0; armed=scopes;
    if (!scopes) assert(xrtMemDebugFailAfter(at));
    XL6_DICT_FACTORY_RESULT result=XL6_DICT_FACTORY_ROUNDTRIP();
    bool hit=scopes ? seen>at : xrtMemDebugFailTriggered(); armed=false; xrtMemDebugFailClear();
    if (!result.ok) xl6ErrorScopeCaptureFailure(&parent);
    xl6_error_result outcome=xl6ErrorScopeEnd(&parent);
    if (scopes && outcome.error) fprintf(stderr,"scope point=%zu kind=%d message=%s\n",
        at,(int)xrtErrorKind(outcome.error),xrtErrorMessage(outcome.error));
    assert(result.ok==!hit && outcome.ok==!hit);
    if (hit) { assert(!result.value); if (scopes) assert(!outcome.error); }
    else {
        assert(xl6DictErasedProbeV1(result.value)==1);
        assert(xl6DictErasedCountV1(result.value,false,&size) && size==2);
        xvalue* item=NULL; bool found=false; int64_t number=0;
        assert(xl6DictErasedReadV1(result.value,(xstrview){"a\0\xE4\xBD\xA0",5},false,&item,&found));
        assert(found && xrtValueGetInt(item,&number) && number==9); xrtValueRelease(item);
    }
    xrtErrorFree(outcome.error); xrtValueRelease(result.value); xrtClearError();
    xlir_module_fini(); assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    printf("Dict factory language mode=%s point=%zu hit=%d balanced\n",scopes ? "scopes" : "oom",at,hit);
    return hit;
}
int main(int argc,char** argv)
{
    assert(argc==2 && (!strcmp(argv[1],"oom") || !strcmp(argv[1],"scopes")));
    for (size_t at=0;at<4096;++at) if (!attempt(at,!strcmp(argv[1],"scopes"))) {
        printf("Dict factory language closed mode=%s runs=%zu first-no-hit=%zu\n",argv[1],at+1,at); return 0;
    }
    assert(false); return 1;
}
