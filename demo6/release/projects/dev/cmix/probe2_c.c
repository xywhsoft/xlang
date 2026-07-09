/* probe2_c.c - companion for probe2.xl.
 *
 * NOTE: p2_const_val / p2_counter2 are OWNED by xlang (declared with
 * `export "..."` in probe2.xl), so this file must NOT define them -- doing so
 * triggers TCC "'sym' defined twice". To read them from C, declare them
 * `extern` (see probe_c.c for the read pattern).
 */
#include <stdlib.h>

/* native 32-bit int return: value > 2^31 boundary stays in 32-bit range.
 * Same value returned via three signatures to characterize the int/int32
 * mapping. */
int p2_big_id(int x)
{
    return 0x41414141 + x;
}

int p2_big_id32(int x)
{
    return 0x41414141 + x;
}

long long p2_wide(long long x)
{
    return 0x41414141LL + x;
}
