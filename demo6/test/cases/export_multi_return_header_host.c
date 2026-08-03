#include "export_multi_return.h"

int main(void)
{
    XL_InitOptions options = xlInitOptions(0, 0);
    int64_t first = 0;
    int64_t second = 0;
    int result;

    if ( !xlInit(&options) ) {
        return 1;
    }
    xl6_pair_i64(41, &first, &second);
    if ( first != 41 || second != 42 ) {
        xlUnit();
        return 2;
    }
    xl6_pair_i64(7, 0, &second);
    result = second == 8 ? 0 : 3;
    xlUnit();
    return result;
}
