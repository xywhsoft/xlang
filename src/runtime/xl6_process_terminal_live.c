#include "src/runtime/xl6_process_live.h"

/* The caller owns a live Process reference throughout this entry. XRT
 * serializes resize with native close and reports non-terminal/closed state;
 * no opaque native field inspection or secondary terminal state is needed. */
bool xl6ProcessResizeV1(void* handle,int64_t columns,int64_t rows)
{
    XL6_ProcessLive* process=xl6ProcessLiveCheck(handle);if(!process)return false;
    if(columns<=0 || rows<=0 || columns>32767 || rows>32767){xrtSetErrorKind(XERR_RANGE);return false;}
    return xrtProcessResize(process->native,(uint32)columns,(uint32)rows);
}
