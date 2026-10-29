#ifdef XL6_EXAMPLE_LINK_EXPORTS
#include "09_c_export_abi.h"

int main(void)
{
    XL_InitOptions options = xlInitOptions(0, NULL);
    int64_t first = 0;
    int64_t second = 0;
    int result = 0;

    if ( !xlInit(&options) ) {
        return 10;
    }

    if ( xl6_feature_add_i64(20, 22) != 42 ) {
        result = 1;
        goto cleanup;
    }

    xl6_feature_pair_i64(41, &first, &second);
    if ( first != 41 || second != 42 ) {
        result = 2;
        goto cleanup;
    }

    if ( xl6_feature_counter != 1 ) {
        result = 3;
        goto cleanup;
    }

    if ( xl6_feature_version != 6 ) {
        result = 4;
        goto cleanup;
    }

    xl6_feature_counter = 36;
    if ( xl6_feature_get_total() != 42 ) {
        result = 5;
    }

cleanup:
    xlUnit();
    return result;
}
#else
#include <stdio.h>

int main(void)
{
    printf("feature_suite host_exports standalone ok\n");
    return 0;
}
#endif
