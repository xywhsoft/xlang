/* Link-only fixture. No generated program from this gate is executed. */
#if defined(__TINYC__)
#include "src/runtime/xl6_native_com_win_abi.h"
#else
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <oleidl.h>
#endif
int main(void)
{
    STGMEDIUM empty={0};
    ReleaseStgMedium(&empty);
    return 0;
}
