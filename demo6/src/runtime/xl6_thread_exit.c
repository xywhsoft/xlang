#define XRT_MODULE_ATOMIC
#include "lib/xrt/xrt.h"
#include "xl6_thread_exit.h"
#include <stddef.h>
/* Protect a typed function pointer, not an erased object-pointer cast. The
 * XRT atomic layer also supports TinyCC installations without stdatomic.h.
 * Only this bounded pointer publication/copy is inside the gate; callback
 * execution and every semantic cleanup stay outside it. */
static xatomic32 localExitGate;
static XL6_ThreadLocalExitV1 localExit;
static void lock(void)
{
    for(;;){uint32 expected=0;
        if(xrtAtomic32CompareExchange(&localExitGate,&expected,1,XMEMORY_ACQUIRE,XMEMORY_RELAXED))return;
    }
}
static void unlock(void){xrtAtomic32Store(&localExitGate,0,XMEMORY_RELEASE);}
bool xl6ThreadExitRegisterLocalV1(XL6_ThreadLocalExitV1 callback)
{
    if(!callback)return false;
    lock();bool ok=!localExit||localExit==callback;if(ok)localExit=callback;unlock();return ok;
}
bool xl6ThreadExitCurrentV1(void)
{
    lock();XL6_ThreadLocalExitV1 callback=localExit;unlock();
    return !callback || callback();
}
