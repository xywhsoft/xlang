#include <__xl_runtime/xruntime_decl.h>
#include <string.h>

char* example_owned_text(void)
{
    /* A raw libc malloc allocation is NOT compatible with xrtFree. A source
     * library can instead select XRT through its allocator hooks. */
    char* text=(char*)xrtMalloc(32);
    if(text)memcpy(text,"hello from C",13);
    return text; /* Ownership passes to xlang; do not free or mutate it. */
}
