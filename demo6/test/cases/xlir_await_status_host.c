/* Generated C is preincluded in this TU only. */
#include <assert.h>
#include <stdio.h>
extern void await_status_begin(unsigned);
extern void await_status_end(void);
int main(void)
{
    unsigned round, mode;
    for (round = 0; round < 100; ++round) for (mode = 0; mode < 3; ++mode) {
        await_status_begin(mode);
        assert(xlir_module_init());
        xlir_module_fini();
        assert(xlir_module_initialized == 0 && xrtGetError() == NULL);
        await_status_end();
    }
    puts("await status: 300 module transactions, blocking + generator, ready/missing-payload/diagnostic-free-copy, finally and balanced memory");
    return 0;
}
