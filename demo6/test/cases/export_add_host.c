#include "export_add.h"

int main(void)
{
    XL_InitOptions options = xlInitOptions(0, 0);
    int result;

    if ( !xlInit(&options) ) {
        return 1;
    }
    result = xl6_add_i64(20, 22) == 42 ? 0 : 2;
    xlUnit();
    return result;
}
