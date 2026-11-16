#include "foreign_xge3_host_value.h"
#include <assert.h>
static xmemdebugsnapshot before;
xge_host_session_desc_t fixtureHostValueCopy(xge_host_session_desc_t value)
{assert(value.size==sizeof(value));++value.width;return value;}
int fixtureHostValueRead(const xge_host_session_desc_t* input,xge_host_session_desc_t* output,int status)
{assert(input && output && input->size==sizeof(*input));*output=*input;++output->height;return status;}
size_t fixtureHostValueSize(void){return sizeof(xge_host_session_desc_t);}
void fixtureHostValueHeapBegin(void)
{assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
bool fixtureHostValueHeapEqual(void)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    return before.Enabled && after.Enabled && after.AllocCount==before.AllocCount &&
        after.FreeCount==before.FreeCount && after.ReallocCount==before.ReallocCount &&
        after.LiveCount==before.LiveCount && after.LiveBytes==before.LiveBytes;
}
bool fixtureHostValueNativeHot(void)
{
    xge_host_session_desc_t input={sizeof(input),640,480,2.5f,0xf1234567u};
    fixtureHostValueHeapBegin();
    for(int i=0;i<1000;++i){xge_host_session_desc_t output=fixtureHostValueCopy(input);
        assert(output.width==641 && output.height==480 && output.dpiScale==2.5f && output.framebuffer==0xf1234567u);}
    return fixtureHostValueHeapEqual();
}
