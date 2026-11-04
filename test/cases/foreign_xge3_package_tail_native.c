/* An actual native DLL publishes a language callback, drops its final owner,
 * then continues executing its own native machine code until the host joins. */
#define TAIL_BUILD_DLL
#include "foreign_xge3_package_tail.h"
static TailHandler callback;
static void* user;
static TailOwner registration;
int tail_install(TailHandler next,void* data,const TailOwner* owner)
{
    TailOwner incoming=owner ? *owner:(TailOwner){0};
    if(next && (!incoming.reference.Data || !incoming.reference.Ops || !incoming.Hold || !incoming.Drop))return -1;
    if(!next && (incoming.reference.Data || incoming.reference.Ops || incoming.Hold || incoming.Drop))return -1;
    if(next && !incoming.Hold(incoming.reference.Data))return -1;
    TailOwner old=registration;callback=next;user=next ? data:NULL;registration=incoming;
    return !old.Drop || old.Drop(old.reference.Data) ? 0:-1;
}
int tail_dispatch(TailControl* control)
{
    TailHandler selected=callback;void* data=user;TailOwner snapshot=registration;
    if(!control || !selected || !snapshot.Hold || !snapshot.Hold(snapshot.reference.Data))return -1;
    const TailEvent event={23};
    selected(&event,data); /* Clears registration and requests module close. */
    if(!snapshot.Drop(snapshot.reference.Data))return -1;
    /* No callback/owner reference remains. This is real publisher DLL code,
     * deliberately kept on the stack after the resident Drop has returned. */
    __atomic_store_n(&control->phase,1,__ATOMIC_RELEASE);
    while(!__atomic_load_n(&control->release,__ATOMIC_ACQUIRE)){
#if defined(__x86_64__) || defined(__i386__)
        __asm__ __volatile__("pause");
#endif
    }
    __atomic_store_n(&control->phase,2,__ATOMIC_RELEASE);
    return 0;
}
