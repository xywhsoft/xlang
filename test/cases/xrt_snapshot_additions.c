/* Link/dispatch proof for the additive producer revision. No sockets, DNS,
 * certificates or TLS handshakes: only local scalar/error API calls. */
#define XRT_MODULE_ALL
#include <xrt_decl.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned notices;
static void notified(const xerror* error,ptr context)
{(void)error;(void)context;++notices;}
int main(void)
{
    xrtSetErrorHandler(notified,NULL);
    xrtSetErrorInfo(XERR_STATE,"snapshot",1,"owned current error");
    const xerror* current=xrtGetError();assert(current && notices==1);
    xerror* previous=NULL;
    assert(xrtTrySwapErrorTake(NULL,&previous) && previous==current && !xrtGetError());
    assert(notices==1);
    xerror* empty=(xerror*)(uintptr_t)1;
    assert(xrtTrySwapErrorTake(previous,&empty) && !empty && xrtGetError()==current);
    xnetaddr address;memset(&address,0xa5,sizeof(address));
    assert(xrtNetAddrTryParse(&address,(xstrview){"::1",3},123));
    assert(address.Family==XNET_FAMILY_IPV6 && address.Port==123 && address.Address[15]==1);
    xnetaddr saved=address;
    assert(!xrtNetAddrTryParse(&address,(xstrview){"word",4},123));
    assert(!memcmp(&address,&saved,sizeof(address)) && xrtGetError()==current && notices==1);
    assert(xrtTrySwapErrorTake(NULL,&previous) && previous==current);
    xrtErrorFree(previous);
    // Presence and checked invalid-argument behavior; no protocol operation.
    assert(xrtTlsClientResumeCapacity(NULL)==0 && xrtGetError());
    xrtSetErrorHandler(NULL,NULL);xrtClearError();
    puts("XRT_SNAPSHOT_ADDITIONS_OK");
    return 0;
}
