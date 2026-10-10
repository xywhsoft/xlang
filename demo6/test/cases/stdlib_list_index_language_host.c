#define XL6_TEST_LIST_INDEX_NO_MAIN
#include "stdlib_list_index_native_host.c"
static int providerCountdown,providerSeen;
static bool providerFailure(void)
{ if (!providerCountdown || ++providerSeen!=providerCountdown) return false; xrtClearError(); return true; }
extern bool __real_xl6ListErasedReadV1(const xvalue*,int64_t,bool,xvalue**,bool*);
bool __wrap_xl6ListErasedReadV1(const xvalue* source,int64_t key,bool take,xvalue** output,bool* found)
{ return !providerFailure() && __real_xl6ListErasedReadV1(source,key,take,output,found); }
extern bool __real_xl6ListErasedSetV1(const xvalue*,int64_t,const xvalue*);
bool __wrap_xl6ListErasedSetV1(const xvalue* source,int64_t key,const xvalue* item)
{ return !providerFailure() && __real_xl6ListErasedSetV1(source,key,item); }
extern bool __real_xl6ListErasedContainsV1(const xvalue*,const xvalue*,bool*);
bool __wrap_xl6ListErasedContainsV1(const xvalue* source,const xvalue* item,bool* found)
{ return !providerFailure() && __real_xl6ListErasedContainsV1(source,item,found); }
static bool languageAttempt(unsigned operation,size_t point,bool providers)
{
    xl6ProductMemoryBegin(SIZE_MAX); assert(xlir_module_init());
    xvalue* list=create(true),*key=xrtValueInt(INT64_MIN),*needle=query(),*result=NULL;
    assert(key && needle); set(list,INT64_MIN); set(list,INT64_MAX);
    int64_t before=XL6_INDEX_CLEANUPS; xl6_error_scope boundary={0}; assert(xl6ErrorScopeBegin(&boundary));
    if (providers) { providerSeen=0; providerCountdown=(int)point+1; } else assert(xrtMemDebugFailAfter(point));
    bool success;
    if (!operation) { XL6_INDEX_READ_RESULT r=XL6_INDEX_READ(list,key); success=r.ok; result=r.value; }
    else if (operation==1) { XL6_INDEX_WRITE_RESULT r=XL6_INDEX_WRITE(list,key,needle); success=r.ok; if (success) assert(r.value); }
    else { XL6_INDEX_CONTAINS_RESULT r=XL6_INDEX_CONTAINS(list,needle); success=r.ok; if (success) assert(r.value); }
    bool hit=providers ? providerSeen>=providerCountdown : xrtMemDebugFailTriggered();
    providerCountdown=0; xrtMemDebugFailClear();
    if (!success) xl6ErrorScopeCaptureFailure(&boundary);
    xl6_error_result outcome=xl6ErrorScopeEnd(&boundary);
    assert(success==!hit && outcome.ok==!hit && XL6_INDEX_CLEANUPS==before+1);
    if (result) exact(result,"a\0\xE4\xBD\xA0",5);
    if (hit) assert(!result && xl6ListValueCountV1(list)==2);
    xrtErrorFree(outcome.error); xrtValueRelease(result); xrtValueRelease(needle); xrtValueRelease(key); xrtValueRelease(list);
    xrtClearError(); xlir_module_fini(); assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    printf("List index language operation=%u point=%zu hit=%d balanced\n",operation,point,hit); return hit;
}
int main(int argc,char** argv)
{
    assert(argc==2); bool providers=!strcmp(argv[1],"providers"); assert(providers || !strcmp(argv[1],"oom"));
    for (unsigned operation=0;operation<3;++operation) {
        bool closed=false;
        for (size_t point=0;point<4096;++point) if (!languageAttempt(operation,point,providers)) {
            printf("List index closed-batch operation=%u runs=%zu first-no-hit=%zu\n",operation,point+1,point);
            closed=true; break;
        }
        assert(closed);
    }
    return 0;
}
