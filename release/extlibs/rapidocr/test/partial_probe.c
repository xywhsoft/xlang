#include "rapidocr.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    if (rapidocr_available_native() != 0 ||
        strstr(rapidocr_last_error_native(), "symbols") == NULL ||
        rapidocr_create_native(NULL) != NULL ||
        rapidocr_call_native(NULL, "missing.png", 1, 1, 1, 0) != NULL ||
        rapidocr_output_json_native(NULL) != NULL) {
        fputs("incomplete RapidOCR bridge was accepted\n", stderr);
        return 1;
    }
    return 0;
}
