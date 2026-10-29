#include "src/runtime/xl6_array_shuffle_internal.h"

static uint64_t below(void* context, uint64_t bound)
{ (void)context; return xrtRandBelow(bound); }

bool xl6ArrayRandShuffleV1(const xvalue* source)
{
    XL6_ArrayShuffleTargetV1 target={0};
    if (!xl6ArrayShuffleBeginV1(source,&target)) return false;
    /* Initialize once before the first wire mutation. Native XRT Shuffle
     * performs the same validation before touching its contiguous slots. */
    bool ok=target.owner ? xrtRandShuffle(target.data,target.count,target.size) :
        xrtRandBytes(NULL,0) && xl6ArrayShuffleWireV1(&target,below,NULL);
    return xl6ArrayShuffleEndV1(&target,ok);
}
