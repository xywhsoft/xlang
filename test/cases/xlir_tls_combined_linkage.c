#if defined(XL6_OBSERVER_CORE)
    #include "include/xrt_decl.h"
#else
    #include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#endif
#include <assert.h>
/* A real relocation with the exact public type; never called by this probe. */
typedef xfuture* (*OwnedProxyFuture)(xnetengine*,xnetresolver*,const xnetproxy*,
    cstr,uint16,const xtlsclientconfig*,const xtlsdialconfig*,ptr,const xtlsstreamownershipv1*);
int main(void)
{
    OwnedProxyFuture volatile target=xrtTlsDialProxyAsyncOwnedV1;
    assert(target!=NULL);
    return 0;
}
