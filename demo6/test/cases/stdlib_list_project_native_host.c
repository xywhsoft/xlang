#define XL6_VALUE_COPY_NO_MAIN
#include "stdlib_value_copy_native_host.c"
#include "src/runtime/xl6_persistence_project.h"
static void observed(unsigned operation,const xvalue* source,const xvalue* result)
{
    assert(xrtValueType(result)==XVALUE_ARRAY && xrtValueCount(result)==2);
    const xvalue* text=operation ? xrtValueArrayAt(xrtValueArrayAt(result,0),0) : xrtValueArrayAt(result,0);
    xstrview view={0}; assert(xrtValueGetString(text,&view) && view.Size==5 && !memcmp(view.Data,"a\0\xE4\xBD\xA0",5));
    if (operation) assert(xrtValueArrayAt(result,0)==xrtValueArrayAt(result,1));
    if (!operation) { textAt(source,INT64_MIN); textAt(source,INT64_MAX); }
    else { xvalue* child=childAt(source,operation==2 ? 0 : INT64_MIN); textAt(child,INT64_MIN); xrtValueRelease(child); }
}
static bool attempt(unsigned operation,size_t point,unsigned mode)
{
    xl6ProductMemoryBegin(SIZE_MAX); xvalue* source=copyInput(operation ? operation+2 : 0);
    const XL6_PersistencePlanV1 plan={.size=sizeof(plan)};
    xl6_error_scope boundary={0}; assert(xl6ErrorScopeBegin(&boundary));
    if (!mode) assert(xrtMemDebugFailAfter(point));
    else if (mode==1) { scopeSeen=0; scopeCountdown=(int)point+1; }
    else if (mode==2) { providerSeen=0; providerCountdown=(unsigned)point+1; }
    xvalue* result=xl6PersistenceProjectListV1(source,&plan);
    bool hit=!mode ? xrtMemDebugFailTriggered() : mode==1 ? scopeSeen>=scopeCountdown :
        mode==2 ? providerSeen>=providerCountdown : false;
    scopeCountdown=0; providerCountdown=0; xrtMemDebugFailClear();
    if (!result) xl6ErrorScopeCaptureFailure(&boundary);
    xl6_error_result outcome=xl6ErrorScopeEnd(&boundary);
    assert((result!=NULL)==!hit && outcome.ok==!hit); xrtErrorFree(outcome.error);
    if (result) observed(operation,source,result);
    xrtValueRelease(result); xrtValueRelease(source); xrtClearError();
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    printf("List project operation=%u point=%zu hit=%d balanced\n",operation,point,hit); return hit;
}
static void refusalVectors(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    const XL6_PersistencePlanV1 plan={.size=sizeof(plan)};
    xvalue* source=create(false); assert(!xl6PersistenceProjectListV1(source,&plan));
    checkError(XERR_STATE); xrtValueRelease(source);
    source=create(true); set(source,INT64_MIN);
    ptr raw=NULL,user=NULL; const xvaluehandleops* ops=NULL; XL6_ListGraphViewV1 view={0};
    assert(xrtValueGetHandle(source,&raw,&ops,&user) && xl6ListGraphBeginV1(ops,raw,user,xrtValueTypeId(source),&view)==1);
    assert(!xl6PersistenceProjectListV1(source,&plan)); checkError(XERR_STATE); xl6ListGraphEndV1(&view);
    xrtSetErrorKind(XERR_TIMEOUT); const xerror* incoming=xrtGetError();
    xvalue* result=xl6PersistenceProjectListV1(source,&plan); assert(result && xrtGetError()==incoming);
    xrtClearError(); xrtValueRelease(result); xrtValueRelease(source);
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
}
int main(int argc,char** argv)
{
    assert(argc==2); unsigned mode=!strcmp(argv[1],"scopes") ? 1 : !strcmp(argv[1],"providers") ? 2 :
        !strcmp(argv[1],"vectors") ? 3 : 0;
    assert(mode || !strcmp(argv[1],"oom"));
    if (mode==3) refusalVectors();
    for(unsigned operation=0;operation<3;++operation) {
        size_t point=0; while(point<4096 && attempt(operation,point,mode)) ++point;
        assert(point<4096); printf("List project closed-batch operation=%u runs=%zu first-no-hit=%zu\n",operation,point+1,point);
    }
    return 0;
}
