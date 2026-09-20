/* The differential gate overwrites this generated input once per backend. */
#include "../../build/test/xlir_cgen_differential_generated.c"

int main(void)
{
#ifdef XLIR_DIFFERENTIAL_LEGACY
    if ( !xl6_temp_managed_runtime_init() ) return 1;
#endif
    return xl6_diff_converted() == 12 ? 0 : 1;
}
