#include <stdio.h>
#include <string.h>
#include "xrt.h"
#include "../include/dinov3.h"
#include "../../onnxruntime/include/onnxruntime.h"
#include "../../opencv/include/opencv.h"

int main(int argc, char** argv)
{
    void* session = NULL;
    void* image = NULL;
    void* rgb = NULL;
    void* resized = NULL;
    void* tensor = NULL;
    void* result = NULL;
    void* embedding = NULL;
    int status = 1;

    if (argc < 3 || argc > 4 || (argc == 4 && strcmp(argv[3], "--json") != 0)) {
        fprintf(stderr, "usage: live_model_c model.onnx image.png [--json]\n");
        return 2;
    }
    session = ort_session_create_native(argv[1], NULL);
    image = opencv_imread_native(argv[2], 1);
    if (session == NULL || image == NULL) goto done;
    /* 与 XLang 范例采用相同的 RGB、尺寸和归一化路径。 */
    rgb = opencv_cvt_color_native(image, 4);
    if (rgb == NULL) goto done;
    resized = opencv_resize_native(rgb, 224, 224);
    if (resized == NULL) goto done;
    tensor = ort_tensor_from_opencv_native(resized, 1.0 / 255.0,
                                          0.485, 0.456, 0.406,
                                          0.229, 0.224, 0.225);
    if (tensor == NULL) goto done;
    result = ort_session_run_tensor_fast_native(session, "image", tensor, "[\"embedding\"]");
    if (result == NULL || ort_result_float_count_native(result) != 384) goto done;
    if (argc == 4) {
        embedding = dinov3_embedding_json_native(result);
        if (embedding == NULL) goto done;
        printf("%s\n", (const char*)embedding);
    } else {
        printf("count=%d first=%.9g norm=%.9g\n", ort_result_float_count_native(result),
               ort_result_float_at_native(result, 0), dinov3_l2_norm_native(result));
    }
    status = 0;

done:
    if (status) fprintf(stderr, "inference failed: %s\n", ort_last_error_native());
    /* 每个桥接结果都只由创建它的一方释放一次。 */
    xrtFree(embedding);
    if (result != NULL) ort_result_release_native(result);
    if (tensor != NULL) ort_tensor_release_native(tensor);
    if (resized != NULL) opencv_release_native(resized);
    if (rgb != NULL) opencv_release_native(rgb);
    if (image != NULL) opencv_release_native(image);
    if (session != NULL) ort_session_release_native(session);
    return status;
}
