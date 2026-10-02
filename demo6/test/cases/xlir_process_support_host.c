/* The XRT implementation and terminal probe are real. Only association launch
 * is replaced in the adapter translation unit to avoid opening GUI programs.
 * This verifies input/ownership/error transport, not OS launch success. */
#define XRT_MODULE_PROCESS_OPEN
#define XRT_MODULE_PROCESS_TERMINAL
#define XRT_MODULE_UNICODE_TEXT
#define XRT_MODULE_STRING
#define XRT_MODULE_VALUE
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned launches;
static bool accepted=true;
static char lastTarget[128];
static bool controlledOpen(cstr text)
{
    assert(strlen(text)<sizeof(lastTarget));
    strcpy(lastTarget,text);++launches;
    if(!accepted)xrtSetErrorKind(XERR_IO);
    return accepted;
}
#define xrtProcessOpen controlledOpen
#include "src/runtime/xl6_process_open.c"
#undef xrtProcessOpen
#include "src/runtime/xl6_process_terminal.c"

static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount);
    assert(before->DoubleFreeCount==after.DoubleFreeCount);
    assert(before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static void rejected(xstrview text,int64_t size,xerrkind kind)
{
    unsigned count=launches;xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    assert(!xl6ProcessOpenV1(text,size));
    assert(launches==count && xrtErrorKind(xrtGetError())==kind);
    xrtClearError();balanced(&before);
}
static void errorBuildPrefix(void)
{
    unsigned failures=0,completions=0;
    for(unsigned cause=0;cause<3;++cause) {
        bool complete=false;
        for(size_t point=0;point<32;++point) {
            xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
            if(cause==1)xrtSetErrorKind(XERR_IO);
            if(cause==2)xrtSetErrorInfo(XERR_IO,"controlled.launch",7,"native launch cause");
            const xerror* original=xrtGetError();
            assert(cause ? original!=NULL : original==NULL);
            assert(xrtMemDebugFailAfter(point));
            /* Real XRT error builder, not the controlled association launcher. */
            __xrtProcessOpenError("controlled launch failed");
            bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
            const xerror* error=xrtGetError();
            if(hit){assert(xrtErrorKind(error)==XERR_MEMORY);++failures;}
            else {
                assert(xrtErrorKind(error)==XERR_IO && xrtErrorCode(error)==XPROCESS_ERROR_OPEN);
                assert(!strcmp(xrtErrorDomain(error),"xrt.process") && !strcmp(xrtErrorOperation(error),"open"));
                assert(xrtErrorCause(error)==original);++completions;
            }
            xrtClearError();balanced(&before);
            if(!hit){complete=true;break;}
        }
        assert(complete);
    }
    printf("process open error prefix: %u failures and %u first no-hit, causes preserved, balanced\n",failures,completions);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    errorBuildPrefix();
    rejected((xstrview){"",0},-1,XERR_VALUE);
    rejected((xstrview){"a\0b",3},-1,XERR_VALUE);
    rejected((xstrview){"abc",3},-2,XERR_RANGE);
    rejected((xstrview){"abc",3},0,XERR_VALUE);
    rejected((xstrview){"\xe4\xbd\xa0",3},1,XERR_VALUE);
    rejected((xstrview){NULL,1},-1,XERR_ARGUMENT);
    rejected((xstrview){"\xff",1},-1,XERR_VALUE);
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    assert(xl6ProcessOpenV1((xstrview){"a\0b",3},1));
    assert(!strcmp(lastTarget,"a"));
    assert(xl6ProcessOpenV1((xstrview){"\xe4\xbd\xa0",3},20));
    assert(!strcmp(lastTarget,"\xe4\xbd\xa0"));
    accepted=false;
    assert(!xl6ProcessOpenV1((xstrview){"example:target",14},-1));
    assert(xrtErrorKind(xrtGetError())==XERR_IO);
    xrtClearError();balanced(&before);accepted=true;
    unsigned failures=0,completions=0;
    for(size_t point=0;point<32;++point) {
        unsigned count=launches;xrtMemDebugSnapshot(&before);
        assert(xrtMemDebugFailAfter(point));
        bool result=xl6ProcessOpenV1((xstrview){"example:target",14},-1);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(hit){assert(!result && launches==count && xrtErrorKind(xrtGetError())==XERR_MEMORY);++failures;}
        else {assert(result && launches==count+1 && !xrtGetError());++completions;}
        xrtClearError();balanced(&before);if(!hit)break;
    }
    assert(failures>0 && completions==1);
    xrtMemDebugSnapshot(&before);
    bool supported=xl6ProcessTerminalSupportedV1();
    assert(supported==xrtProcessTerminalSupported() && !xrtGetError());
    balanced(&before);
    printf("process support: 7 rejected inputs, exact prefix/clamp, native error transport; %u allocation failures and %u first no-hit, balanced; real terminal probe=%d\n",failures,completions,supported);
    return 0;
}
