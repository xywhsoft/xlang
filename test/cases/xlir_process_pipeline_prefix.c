/* Actual synchronous Pipeline, not a mock. The setup-failure scan must neither
 * close an unrelated parent descriptor nor leave detached waiter allocations.
 * Current-thread logical allocation prefixes; worker allocations are not
 * injected here. Existing pipeline capture-OOM tests cover their allocator. */
#if defined(XL6_PIPELINE_PREFIX_FROM_XLANG)
#define XRT_MODULE_PROCESS_PIPELINE
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#ifndef XRT_MODULE_WAIT
#define XRT_MODULE_WAIT
#endif
#include "lib/xrt/xrt.h"
#include "release/include/xl_clock.h"
#else
#include "../test.h"
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(_WIN32) || defined(_WIN64)
#include <fcntl.h>
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

static int pipelinePrefixChild(void)
{
#if defined(_WIN32) || defined(_WIN64)
    (void)_setmode(_fileno(stdin),_O_BINARY);
    (void)_setmode(_fileno(stdout),_O_BINARY);
    (void)_setmode(_fileno(stderr),_O_BINARY);
#endif
    unsigned char data[4096]; size_t size;
    while((size=fread(data,1,sizeof(data),stdin))!=0)
        if(fwrite(data,1,size,stdout)!=size) return 91;
    if(ferror(stdin)) return 92;
    return fwrite("E\0R",1,3,stderr)==3 ? 0 : 93;
}
static void prefixBalanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    if(after.LiveCount!=before->LiveCount || after.LiveBytes!=before->LiveBytes)
        fprintf(stderr,"pipeline live ledger before=%llu/%llu after=%llu/%llu\n",
            (unsigned long long)before->LiveCount,(unsigned long long)before->LiveBytes,
            (unsigned long long)after.LiveCount,(unsigned long long)after.LiveBytes);
    assert(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount);
    assert(after.UseAfterFreeCount==before->UseAfterFreeCount);
}
static unsigned pipelinePrefix(const char* program,bool badLast)
{
    const cstr args[]={"--pipeline-prefix-child"};
    xprocessconfig stages[2]; xprocesspipelineoptions options;
    const unsigned char input[]={'a',0,0xe4,0xbd,0xa0,'b'};
    for(size_t i=0;i<2;++i) {
        assert(xrtProcessConfigInit(&stages[i]));
        stages[i].Program=program; stages[i].Args=args; stages[i].ArgCount=1;
        stages[i].HideWindow=true;
    }
    if(badLast) stages[1].Program="__XRT_PIPELINE_MISSING_4c719b__/child";
    assert(xrtProcessPipelineOptionsInit(&options));
    options.Input=(xbytesview){input,sizeof(input)};
    options.StopGrace=0; options.StdoutLimit=1024; options.StderrLimit=1024;
    for(size_t point=0;point<512;++point) {
        xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
        xprocesspipelineresult result;
        options.Deadline=xl6DeadlineAfterMillisecondsV1(5000);
        assert(xrtMemDebugFailAfter(point));
        bool ok=xrtProcessPipeline(stages,2,&options,&result);
        bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        const xerror* error=xrtGetError();
#if !defined(_WIN32) && !defined(_WIN64)
        /* fd 0 belongs to a controlled /dev/null descriptor in this independent
         * test process. Child stdio is always a private Pipeline pipe. */
        if(fcntl(0,F_GETFD)==-1) fprintf(stderr,"pipeline closed unrelated fd 0: badLast=%d point=%zu\n",badLast,point);
        assert(fcntl(0,F_GETFD)!=-1);
#endif
        if(hit) {
            if(ok || xrtErrorKind(error)!=XERR_MEMORY)
                fprintf(stderr,"pipeline OOM category badLast=%d point=%zu ok=%d kind=%d\n",badLast,point,ok,(int)xrtErrorKind(error));
            assert(!ok && xrtErrorKind(error)==XERR_MEMORY);
        } else if(badLast) {
            assert(!ok && error && xrtErrorKind(error)!=XERR_MEMORY);
            assert(result.Wait==XWAIT_ERROR && !result.Stages && result.StageCount==0 && !result.Stdout);
        } else {
            assert(ok && !error && xrtProcessPipelineSuccess(&result));
            assert(result.StageCount==2 && result.InputWritten==sizeof(input));
            assert(result.StdoutSize==sizeof(input) && !memcmp(result.Stdout,input,sizeof(input)));
            for(size_t i=0;i<2;++i) {
                assert(result.Stages[i].Status.Kind==XPROCESS_EXIT_CODE && result.Stages[i].Status.Code==0);
                assert(result.Stages[i].StderrSize==3 && !memcmp(result.Stages[i].Stderr,"E\0R",3));
            }
        }
        xrtProcessPipelineResultUnit(&result); xrtClearError(); prefixBalanced(&before);
        if(!hit) return (unsigned)point;
    }
    abort();
}
int main(int argc,char** argv)
{
    if(argc==2 && !strcmp(argv[1],"--pipeline-prefix-child")) return pipelinePrefixChild();
    assert(argc==1);
#if !defined(_WIN32) && !defined(_WIN64)
    int controlled=open("/dev/null",O_RDONLY); assert(controlled>=0);
    if(controlled!=0) {assert(dup2(controlled,0)==0); assert(close(controlled)==0);}
#endif
    assert(xrtMemDebugEnable(true));
    unsigned normal=pipelinePrefix(argv[0],false),bad=pipelinePrefix(argv[0],true);
    printf("pipeline native prefixes: %u/%u failures + 2 first no-hit, unrelated stdin intact, exact binary per-stage stderr, private waiters retired, balanced\n",normal,bad);
    return 0;
}
