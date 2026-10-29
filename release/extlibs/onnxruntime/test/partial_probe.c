#include <stdio.h>
#include <string.h>
#include "../include/onnxruntime.h"

int main(int argc, char** argv)
{
    const char* expected = argc > 1 ? "not found at ONNXRUNTIME_XLANG_DLL" :
        "required symbols";
    (void)argv;
    if (ort_tensor_from_opencv_native(NULL, 1.0, 0.0, 0.0, 0.0,
            1.0, 1.0, 1.0) != NULL) return 4;
    if (ort_available_native()) return 1;
    if (ort_backend_native()[0] != '\0') return 2;
    if (strstr(ort_last_error_native(), expected) == NULL) return 3;
    puts("onnxruntime partial shim: PASS");
    return 0;
}
