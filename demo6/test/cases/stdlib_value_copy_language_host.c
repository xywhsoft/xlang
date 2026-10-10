#define XL6_VALUE_COPY_NO_MAIN
#include "stdlib_value_copy_native_host.c"
static xvalue* languageInput(unsigned operation)
{
    XL6_ListLayoutV1 t={XL6_COPY_OWNER,{XL6_COPY_SCHEMA,XL6_COPY_ELEMENT,2,sizeof(xstrview),xl6StringTypeV1()}};
    xvalue* child=profile(&t,XL6_VALUE_ADMIT_STRING,false,t.value.elementIdentity,xl6StringConverterV1());
    xstrview text={"a\0\xE4\xBD\xA0",5};
    assert(xl6ListSetV1(child,&t,INT64_MIN,&text) && xl6ListSetV1(child,&t,INT64_MAX,&text));
    if (operation<3) return child;
    xvalue* source=NULL;
    if (operation==4) {
        source=xrtValueArray(); assert(source && xrtValueArrayAppend(source,child) && xrtValueArrayAppend(source,child));
    } else {
        t=tuple(); t.identity+=0x100; t.value.identity+=0x100; t.value.elementIdentity+=0x100;
        t.value.transfer=3; t.value.size=sizeof(xvalue*); t.value.type=xrtTypeValue();
        source=profile(&t,XL6_VALUE_ADMIT_ANY,false,0,NULL);
        assert(xl6ListSetV1(source,&t,INT64_MIN,&child) && xl6ListSetV1(source,&t,INT64_MAX,&child));
    }
    xrtValueRelease(child); return source;
}
static xvalue* languageOperation(unsigned operation,xvalue* source,bool* success)
{
    if (!operation) { XL6_COPY_copyOpen_RESULT r=XL6_COPY_copyOpen(source); *success=r.ok; return r.value; }
    if (operation==2) { XL6_COPY_cloneList_RESULT r=XL6_COPY_cloneList(source); *success=r.ok; return r.value; }
    XL6_COPY_cloneOpen_RESULT r=XL6_COPY_cloneOpen(source); *success=r.ok; return r.value;
}
static bool languageAttempt(unsigned operation,size_t point,unsigned mode)
{
    xl6ProductMemoryBegin(SIZE_MAX); assert(xlir_module_init());
    xvalue* source=languageInput(operation); int64_t before=XL6_COPY_CLEANUPS;
    xl6_error_scope boundary={0}; assert(xl6ErrorScopeBegin(&boundary));
    if (!mode) assert(xrtMemDebugFailAfter(point));
    else if (mode==2) { providerSeen=0; providerCountdown=(unsigned)point+1; }
    bool success=false; xvalue* result=languageOperation(operation,source,&success);
    bool hit=!mode ? xrtMemDebugFailTriggered() : mode==2 ? providerSeen>=providerCountdown : false;
    providerCountdown=0; xrtMemDebugFailClear(); if (!success) xl6ErrorScopeCaptureFailure(&boundary);
    xl6_error_result outcome=xl6ErrorScopeEnd(&boundary);
    assert(success==!hit && outcome.ok==!hit && (result!=NULL)==!hit && XL6_COPY_CLEANUPS==before+1);
    xrtErrorFree(outcome.error);
    if (result) copyObserved(operation==2 ? 1 : operation,source,result);
    xrtValueRelease(result); xrtValueRelease(source); xrtClearError(); xlir_module_fini();
    assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    printf("Value copy language operation=%u point=%zu hit=%d balanced\n",operation,point,hit); return hit;
}
int main(int argc,char** argv)
{
    assert(argc==2); unsigned mode=!strcmp(argv[1],"providers") ? 2 : !strcmp(argv[1],"vectors") ? 3 : 0;
    assert(mode || !strcmp(argv[1],"oom"));
    for (unsigned operation=0;operation<5;++operation) {
        if (mode==3) { assert(!languageAttempt(operation,0,mode)); continue; }
        bool closed=false;
        for (size_t point=0;point<4096;++point) if (!languageAttempt(operation,point,mode)) {
            printf("Value copy language closed-batch operation=%u runs=%zu first-no-hit=%zu\n",operation,point+1,point);
            closed=true; break;
        }
        assert(closed);
    }
    puts("Generated copy/clone genuine native List graph: balanced"); return 0;
}
