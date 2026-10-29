#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_NET_FILE
#if defined(XL6_NET_FILE_NATIVE_HEADER)
#include "net-file-native.h"
#else
#include "lib/xrt/xrt.h"
#endif
#include "net_file_boundary_cases.h"
int main(void)
{
    assert(xrtMemDebugEnable(true));
    xrtSetErrorKind(XERR_STATE);xrtClearError();
    testNetFileBoundary();return 0;
}
