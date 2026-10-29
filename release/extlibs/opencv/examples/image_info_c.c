#include <stdio.h>
#include "../include/opencv.h"
#ifdef __TINYC__
/* 单文件 TCC 范例复用包内公开包装层，图像桥接 DLL 仍动态装载。 */
#include "../opencv_artifact.c"
#endif

int main(void)
{
    void* source;
    void* resized;
    if ( !opencv_available_native() ) {
        fprintf(stderr, "OpenCV unavailable: %s\n", opencv_last_error_native());
        return 1;
    }
    source = opencv_imread_native("extlibs/opencv/test/fixture.ppm", 1);
    if ( source == NULL ) {
        fprintf(stderr, "cannot read image: %s\n", opencv_last_error_native());
        return 2;
    }
    resized = opencv_resize_native(source, 4, 4);
    if ( resized == NULL ) {
        opencv_release_native(source);
        fprintf(stderr, "cannot resize image: %s\n", opencv_last_error_native());
        return 3;
    }
    printf("opencv image: %d x %d -> %d x %d\n",
           opencv_width_native(source), opencv_height_native(source),
           opencv_width_native(resized), opencv_height_native(resized));
    opencv_release_native(resized);
    opencv_release_native(source);
    return 0;
}
