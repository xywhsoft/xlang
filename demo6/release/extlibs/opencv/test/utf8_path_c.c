#include <stdio.h>
#include "../include/opencv.h"

int main(int argc, char** argv)
{
    const char* path = argc > 1 ? argv[1] : "../build/test/opencv/中文.ppm";
    void* image = opencv_imread_native(path, 1);
    if ( image == NULL ) {
        fprintf(stderr, "UTF-8 image path failed: %s\n", opencv_last_error_native());
        return 1;
    }
    if ( opencv_width_native(image) != 2 || opencv_height_native(image) != 2 ) {
        opencv_release_native(image);
        return 2;
    }
    opencv_release_native(image);
    puts("opencv C UTF-8 path: PASS");
    return 0;
}
