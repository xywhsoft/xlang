/* 预置合法的内存首因；正常映射/缓冲/释放均调用真实实现。 */
#define XRT_MODULE_BUFFER
#define XRT_MODULE_UNICODE
#define XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

enum { FAIL_CALLOC, FAIL_BUFFER_CREATE, FAIL_BUFFER_APPEND, FAIL_TEXT_ALLOC };
static struct { bool active; int stage; unsigned calls; xerror* cause; } fault;
static unsigned cases, checks, failures;
static void require(bool ok, const char* label)
{
    ++checks;
    if (!ok) { if (failures<12) fprintf(stderr,"sharedmem error contract: %s\n",label); ++failures; }
}
static bool fail_at(int stage)
{
    if (!fault.active || fault.stage!=stage) return false;
    ++fault.calls;
    xrtSetError(fault.cause);
    require(xrtMemDebugFailAfter(0),"guard error propagation after original publication");
    return true;
}
static void* allocate_handle(size_t count,size_t size)
{ return fail_at(FAIL_CALLOC) ? NULL : xrtCalloc(count,size); }
static xbuffer* create_buffer(void)
{ return fail_at(FAIL_BUFFER_CREATE) ? NULL : xrtBufferCreate(); }
static bool append_buffer(xbuffer* buffer,xbytesview data)
{ return fail_at(FAIL_BUFFER_APPEND) ? false : xrtBufferAppend(buffer,data); }
static void* allocate_text(size_t size)
{ return fail_at(FAIL_TEXT_ALLOC) ? NULL : xrtMalloc(size); }
#undef xrtCalloc
#undef xrtMalloc
#define xrtCalloc allocate_handle
#define xrtBufferCreate create_buffer
#define xrtBufferAppend append_buffer
#define xrtMalloc allocate_text
#include "../sharedmem_native.c"
#undef xrtCalloc
#undef xrtBufferCreate
#undef xrtBufferAppend
#undef xrtMalloc

static bool balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtClearError(); xrtMemDebugSnapshot(&after);
    return before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->OverflowCount==after.OverflowCount && before->UnderflowCount==after.UnderflowCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount;
}
static void one(const char* name, int operation, int code, bool longMessage)
{
    xmemdebugsnapshot before; xrtClearError(); xrtMemDebugSnapshot(&before);
    char message[301];
    if (longMessage) { memset(message,'m',300); message[300]=0; } else message[0]=0;
    xrtSetErrorInfo(XERR_MEMORY,"test.sharedmem.allocator",code,message);
    fault.cause=xrtTakeError(); fault.calls=0;
    require(fault.cause!=NULL,"own original allocator cause");
    void* owner=xshm_create(name,32);
    require(owner!=NULL && xshm_write(owner,0,"A\0B",3),"prepare real mapped source");
    /* create 用另一个名称，以免独占检查掩盖预期的句柄分配失败。 */
    char candidate[128]; snprintf(candidate,sizeof(candidate),"%s-new",name);
    fault.stage=operation<2 ? FAIL_CALLOC : operation==2 ? FAIL_BUFFER_CREATE : operation==3 ? FAIL_BUFFER_APPEND : FAIL_TEXT_ALLOC;
    fault.active=true;
    void* result=operation==0 ? xshm_create(candidate,32) : operation==1 ? xshm_open(name) :
        operation==4 ? xshm_read_text(owner,0,3) : xshm_read(owner,0,3);
    bool allocated=xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); fault.active=false;
    require(result==NULL && fault.calls==1,"chosen allocation failure observed once");
    require(!allocated,"no diagnostic reallocation after failure");
    require(xrtGetError()==fault.cause && xrtErrorKind(xrtGetError())==XERR_MEMORY &&
        xrtErrorCode(xrtGetError())==code && strcmp(xrtErrorDomain(xrtGetError()),"test.sharedmem.allocator")==0 &&
        strcmp(xrtErrorMessage(xrtGetError()),message)==0,"original identity/domain/code/message retained");
    xrtClearError();
    if (operation==0) {
        void* retry=xshm_create(candidate,32); require(retry!=NULL,"failed creation did not publish a name"); xshm_close(retry);
    } else if (operation==1) {
        void* retry=xshm_open(name); require(retry!=NULL,"same mapping opens after allocator recovery"); xshm_close(retry);
    }
    xbuffer* binary=xshm_read(owner,0,3);
    char* text=xshm_read_text(owner,0,3);
    require(binary && text && xrtBufferView(binary).Size==3 && memcmp(xrtBufferView(binary).Data,"A\0B",3)==0 &&
        memcmp(text,"A\0B",3)==0 && text[3]==0,"source unchanged and both read APIs recover");
    xrtBufferDestroy(binary); xrtFree(text);
    xrtSetError(fault.cause); xshm_close(owner);
    require(xrtGetError()==fault.cause,"mapping retirement preserves pending business error");
    xrtClearError(); xrtErrorFree(fault.cause); fault.cause=NULL;
    require(balanced(&before),"all owners and seven memory metrics balanced"); ++cases;
}
int main(void)
{
    if (!xrtMemDebugEnabled()) return 2;
    uint64_t nonce;
    if (!xrtSecureRandom(&nonce,sizeof(nonce))) return 2;
    char name[96]; snprintf(name,sizeof(name),"xl-shm-contract-%016llx",(unsigned long long)nonce);
    xrtSetErrorInfo(XERR_STATE,"test.sharedmem",0,"warm error owner"); xrtClearError();
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    static const int codes[]={INT_MIN,-17,INT_MAX};
    for (int operation=0; operation<5; ++operation) for (unsigned code=0; code<3; ++code)
        for (unsigned length=0; length<2; ++length) one(name,operation,codes[code],length!=0);
    require(cases==30,"all five allocation paths executed");
    require(balanced(&before),"entire test memory balanced");
    printf("sharedmem error contract: %u cases, %u checks, %u failures\n",cases,checks,failures);
    return failures ? 1 : 0;
}
