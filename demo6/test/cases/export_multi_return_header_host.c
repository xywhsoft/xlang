#include "export_multi_return.h"

int main(void)
{
    int64_t first = 0;
    int64_t second = 0;

    xl6_pair_i64(41, &first, &second);
    if ( first != 41 || second != 42 ) {
        return 1;
    }
    xl6_pair_i64(7, 0, &second);
    return second == 8 ? 0 : 1;
}
