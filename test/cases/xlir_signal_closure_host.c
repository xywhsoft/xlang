/* Exact generated/native/resident closure, with no instrumentation-only
 * MEMORY_DEBUG root added to the production module plan. */
#include <stdio.h>
int main(void)
{
    int initialized=xlir_module_init();
    if(!initialized && xrtGetError())fprintf(stderr,"signal closure error: %s\n",xrtErrorMessage(xrtGetError()));
    xlir_module_fini();xrtClearError();
#if defined(XL6_SIGNAL_CLOSURE)
    xrtSignalShutdown();
#endif
    return initialized?0:1;
}
