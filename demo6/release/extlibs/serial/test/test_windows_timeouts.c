/* 直接包含生产实现，检查发往 Win32 的配置值；不需要实体串口。 */
#include "../serial_native.c"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>

int main(void)
{
    COMMTIMEOUTS timeouts = serial_windows_timeouts(0, 0);
    assert(timeouts.ReadIntervalTimeout == MAXDWORD);
    assert(timeouts.ReadTotalTimeoutMultiplier == 0);
    assert(timeouts.ReadTotalTimeoutConstant == 0);
    assert(timeouts.WriteTotalTimeoutMultiplier == 0);
    assert(timeouts.WriteTotalTimeoutConstant == 1);

    timeouts = serial_windows_timeouts(150, 1000);
    assert(timeouts.ReadTotalTimeoutConstant == 150);
    assert(timeouts.WriteTotalTimeoutConstant == 1000);
    puts("serial Windows timeouts: PASS");
    return 0;
}
