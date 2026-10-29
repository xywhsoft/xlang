#include <stdio.h>
#include "../include/opencv.h"

int main(void)
{
    void* source;
    void* bad;
    if ( !opencv_available_native() ) return 1;
    if ( opencv_imread_native("extlibs/opencv/test/not_found.ppm", 1) != NULL ) return 2;
    source = opencv_imread_native("extlibs/opencv/test/fixture.ppm", 1);
    if ( source == NULL || opencv_width_native(source) != 2 ||
         opencv_height_native(source) != 2 || opencv_channels_native(source) != 3 ) return 3;
    bad = opencv_resize_native(source, 0, 4);
    if ( bad != NULL ) {
        opencv_release_native(bad);
        opencv_release_native(source);
        return 4;
    }
    if ( !opencv_release_native(source) ) return 5;
    puts("opencv C boundary: PASS");
    return 0;
}
