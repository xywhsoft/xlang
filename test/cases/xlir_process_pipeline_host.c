#include "src/runtime/xl6_process_pipeline_internal.h"
#include "src/runtime/xl6_process_run_internal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static xstrview view(const char* s){return (xstrview){s,strlen(s)};}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(after.DoubleFreeCount==before->DoubleFreeCount && after.InvalidFreeCount==before->InvalidFreeCount);
    assert(after.UseAfterFreeCount==before->UseAfterFreeCount);
}
static void* command(const char* program,const char* arg)
{
    xvalue* args=xrtValueArray();assert(args);
    xvalue* item=xrtValueString(view(arg));assert(item && xrtValueArrayAppendNew(args,item));
    void* h=xl6ProcessCommandDirectV1(view(program),args,-1,-1);assert(h);xrtValueRelease(args);
    xl6ProcessCommandArg0V1(h,view("pipeline-alias"),-1);
    xl6ProcessCommandWorkDirV1(h,view("."),-1);
    xl6ProcessCommandEnvV1(h,view("XL6_PIPELINE_OWNED"),view("original"),-1,-1);
    xl6ProcessCommandEnvV1(h,view("XL6_PIPELINE_EMPTY"),view(""),-1,-1);
    xl6ProcessCommandRemoveEnvV1(h,view("XL6_PIPELINE_REMOVED"),-1);assert(!xrtGetError());return h;
}
static unsigned configPrefix(void* cmd,unsigned mode)
{
    for(size_t point=0;point<256;++point){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        XL6_ProcessPipeline* p=NULL;
        if(mode){p=xl6ProcessPipelineCreateV1();assert(p);}
        if(mode==2)for(unsigned i=0;i<8;++i)xl6ProcessPipelineAddV1(p,cmd);
        assert(!xrtGetError());size_t count=p ? p->count : 0;
        assert(xrtMemDebugFailAfter(point));
        if(!mode)p=xl6ProcessPipelineCreateV1();else xl6ProcessPipelineAddV1(p,cmd);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(hit){assert(xrtErrorKind(xrtGetError())==XERR_MEMORY);if(p)assert(p->count==count);}
        else{assert(p && !xrtGetError());if(mode)assert(p->count==count+1);}
        xl6ProcessPipelineDestroyV1(p);xrtClearError();balanced(&before);
        if(!hit)return (unsigned)point;
    }
    abort();
}
typedef struct OwnerProbe{void* p;void* cmd;void* options;const xvalue* input;} OwnerProbe;
static int32 wrongOwner(void* data)
{
    OwnerProbe* o=data;
#define DENY(Call) do{xrtClearError();Call;assert(xrtErrorKind(xrtGetError())==XERR_STATE);}while(0)
    DENY(xl6ProcessPipelineAddV1(o->p,o->cmd));DENY(xl6ProcessPipelineClearV1(o->p));
    DENY((void)xl6ProcessPipelineCountV1(o->p));DENY((void)xl6ProcessPipelineRunV1(o->p,o->options,o->input,-1));
    XL6_ProcessPipeline* local=xl6ProcessPipelineCreateV1();assert(local);
    DENY(xl6ProcessPipelineAddV1(local,o->cmd));assert(local->count==0);
    xl6ProcessPipelineDestroyV1(local);xrtClearError();return 0;
#undef DENY
}
static unsigned runPrefix(void* p,void* options,const xvalue* input,bool missing)
{
    for(size_t point=0;point<512;++point){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(point));
        void* r=xl6ProcessPipelineRunV1(p,options,input,-1);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(hit){assert(!r && xrtErrorKind(xrtGetError())==XERR_MEMORY);}
        else if(missing){assert(!r && xrtGetError() && xrtErrorKind(xrtGetError())!=XERR_MEMORY);}
        else{assert(r && !xrtGetError() && xl6PipelineResultSuccessV1(r));}
        xl6PipelineResultDestroyV1(r);xrtClearError();balanced(&before);
        if(!hit)return (unsigned)point;
    }
    abort();
}
static unsigned projectionPrefix(void* result,void* stage,unsigned which)
{
    for(size_t point=0;point<64;++point){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(point));
        void* resource=NULL;xvalue* bytes=NULL;xstrview text={0};
        if(which==0)resource=xl6PipelineResultStageV1(result,0);
        else if(which==1)bytes=xl6PipelineResultStdoutV1(result,-1);
        else if(which==2)text=xl6PipelineResultStdoutTextV1(result,-1);
        else if(which==3)resource=xl6ProcessStageStatusV1(stage);
        else if(which==4)bytes=xl6ProcessStageStderrV1(stage,-1);
        else text=xl6ProcessStageStderrTextV1(stage,-1);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(hit)assert(xrtErrorKind(xrtGetError())==XERR_MEMORY && !resource && !bytes && !text.Data);
        else assert(!xrtGetError() && (resource || bytes || text.Data));
        if(which==0)xl6ProcessStageDestroyV1(resource);else xl6ProcessStatusDestroyV1(resource);
        xrtValueRelease(bytes);xrtFree((void*)text.Data);xrtClearError();balanced(&before);
        if(!hit)return (unsigned)point;
    }
    abort();
}
static int32 cancelLater(void* options){xrtSleep(10);assert(xl6ProcessRunOptionsCancelV1(options));return 0;}
int main(int argc,char** argv)
{
    assert(argc==2 && xrtMemDebugEnable(true));xmemdebugsnapshot initial;xrtMemDebugSnapshot(&initial);
    XL6_ProcessCommand* cmd=command(argv[1],"echo");
    unsigned create=configPrefix(cmd,0),add=configPrefix(cmd,1),grow=configPrefix(cmd,2);
    XL6_ProcessPipeline* p=xl6ProcessPipelineCreateV1();assert(p);
    xl6ProcessPipelineAddV1(p,cmd);xl6ProcessPipelineAddV1(p,cmd);assert(p->count==2);
    assert(p->stages[0].Program!=cmd->config.Program && p->stages[0].Args!=cmd->config.Args);
    assert(p->stages[0].Env!=cmd->config.Env && !strcmp(p->stages[0].Env[0].Value,"original"));
    assert(p->stages[0].Env[1].Value && !*p->stages[0].Env[1].Value && !p->stages[0].Env[2].Value);
    xl6ProcessCommandEnvV1(cmd,view("XL6_PIPELINE_OWNED"),view("changed"),-1,-1);
    xl6ProcessCommandArg0V1(cmd,view("different"),-1);assert(!strcmp(p->stages[0].Arg0,"pipeline-alias"));
    void* options=xl6ProcessRunOptionsCreateV1(5000,1024,1024,0,0,0);assert(options);
    const unsigned char raw[]={'a',0,0xe4,0xbd,0xa0,'b'};
    xvalue* input=xrtValueBytes((xbytesview){raw,sizeof(raw)});assert(input);
    OwnerProbe probe={p,cmd,options,input};xthread* worker=xrtThreadCreate(wrongOwner,&probe,0);
    assert(worker && xrtThreadWait(worker)==XWAIT_OK);xrtThreadDestroy(worker);
    xl6ProcessCommandTerminalV1(cmd,true);xl6ProcessPipelineAddV1(p,cmd);
    assert(xrtErrorKind(xrtGetError())==XERR_UNSUPPORTED && p->count==2);xrtClearError();
    xl6ProcessCommandDestroyV1(cmd);cmd=NULL;
    unsigned run=runPrefix(p,options,input,false);
    void* result=xl6ProcessPipelineRunV1(p,options,input,-1);assert(result);
    assert(xl6PipelineResultCountV1(result)==2 && xl6PipelineResultCompletedV1(result)==2);
    assert(xl6PipelineResultInputWrittenV1(result)==sizeof(raw));
    void* stage=xl6PipelineResultStageV1(result,0);assert(stage);
    unsigned projections[6];for(unsigned i=0;i<6;++i)projections[i]=projectionPrefix(result,stage,i);
    void* status=xl6ProcessStageStatusV1(stage);assert(status);
    xvalue* output=xl6PipelineResultStdoutV1(result,-1);xbytesview actual;
    assert(output && xrtValueGetBytes(output,&actual) && actual.Size==sizeof(raw) && !memcmp(actual.Data,raw,sizeof(raw)));
    xl6PipelineResultDestroyV1(result);
    xvalue* errorOutput=xl6ProcessStageStderrV1(stage,-1);xbytesview errorBytes;
    assert(errorOutput && xrtValueGetBytes(errorOutput,&errorBytes) && errorBytes.Size==3 && !memcmp(errorBytes.Data,"E\0R",3));
    xl6ProcessStageDestroyV1(stage);assert(xl6ProcessStatusCodeV1(status)==0);
    xl6ProcessStatusDestroyV1(status);xrtValueRelease(output);xrtValueRelease(errorOutput);
    void* missingCommand=command("__XL6_PIPELINE_MISSING_734c9__/child","echo");
    xl6ProcessPipelineAddV1(p,missingCommand);xl6ProcessCommandDestroyV1(missingCommand);
    unsigned missing=runPrefix(p,options,input,true);xl6ProcessPipelineClearV1(p);
    cmd=command(argv[1],"sleep");xl6ProcessPipelineAddV1(p,cmd);xl6ProcessCommandDestroyV1(cmd);
    xl6ProcessRunOptionsDestroyV1(options);options=xl6ProcessRunOptionsCreateV1(-1,1024,1024,0,0,0);assert(options);
    worker=xrtThreadCreate(cancelLater,options,0);assert(worker);
    result=xl6ProcessPipelineRunV1(p,options,input,-1);assert(result && xl6PipelineResultCancelledV1(result));
    assert(xrtThreadWait(worker)==XWAIT_OK);xrtThreadDestroy(worker);xl6PipelineResultDestroyV1(result);
    xl6ProcessRunOptionsDestroyV1(options);
    options=xl6ProcessRunOptionsCreateV1(-1,1024,1024,0,0,xrtClock()+10000);assert(options);
    result=xl6ProcessPipelineRunV1(p,options,input,-1);assert(result && xl6PipelineResultTimedOutV1(result));
    xl6PipelineResultDestroyV1(result);xl6ProcessRunOptionsDestroyV1(options);xrtValueRelease(input);
    xrtSetErrorKind(XERR_IO);const xerror* ambient=xrtGetError();xl6ProcessPipelineDestroyV1(p);
    assert(xrtGetError()==ambient);xrtClearError();balanced(&initial);
    printf("pipeline resident prefixes: %u/%u/%u/%u/%u failures + 5 first no-hit; snapshots, 5 owner guards, real collectors balanced\n",create,add,grow,run,missing);
    printf("pipeline projections: %u/%u/%u/%u/%u/%u failures + 6 first no-hit; independent binary/status, cancellation/deadline, balanced\n",projections[0],projections[1],projections[2],projections[3],projections[4],projections[5]);
    return 0;
}
