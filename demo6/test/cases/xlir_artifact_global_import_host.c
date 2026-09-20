/* The gate force-includes the freshly emitted header with XL6_USE_SHARED.
 * Link directly against the produced DLL to check data/function imports. */
#include <stdio.h>
int main(void)
{
    if (xlir_artifact_counter != 41 || xlir_artifact_offset != 1) return 1;
    if (xlir_artifact_check() != 42 || xlir_artifact_counter != 42) return 2;
    if (!xl_script_runtime_retire_checked()) return 3;
    puts("artifact imported globals and function complete");
    return 0;
}
