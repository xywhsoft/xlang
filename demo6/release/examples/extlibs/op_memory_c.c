#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include "xrt.h"
#include "../../extlibs/op/include/op.h"

int main(void)
{
    char message[] = "xlang-op";
    long long pid = (long long)GetCurrentProcessId();
    long long address = (long long)(intptr_t)message;
    char *text = xop_read_string(pid, address, sizeof(message));
    char *found = xop_find_memory(pid, address, address + sizeof(message),
                                  "78 6C 61 6E 67");
    if (!text || !found) {
        xrtFree(text);
        xrtFree(found);
        return 1;
    }
    puts(text);
    puts(*found ? "true" : "false");
    xrtFree(text);
    xrtFree(found);
    return 0;
}
