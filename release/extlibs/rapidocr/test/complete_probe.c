#include "rapidocr.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    void* engine;
    void* output;
    if (!rapidocr_available_native() || rapidocr_backend_native()[0] == 0)
        return 1;
    engine = rapidocr_create_native(NULL);
    if (engine == NULL) return 1;

    output = rapidocr_call_native(engine, "ok.png", 1, 1, 1, 0);
    if (output == NULL || !rapidocr_output_ok_native(output) ||
        strcmp(rapidocr_output_text_native(output), "mock text") != 0 ||
        strcmp(rapidocr_output_boxes_native(output), "[]") != 0 ||
        rapidocr_output_elapse_native(output) != 0.25 ||
        !rapidocr_output_release_native(output)) return 1;

    if (rapidocr_call_native(engine, "missing.png", 1, 1, 1, 0) != NULL ||
        strstr(rapidocr_last_error_native(), "mock missing image") == NULL)
        return 1;
    output = rapidocr_call_native(engine, "ok.png", 1, 1, 1, 0);
    if (output == NULL || rapidocr_last_error_native()[0] != 0 ||
        !rapidocr_output_release_native(output) || !rapidocr_release_native(engine))
        return 1;
    puts("rapidocr complete shim call-through: PASS");
    return 0;
}
