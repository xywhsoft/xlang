#define main xl6DynamicFieldsListMain
#include "stdlib_dynamic_fields_list_host.c"
#undef main
static bool restoreAttempt(size_t point,unsigned mode)
{
    xl6ProductMemoryBegin(SIZE_MAX); assert(xlir_module_init());
    xvalue* original=fieldsGraph(),*owned=xrtValueRetain(original); assert(owned);
    assert(xrtValueObjectSetTake(XL6_RESTORE_COPY_SHARED,(xstrview){"payload",7},&owned) && !owned);
    xl6_error_scope boundary={0}; assert(xl6ErrorScopeBegin(&boundary));
    if (!mode) assert(xrtMemDebugFailAfter(point));
    else if (mode==2) { providerSeen=0; providerCountdown=(unsigned)point+1; }
    xstrview text={"{\"child\":{\"n\":31}}",18};
    XL6_RESTORE_COPY_RESULT result=XL6_RESTORE_COPY_FN(text);
    bool hit=!mode ? xrtMemDebugFailTriggered() : mode==2 ? providerSeen>=providerCountdown : false;
    providerCountdown=0; xrtMemDebugFailClear(); if (!result.ok) xl6ErrorScopeCaptureFailure(&boundary);
    xl6_error_result outcome=xl6ErrorScopeEnd(&boundary);
    assert(result.ok==!hit && outcome.ok==!hit); xrtErrorFree(outcome.error);
    const xvalue* child=result.ok ? xrtValueObjectGet(result.value,(xstrview){"child",5}) : NULL;
    if (child) {
        xvalue* isolated=xrtValueObjectGet(child,(xstrview){"payload",7});
        assert(isolated && isolated!=original); fieldsObserved(0,original,isolated,NULL);
    }
    assert(xrtValueObjectGet(XL6_RESTORE_COPY_SHARED,(xstrview){"payload",7})==original);
    int64 n=0; assert(xrtValueGetInt(xrtValueObjectGet(XL6_RESTORE_COPY_SHARED,(xstrview){"n",1}),&n) && n==11);
    xrtValueRelease(result.value); xrtValueRelease(original); xrtClearError(); xlir_module_fini();
    assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    printf("Restore List point=%zu hit=%d balanced\n",point,hit); return hit;
}
int main(int argc,char** argv)
{
    assert(argc==2); unsigned mode=!strcmp(argv[1],"providers") ? 2 : !strcmp(argv[1],"vectors") ? 3 : 0;
    assert(mode || !strcmp(argv[1],"oom")); bool closed=false;
    for (size_t point=0;point<4096;++point) if (!restoreAttempt(point,mode)) {
        printf("Restore List closed-batch runs=%zu first-no-hit=%zu\n",point+1,point); closed=true; break;
    }
    assert(closed); puts("Generated shared-default native List restore: balanced"); return 0;
}
