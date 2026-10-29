#define XRT_MODULE_CRC32
#include "xrt.h"

int main(void)
{
    return xrtCrc32("123456789", 9) == UINT32_C(0xcbf43926) ? 0 : 1;
}
