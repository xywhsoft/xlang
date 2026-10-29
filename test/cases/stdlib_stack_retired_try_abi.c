/* A stale generated caller's old nine-argument declaration must compile, but
 * cannot link to the current runtime. Never run it against the new ABI. */
#include "src/runtime/xl6_stack.h"
extern bool xl6StackTryTakeV1(const xvalue*, uint64_t, uint64_t, unsigned int,
    size_t, const xrttype*, int64_t, void*, bool);
int main(void)
{
    int64_t output=0;
    return xl6StackTryTakeV1(NULL,1,2,1,sizeof(output),xrtTypeInt64(),0,&output,false) ? 0 : 1;
}
