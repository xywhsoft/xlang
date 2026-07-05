#ifdef XL6_EXAMPLE_LINK_EXPORTS
#include "09_c_export_abi.h"

int main(void)
{
    int64_t first = 0;
    int64_t second = 0;

    if ( xl6_feature_add_i64(20, 22) != 42 ) {
        return 1;
    }

    xl6_feature_pair_i64(41, &first, &second);
    if ( first != 41 || second != 42 ) {
        return 2;
    }

    if ( xl6_feature_counter != 1 ) {
        return 3;
    }

    if ( xl6_feature_version != 6 ) {
        return 4;
    }

    xl6_feature_counter = 36;
    return xl6_feature_get_total() == 42 ? 0 : 5;
}
#else
#include <stdio.h>

int main(void)
{
    printf("feature_suite host_exports standalone ok\n");
    return 0;
}
#endif
