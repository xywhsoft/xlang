/* Preinclude the real generated program. No hand-written substitute for the
 * language wrapper, result boxing, versioned commit or exported call boundary. */
#include <assert.h>
#include <stdio.h>
#include "test/support/module_product_memory.h"
#if defined(XL6_FRAME_EAGER_REGRESSION)
/* Deliberately reproduce eager consumption before language boxing. Only this
 * host TU's generated call is redirected; all production objects are intact.
 * The mandatory expected-failure check must identify the lost input at OOM. */
#undef xl6NetLineFramerNextV1
extern void* xl6NetLineFramerNextV1(void*);
void* xl_frame_eager_line(void* raw)
{
    void* result=xl6NetLineFramerNextV1(raw);
    if(result&&xl6NetFrameResultStatusV1(result)==1)xl6NetLineFramerCommitV1(raw,result);
    return result;
}
#endif
static bool point(int kind,size_t fail)
{
    xl6ProductMemoryBegin(SIZE_MAX);assert(xlir_module_init());
    assert(xl_frame_prepare(kind)==1&&!xrtGetError());
    assert(xrtMemDebugFailAfter(fail));int64_t result=xl_frame_take(kind);
    bool hit=xl6ProductMemoryEnd();xerror* error=xrtTakeError();
    int64_t remaining=xl_frame_remaining(kind);assert(!xrtGetError());
    if(hit?result!=0||xrtErrorKind(error)!=XERR_MEMORY||remaining!=(kind?7:4):result!=3||error||remaining!=0)
        fprintf(stderr,"frame boxing point=%zu kind=%d hit=%d result=%lld error=%d remaining=%lld\n",fail,kind,hit,(long long)result,(int)xrtErrorKind(error),(long long)remaining);
    assert(hit?result==0&&xrtErrorKind(error)==XERR_MEMORY&&remaining==(kind?7:4):result==3&&!error&&!remaining);
    xlir_module_fini();assert(!xlir_module_initialized&&!xrtGetError());xrtErrorFree(error);xl6ProductMemoryVerify();return hit;
}
int main(void)
{
    for(int kind=0;kind<2;++kind){size_t n=0;while(n<1000&&point(kind,n))++n;assert(n<1000);
        printf("generated frame boxing kind=%d prefixes=%zu first-no-hit=%zu input-preserved balanced\n",kind,n+1,n);}
    return 0;
}
