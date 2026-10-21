#include <stdio.h>
#include <string.h>
#include "../include/opencv.h"

int main(void)
{
    if (strstr(opencv_last_error_native(), "required symbols") == NULL) return 4;
    if (opencv_available_native()) return 1;
    if (opencv_backend_native()[0] != '\0') return 2;
    if (strstr(opencv_last_error_native(), "required symbols") == NULL) return 3;
    puts("opencv partial shim: PASS");
    return 0;
}
