/* Preinclude the actual emitted program. The facade owns the supplied source;
 * every construction prefix must retire watches and leave no default source. */
#include <assert.h>
#include <stdio.h>
#include "test/support/module_product_memory.h"
extern bool xrtSignalShutdown(void);

static bool point(size_t budget)
{
    xl6ProductMemoryBegin(budget);
    int initialized=xlir_module_init();bool hit=xl6ProductMemoryEnd();
    xerror* error=xrtTakeError();
    if(hit?initialized||xrtErrorKind(error)!=XERR_MEMORY:!initialized||error)
        fprintf(stderr,"signal initialization point=%zu hit=%d initialized=%d kind=%d\n",budget,hit,initialized,(int)xrtErrorKind(error));
    assert(hit?!initialized&&xrtErrorKind(error)==XERR_MEMORY:initialized&&!error);
    xlir_module_fini();assert(!xlir_module_initialized&&!xrtGetError());
    xlir_module_fini();assert(!xrtGetError());
    assert(xrtSignalShutdown()&&!xrtGetError());
    xrtErrorFree(error);xl6ProductMemoryVerify();return hit;
}
int main(void)
{
    size_t budget=0;while(budget<10000&&point(budget))++budget;assert(budget<10000);
    printf("signal initialization prefixes=%zu first-no-hit=%zu balanced\n",budget+1,budget);
    return 0;
}
