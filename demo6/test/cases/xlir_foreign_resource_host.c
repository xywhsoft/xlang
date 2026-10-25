#include "src/runtime/xl6_foreign_resource.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

static size_t drops;
static bool dispose(void* context,const xrtcallframe* frame,xrtcallresult* result)
{
    (void)context;(void)result;void* raw=NULL;
    assert(frame->ArgumentCount==1 && xrtValueGetPointer(frame->Arguments[0],&raw));
    assert(raw && *(int*)raw==42);free(raw);++drops;return true;
}
static void* allocate(void) { int* raw=malloc(sizeof(*raw));assert(raw);*raw=42;return raw; }
int main(void)
{
    assert(xrtMemDebugEnable(true));
    xrtcallable* callable=xrtCallableCreate(NULL,dispose,NULL,NULL);assert(callable);
    xvalue* releaser=xrtValueCallableTake(&callable);assert(releaser && !callable);
    void* raw=allocate();xvalue* owner=xl6ForeignResourceCreateV1(raw,releaser);assert(owner);
    assert(xl6ForeignResourcePointerV1(owner)==raw);
    xvalue* alias=xrtValueRetain(owner);assert(alias);
    xrtValueRelease(owner);assert(drops==0);
    xrtValueRelease(alias);assert(drops==1 && !xrtGetError());
    xmemdebugsnapshot before={0},after={0};xrtMemDebugSnapshot(&before);
    size_t failed=0,passed=0;
    for(int64_t fault=0;fault<32;++fault){
        raw=allocate();size_t previous=drops;
        assert(xrtMemDebugFailAfter(fault));
        owner=xl6ForeignResourceCreateV1(raw,releaser);
        xrtMemDebugFailClear();xrtClearError();
        if(owner){assert(xl6ForeignResourcePointerV1(owner)==raw);xrtValueRelease(owner);assert(drops==previous+1);++passed;}
        else { assert(drops==previous);free(raw);++failed; }
        xrtClearError();xrtMemDebugSnapshot(&after);assert(after.LiveCount==before.LiveCount);
    }
    assert(failed && passed);
    raw=allocate();assert(!xl6ForeignResourceCreateV1(raw,xrtValueNull()));free(raw);xrtClearError();
    xrtValueRelease(releaser);
    printf("foreign resource faults: %zu refused, %zu completed\n",failed,passed);
    return 0;
}
