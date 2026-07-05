#ifdef XL6_EXAMPLE_LINK_EXPORTS
#include "11_export_to_c.h"

int main(void)
{
    XL_InitOptions options = xlInitOptions(0, 0);
    long long first = 0;
    long long second = 0;

    if ( !xlInit(&options) ) {
        return 1;
    }

    if ( tutorial_add(20, 22) != 42 ) {
        xlUnit();
        return 2;
    }
    if ( tutorial_self_check() != 0 ) {
        xlUnit();
        return 7;
    }
    if ( tutorial_counter != 40 || tutorial_answer != 42 ) {
        xlUnit();
        return 3;
    }

    tutorial_counter += 2;
    if ( tutorial_counter != 42 ) {
        xlUnit();
        return 4;
    }

    tutorial_pair(41, &first, &second);
    if ( first != 41 || second != 42 ) {
        xlUnit();
        return 5;
    }

    xlUnit();
    return 0;
}
#else
#include <stdio.h>

int main(void)
{
    printf("tutorial host_export standalone ok\n");
    return 0;
}
#endif
