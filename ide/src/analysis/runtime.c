/* The standalone analysis process owns exactly one compiler XRT implementation.
 * This file must never be linked into the XGE-owned IDE process. */
#define XRT_IMPLEMENTATION
#include <xrt.h>
#include "analysis/pool_idle.h"

int xlsRuntimeTaskPoolIdle(xtaskpool* pool)
{
    if(!pool || !__xrtTaskPoolEnter(pool,false))return 0;
    int idle=0;
    if(xrtMutexLock(&pool->Lock)) {
        idle=__xrtTaskPoolIdleLocked(pool);
        xrtMutexUnlock(&pool->Lock);
    }
    __xrtTaskPoolLeave(pool);return idle;
}
