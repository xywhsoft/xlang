#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <windows.h>

int main(void)
{
    _Atomic uint32_t value = 1;
    assert(atomic_fetch_add(&value, 3) == 1);
    assert(atomic_load(&value) == 4);
    assert(WC_ERR_INVALID_CHARS == 0x80);
    puts("Selected source TCC: C11 atomics, Windows SDK and CRT passed");
    return 0;
}
