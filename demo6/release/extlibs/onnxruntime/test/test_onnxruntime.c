/* ONNX Runtime C smoke: load, lifecycle and invalid-input behavior. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../include/onnxruntime.h"

static int fail(const char* message)
{
    fprintf(stderr, "onnxruntime C smoke failed: %s\n", message);
    return 1;
}

int main(int argc, char** argv)
{
    void* options;
    void* tensor;
    void* missing;
    void* session;
    void* result;

    if (!ort_available_native() || ort_backend_native()[0] == 0 ||
        ort_get_version_string_native()[0] == 0 ||
        strstr(ort_get_available_providers_json_native(), "CPUExecutionProvider") == NULL) {
        return fail("runtime discovery");
    }
    options = ort_session_options_create_native();
    if (options == NULL || !ort_session_options_set_intra_op_num_threads_native(options, 1) ||
        !ort_session_options_set_inter_op_num_threads_native(options, 1)) {
        ort_session_options_release_native(options);
        return fail("session options");
    }
    tensor = ort_tensor_zeros_f32_native("[1,2]");
    if (tensor == NULL || !ort_tensor_release_native(tensor)) {
        ort_session_options_release_native(options);
        return fail("tensor lifecycle");
    }
    missing = ort_session_create_native("__xlang_missing_model__.onnx", options);
    if (missing != NULL || strcmp(ort_session_inputs_native(missing), "[]") != 0 ||
        ort_last_error_native()[0] == 0 || !ort_session_release_native(missing)) {
        ort_session_options_release_native(options);
        return fail("missing model error path");
    }
    if (!ort_session_options_release_native(options)) {
        return fail("options release");
    }

    if (argc < 2) return fail("model path is required");
    if (argc >= 3 && strcmp(argv[2], "--expect-utf8-path") == 0 &&
        (strstr(ort_backend_native(), "utf8-") == NULL ||
         strstr(ort_backend_native(), "\xE4\xB8\xAD\xE6\x96\x87") == NULL)) {
        fprintf(stderr, "selected backend: %s\n", ort_backend_native());
        return fail("UTF-8 loader path");
    }
    session = ort_session_create_native(argv[1], NULL);
    if (session == NULL || strstr(ort_session_inputs_native(session), "\"x\"") == NULL ||
        strstr(ort_session_outputs_native(session), "save_infer_model/scale_0.tmp_1") == NULL) {
        return fail("load classifier model");
    }
    tensor = ort_tensor_zeros_f32_native("[1,3,48,192]");
    if (tensor == NULL) return fail("classifier input tensor");
    result = ort_session_run_tensor_native(session, "x", tensor,
        "[\"save_infer_model/scale_0.tmp_1\"]");
    if (result == NULL || ort_result_float_count_native(result) != 2 ||
        !isfinite(ort_result_float_at_native(result, 0)) ||
        !isfinite(ort_result_float_at_native(result, 1))) {
        return fail("classifier inference");
    }
    if (!ort_result_release_native(result) || !ort_tensor_release_native(tensor) ||
        !ort_session_release_native(session)) {
        return fail("classifier release");
    }
    puts("onnxruntime C smoke: PASS");
    return 0;
}
