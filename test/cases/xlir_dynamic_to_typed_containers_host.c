#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"

int xlang_main(xvalue* arg);

int main(void)
{
    xvalue* arguments = xrtValueArray();
    if (arguments == NULL) return 1;
    int result = xlang_main(arguments);
    xrtValueRelease(arguments);
    return result;
}
