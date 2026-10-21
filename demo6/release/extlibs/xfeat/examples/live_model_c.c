#include <stdio.h>
#include <string.h>
#include "xrt.h"
#include "../include/xfeat.h"
#include "../../onnxruntime/include/onnxruntime.h"
#include "../../opencv/include/opencv.h"

static void* extract(void* session, const char* path, float scale)
{
    void* image = NULL;
    void* scaled = NULL;
    void* rgb = NULL;
    void* padded = NULL;
    void* tensor = NULL;
    void* result = NULL;
    void* features = NULL;
    int width, height, padded_w, padded_h;

    image = opencv_imread_native(path, 1);
    if (image == NULL) goto done;
    width = opencv_width_native(image);
    height = opencv_height_native(image);
    if (width <= 0 || height <= 0 || width > 2147483616 || height > 2147483616) goto done;
    if (scale != 1.0f) {
        double scaled_w = width * (double)scale + 0.5;
        double scaled_h = height * (double)scale + 0.5;
        if (scaled_w > 2147483616.0 || scaled_h > 2147483616.0) goto done;
        width = scaled_w < 16.0 ? 16 : (int)scaled_w;
        height = scaled_h < 16.0 ? 16 : (int)scaled_h;
        scaled = opencv_resize_native(image, width, height);
        if (scaled == NULL) goto done;
    }
    padded_w = ((width + 31) / 32) * 32;
    padded_h = ((height + 31) / 32) * 32;
    /* XFeat 输入为 RGB/0-1，右下补零而不是缩放原图。 */
    rgb = opencv_cvt_color_native(scaled != NULL ? scaled : image, 4);
    if (rgb == NULL) goto done;
    if (padded_w != width || padded_h != height) {
        padded = opencv_copy_make_border_native(rgb, 0, padded_h - height, 0,
                                                padded_w - width, 0, 0, 0, 0, 0);
        if (padded == NULL) goto done;
    }
    tensor = ort_tensor_from_opencv_native(padded != NULL ? padded : rgb,
                                          1.0 / 255.0, 0, 0, 0, 1, 1, 1);
    if (tensor == NULL) goto done;
    result = ort_session_run_tensor_fast_native(session, "image", tensor, "[\"features\"]");
    if (result != NULL) {
        features = xfeat_features_from_result_native(result, width, height, padded_w, padded_h);
    }

done:
    /* 会话由 main 持有，这里只释放本次提取创建的对象。 */
    if (result != NULL) ort_result_release_native(result);
    if (tensor != NULL) ort_tensor_release_native(tensor);
    if (padded != NULL) opencv_release_native(padded);
    if (rgb != NULL) opencv_release_native(rgb);
    if (scaled != NULL) opencv_release_native(scaled);
    if (image != NULL) opencv_release_native(image);
    return features;
}

int main(int argc, char** argv)
{
    static const float scales[] = {0.70f, 0.85f, 1.00f, 1.15f, 1.30f, 1.50f};
    void* session = NULL;
    void* query_set = NULL;
    void* target = NULL;
    void* location = NULL;
    void* json = NULL;
    int status = 1;
    int expect_found;
    int found;
    size_t i;

    if (argc != 4 && (argc != 5 || strcmp(argv[4], "--expect-missing") != 0)) {
        fprintf(stderr, "usage: live_model_c model.onnx query.png target.png [--expect-missing]\n");
        return 2;
    }
    expect_found = argc == 4;
    session = ort_session_create_native(argv[1], NULL);
    if (session == NULL) goto done;
    puts("open: ok");
    query_set = xfeat_query_create_native();
    if (query_set == NULL) goto done;
    for (i = 0; i < sizeof(scales) / sizeof(scales[0]); ++i) {
        void* features = extract(session, argv[2], scales[i]);
        if (features != NULL && !xfeat_query_add_native(query_set, features, scales[i]))
            xfeat_features_release_native(features);
    }
    if (xfeat_query_count_native(query_set) == 0) goto done;
    puts("prepare_query: ok");
    target = extract(session, argv[3], 1.0f);
    if (target == NULL) goto done;
    for (i = 0; i < (size_t)xfeat_query_count_native(query_set); ++i) {
        void* candidate = xfeat_match_native(xfeat_query_features_native(query_set, (int)i),
            target, 0.80, 4, 6.0, xfeat_query_scale_native(query_set, (int)i));
        if (candidate == NULL) continue;
        if (location == NULL || xfeat_location_score_native(candidate) > xfeat_location_score_native(location)) {
            if (location != NULL) xfeat_location_release_native(location);
            location = candidate;
        } else {
            xfeat_location_release_native(candidate);
        }
    }
    if (location == NULL) goto done;
    found = xfeat_location_found_native(location);
    json = xfeat_location_json_native(location, argv[3], "");
    if (json == NULL) goto done;
    puts((const char*)json);
    status = found == expect_found ? 0 : 1;

done:
    if (status) fprintf(stderr, "localization failed: %s %s\n",
                        xfeat_last_error_native(), ort_last_error_native());
    xrtFree(json);
    if (location != NULL) xfeat_location_release_native(location);
    if (target != NULL) xfeat_features_release_native(target);
    if (query_set != NULL) xfeat_query_release_native(query_set);
    if (session != NULL) ort_session_release_native(session);
    return status;
}
