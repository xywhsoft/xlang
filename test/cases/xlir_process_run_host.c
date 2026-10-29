#include "src/runtime/xl6_process_internal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static xstrview view(const char* text){return (xstrview){text,strlen(text)};}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(after.DoubleFreeCount==before->DoubleFreeCount && after.InvalidFreeCount==before->InvalidFreeCount);
    assert(after.UseAfterFreeCount==before->UseAfterFreeCount);
}
static xvalue* args(const char* first,const char* second)
{
    xvalue* result=xrtValueArray();assert(result);
    xvalue* item=xrtValueString(view(first));assert(item && xrtValueArrayAppendNew(result,item));
    if(second){item=xrtValueString(view(second));assert(item && xrtValueArrayAppendNew(result,item));}
    return result;
}
static void* direct(const char* child,const char* first,const char* second)
{
    xvalue* argv=args(first,second);
    void* result=xl6ProcessCommandDirectV1(view(child),argv,-1,-1);
    xrtValueRelease(argv);assert(result);return result;
}
static unsigned configSweep(unsigned mode)
{
    unsigned hits=0;
    for(size_t point=0;point<128;++point){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        xvalue* argv=args("argument","");assert(argv);
        void* handle=NULL;
        if(mode==1){handle=xl6ProcessCommandDirectV1(view("program"),argv,-1,-1);assert(handle);}
        assert(xrtMemDebugFailAfter(point));
        if(mode==0)handle=xl6ProcessCommandDirectV1(view("program"),argv,-1,-1);
        else if(mode==1)xl6ProcessCommandEnvV1(handle,view("ENV"),view("value"),-1,-1);
        else handle=xl6ProcessRunOptionsCreateV1(-1,1024,1024,0,250,0);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(hit){assert(xrtErrorKind(xrtGetError())==XERR_MEMORY);++hits;}
        else assert(handle && !xrtGetError());
        if(mode==2)xl6ProcessRunOptionsDestroyV1(handle);else xl6ProcessCommandDestroyV1(handle);
        xrtValueRelease(argv);xrtClearError();balanced(&before);
        if(!hit){assert(hits);return hits;}
    }
    abort();
}
static int32 wrongOwner(void* data)
{
    void* h=data;
#define DENY(Call) do{xrtClearError();Call;assert(xrtErrorKind(xrtGetError())==XERR_STATE);}while(0)
    DENY(xl6ProcessCommandWorkDirV1(h,view("."),-1));
    DENY(xl6ProcessCommandArg0V1(h,view("other"),-1));
    DENY(xl6ProcessCommandEnvV1(h,view("ENV"),view("other"),-1,-1));
    DENY(xl6ProcessCommandRemoveEnvV1(h,view("ENV"),-1));
    DENY(xl6ProcessCommandClearEnvV1(h));
    DENY(xl6ProcessCommandInheritEnvV1(h,false));
    DENY(xl6ProcessCommandHideWindowV1(h,false));
    DENY(xl6ProcessCommandNewConsoleV1(h,true));
    DENY(xl6ProcessCommandProcessGroupV1(h,false));
    DENY(xl6ProcessCommandTerminalV1(h,true));
    DENY(xl6ProcessCommandTerminalSizeV1(h,10,10));
    DENY(xl6ProcessCommandStdioV1(h,0,0));
    DENY((void)xl6ProcessRunV1(h,NULL,NULL,-1));
#undef DENY
    xrtClearError();return 0;
}
static int32 cancelLater(void* data){xrtSleep(10);assert(xl6ProcessRunOptionsCancelV1(data));return 0;}
int main(int argc,char** argv)
{
    assert(argc==2 && xrtMemDebugEnable(true));
    unsigned constructor=configSweep(0),environment=configSweep(1),options=configSweep(2);
    printf("process configuration OOM: %u/%u/%u triggers and 3 first no-hit completions, balanced\n",constructor,environment,options);
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    xvalue* ownedArgs=args("original","");
    XL6_ProcessCommand* command=xl6ProcessCommandDirectV1(view("program"),ownedArgs,-1,-1);assert(command);
    xrtValueRelease(ownedArgs);
    assert(!strcmp(command->config.Program,"program") && command->config.ArgCount==2);
    assert(!strcmp(command->config.Args[0],"original") && !strcmp(command->config.Args[1],""));
    xl6ProcessCommandEnvV1(command,view("ENV"),view("original"),-1,-1);
    for(size_t point=0;point<2;++point){
        assert(xrtMemDebugFailAfter(point));xl6ProcessCommandEnvV1(command,view("ENV"),view("replacement"),-1,-1);
        assert(xrtMemDebugFailTriggered());xrtMemDebugFailClear();
        assert(!strcmp(command->config.Env[0].Value,"original"));xrtClearError();
    }
    xl6ProcessCommandRemoveEnvV1(command,view("ENV"),-1);assert(!command->config.Env[0].Value);
    xthread* worker=xrtThreadCreate(wrongOwner,command,0);assert(worker && xrtThreadWait(worker)==XWAIT_OK);xrtThreadDestroy(worker);
    assert(command->config.HideWindow && command->config.InheritEnv && !command->config.Terminal);
    xrtSetErrorKind(XERR_IO);const xerror* ambient=xrtGetError();xl6ProcessCommandDestroyV1(command);
    assert(xrtGetError()==ambient);xrtClearError();balanced(&before);
    puts("process config copies, transactional update, 13 owner guards and ambient destruction, balanced");

    xrtMemDebugSnapshot(&before);
    void* runCommand=direct(argv[1],"echo",NULL);
    void* runOptions=xl6ProcessRunOptionsCreateV1(5000,1024,1024,0,20,0);assert(runOptions);
    const unsigned char raw[]={'a',0,0xe4,0xbd,0xa0,'b'};
    xvalue* input=xrtValueBytes((xbytesview){raw,sizeof(raw)});assert(input);
    assert(xrtMemDebugFailAfter(0));assert(!xl6ProcessRunV1(runCommand,runOptions,input,-1));
    assert(xrtMemDebugFailTriggered());xrtMemDebugFailClear();assert(xrtErrorKind(xrtGetError())==XERR_MEMORY);xrtClearError();
    void* result=xl6ProcessRunV1(runCommand,runOptions,input,-1);assert(result && xl6ProcessResultSuccessV1(result));
    assert(xl6ProcessResultInputWrittenV1(result)==sizeof(raw) && xl6ProcessResultExitCodeV1(result)==0);
    xvalue* output=xl6ProcessResultStdoutV1(result,-1);xbytesview actual;
    assert(output && xrtValueGetBytes(output,&actual) && actual.Size==sizeof(raw) && !memcmp(actual.Data,raw,sizeof(raw)));
    xstrview text=xl6ProcessResultStdoutTextV1(result,-1);assert(text.Data && text.Size==sizeof(raw));
    void* status=xl6ProcessResultStatusV1(result);assert(status && xl6ProcessStatusKindV1(status)==XPROCESS_EXIT_CODE);
    for(unsigned projection=0;projection<3;++projection){
        assert(xrtMemDebugFailAfter(0));
        if(projection==0)assert(!xl6ProcessResultStdoutV1(result,-1));
        else if(projection==1)assert(!xl6ProcessResultStdoutTextV1(result,-1).Data);
        else assert(!xl6ProcessResultStatusV1(result));
        assert(xrtMemDebugFailTriggered());xrtMemDebugFailClear();assert(xrtErrorKind(xrtGetError())==XERR_MEMORY);xrtClearError();
    }
    xl6ProcessResultDestroyV1(result);assert(xl6ProcessStatusCodeV1(status)==0 && !memcmp(actual.Data,raw,sizeof(raw)));
    xl6ProcessStatusDestroyV1(status);xrtValueRelease(output);xrtFree((void*)text.Data);
    xrtValueRelease(input);xl6ProcessCommandDestroyV1(runCommand);xl6ProcessRunOptionsDestroyV1(runOptions);
    balanced(&before);
    puts("real process binary echo, projection OOM retry and independent result/status bytes, balanced");

    xrtMemDebugSnapshot(&before);
    input=xrtValueBytes((xbytesview){NULL,0});assert(input);
    runCommand=direct(argv[1],"sleep","2000");
    runOptions=xl6ProcessRunOptionsCreateV1(-1,1024,1024,0,10,0);assert(runOptions);
    worker=xrtThreadCreate(cancelLater,runOptions,0);assert(worker);
    result=xl6ProcessRunV1(runCommand,runOptions,input,-1);
    assert(result && xl6ProcessResultCancelledV1(result) && !xl6ProcessResultSuccessV1(result));
    assert(xrtThreadWait(worker)==XWAIT_OK);xrtThreadDestroy(worker);
    xl6ProcessResultDestroyV1(result);xl6ProcessRunOptionsDestroyV1(runOptions);
    runOptions=xl6ProcessRunOptionsCreateV1(-1,1024,1024,0,10,xrtClock()+10000);assert(runOptions);
    result=xl6ProcessRunV1(runCommand,runOptions,input,-1);assert(result && xl6ProcessResultTimedOutV1(result));
    xl6ProcessResultDestroyV1(result);xl6ProcessRunOptionsDestroyV1(runOptions);xl6ProcessCommandDestroyV1(runCommand);
    xrtValueRelease(input);xrtClearError();balanced(&before);
    puts("real process concurrent cancellation and absolute deadline, balanced");
    runCommand=direct(argv[1],"echo",NULL);
    xl6ProcessCommandEnvV1(runCommand,view("XL6_PROCESS_NATIVE_OOM"),view("value"),-1,-1);
    xl6ProcessCommandWorkDirV1(runCommand,view("."),-1);
    runOptions=xl6ProcessRunOptionsCreateV1(5000,1024,1024,0,10,0);assert(runOptions);
    input=xrtValueBytes((xbytesview){raw,sizeof(raw)});assert(input);
    bool complete=false;size_t failed=0;
    for(size_t point=0;point<512;++point){
        xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(point));
        result=xl6ProcessRunV1(runCommand,runOptions,input,-1);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(hit && (result || xrtErrorKind(xrtGetError())!=XERR_MEMORY))
            fprintf(stderr,"native run point=%zu result=%p kind=%d message=%s\n",point,result,
                (int)xrtErrorKind(xrtGetError()),xrtGetError()?xrtErrorMessage(xrtGetError()):"none");
        if(hit){assert(!result && xrtErrorKind(xrtGetError())==XERR_MEMORY);++failed;}
        else assert(result && xl6ProcessResultSuccessV1(result) && !xrtGetError());
        xl6ProcessResultDestroyV1(result);xrtClearError();balanced(&before);
        if(!hit){complete=true;break;}
    }
    assert(complete);
    printf("native run complete prefix: %zu failures and 1 first no-hit, balanced\n",failed);
    xl6ProcessRunOptionsDestroyV1(runOptions);xl6ProcessCommandDestroyV1(runCommand);xrtValueRelease(input);
#if !defined(XL6_PROCESS_PRODUCT_OBSERVER)
    /* The private POSIX launch-plan unit belongs to the source-only test.
     * Product mode reaches it through the real exported Run implementation. */
    extern void testProcessPlanOom(void);
    testProcessPlanOom();
#endif
    return 0;
}
