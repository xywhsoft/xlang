#include "src/runtime/xl6_array_shuffle_internal.h"

static uint64_t below(void* context, uint64_t bound)
{ return xrtRngBelow64(context,bound); }

bool xl6ArrayRngShuffleV1(xrng* rng, const xvalue* source)
{
    if (!rng || !xrtRngReady(rng)) { xrtSetErrorKind(XERR_STATE); return false; }
    XL6_ArrayShuffleTargetV1 target={0};
    if (!xl6ArrayShuffleBeginV1(source,&target)) return false;
    bool ok=target.owner ? xrtRngShuffle(rng,target.data,target.count,target.size) :
        xl6ArrayShuffleWireV1(&target,below,rng);
    return xl6ArrayShuffleEndV1(&target,ok);
}
