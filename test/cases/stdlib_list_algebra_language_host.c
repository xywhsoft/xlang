#define XL6_LIST_ALGEBRA_NO_MAIN
#include "stdlib_list_algebra_native_host.c"
static xvalue* languageInput(bool right)
{
    XL6_ListLayoutV1 t={XL6_ALG_copyList_OWNER,{XL6_ALG_copyList_SCHEMA,
        XL6_ALG_copyList_ELEMENT,2,sizeof(xstrview),xl6StringTypeV1()}};
    assert(XL6_ALG_mergeList_OWNER==t.identity && XL6_ALG_withoutList_OWNER==t.identity);
    xvalue* list=profile(&t,XL6_VALUE_ADMIT_STRING,false,t.value.elementIdentity,xl6StringConverterV1());
    xstrview text=right ? (xstrview){"r\0x",3} : (xstrview){"a\0\xE4\xBD\xA0",5};
    assert(xl6ListSetV1(list,&t,INT64_MIN,&text)); text=(xstrview){"z",1};
    assert(xl6ListSetV1(list,&t,right ? 17 : INT64_MAX,&text)); return list;
}
static xvalue* languageOperation(unsigned operation,xvalue* left,xvalue* right,bool* success)
{
    XL6_ALG_copyList_RESULT result;
    if (!operation) result=XL6_ALG_copyList(left);
    else if (operation==1) result=XL6_ALG_mergeList(left,right);
    else result=XL6_ALG_withoutList(left,operation==2 ? INT64_MIN : -17);
    *success=result.ok; return result.value;
}
static bool languageAttempt(unsigned operation,size_t point,unsigned mode)
{
    xl6ProductMemoryBegin(SIZE_MAX); assert(xlir_module_init());
    xvalue* left=languageInput(false),*right=languageInput(true); int64_t before=XL6_ALG_CLEANUPS;
    xl6_error_scope boundary={0}; assert(xl6ErrorScopeBegin(&boundary));
    if (mode==1) { scopeSeen=0; scopeCountdown=(int)point+1; }
    else if (mode==2) { providerSeen=0; providerCountdown=(unsigned)point+1; }
    else if (!mode) assert(xrtMemDebugFailAfter(point));
    bool success=false; xvalue* result=languageOperation(operation,left,right,&success);
    bool hit=mode==1 ? scopeSeen>=scopeCountdown : mode==2 ? providerSeen>=providerCountdown : mode==3 ? false : xrtMemDebugFailTriggered();
    scopeCountdown=0; providerCountdown=0; xrtMemDebugFailClear();
    if (!success) xl6ErrorScopeCaptureFailure(&boundary);
    xl6_error_result outcome=xl6ErrorScopeEnd(&boundary);
    if (!(success==!hit && outcome.ok==!hit && (result!=NULL)==!hit && XL6_ALG_CLEANUPS==before+1))
        fprintf(stderr,"algebra language op=%u point=%zu hit=%d success=%d outcome=%d result=%p cleanup=%lld/%lld\n",
            operation,point,hit,success,outcome.ok,(void*)result,(long long)XL6_ALG_CLEANUPS,(long long)before);
    assert(success==!hit && outcome.ok==!hit && (result!=NULL)==!hit && XL6_ALG_CLEANUPS==before+1);
    xrtErrorFree(outcome.error); unchanged(left,right);
    if (result) {
        assert(xl6ListErasedProbeV1(result)==1);
        assert(xl6ListValueCountV1(result)==(operation==1 ? 3 : operation==2 ? 1 : 2));
        algebraText(result,INT64_MIN,operation==2 ? NULL : operation==1 ? "r\0x" : "a\0\xE4\xBD\xA0",operation==1 ? 3 : 5);
        xvalue* text=xrtValueString((xstrview){"changed",7}); assert(text && xl6ListErasedSetV1(result,INT64_MAX,text));
        xrtValueRelease(text); unchanged(left,right);
    }
    xrtValueRelease(result); xrtValueRelease(right); xrtValueRelease(left);
    xrtClearError(); xlir_module_fini(); assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    printf("List algebra language operation=%u point=%zu hit=%d balanced\n",operation,point,hit); return hit;
}
int main(int argc,char** argv)
{
    assert(argc==2);
    unsigned mode=!strcmp(argv[1],"scopes") ? 1 : !strcmp(argv[1],"providers") ? 2 : !strcmp(argv[1],"vectors") ? 3 : 0;
    assert(mode || !strcmp(argv[1],"oom"));
    for (unsigned operation=0;operation<4;++operation) {
        if (mode==3) { assert(!languageAttempt(operation,0,mode)); continue; }
        bool closed=false;
        for (size_t point=0;point<4096;++point) if (!languageAttempt(operation,point,mode)) {
            printf("List algebra language closed-batch operation=%u runs=%zu first-no-hit=%zu\n",operation,point+1,point); closed=true; break;
        }
        assert(closed);
    }
    puts("Generated List algebra accepts genuine native inputs: balanced"); return 0;
}
