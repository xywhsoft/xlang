#include "volc_core_native.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    const char* body = "{\"ProductId\":\"prod-1\"}";
    const char* preview = x_volc_core_sign_preview_acep(
        "ListPod", "2023-10-30", "AKID", "SECRET", "", body,
        "20240102T030405Z");
    if ( !preview || !strstr(preview, "\"authorization\"") ) {
        fputs("ACEP preview failed\n", stderr);
        return 1;
    }
    puts("volc.phone offline example: ok");
    return 0;
}
