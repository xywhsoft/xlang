/* ONNX Runtime C smoke: load, lifecycle and invalid-input behavior. */
#include <stdio.h>
#include <string.h>

int ort_available_native(void);
const char* ort_backend_native(void);
const char* ort_get_version_string_native(void);
const char* ort_get_available_providers_json_native(void);
const char* ort_last_error_native(void);
void* ort_session_options_create_native(void);
int ort_session_options_set_intra_op_num_threads_native(void* handle, int value);
int ort_session_options_set_inter_op_num_threads_native(void* handle, int value);
int ort_session_options_release_native(void* handle);
void* ort_session_create_native(const char* model_path, void* options);
int ort_session_release_native(void* handle);
const char* ort_session_inputs_native(void* handle);
void* ort_tensor_zeros_f32_native(const char* shape_json);
int ort_tensor_release_native(void* handle);

static int fail(const char* message)
{
    fprintf(stderr, "onnxruntime C smoke failed: %s\n", message);
    return 1;
}

int main(void)
{
    void* options;
    void* tensor;
    void* missing;

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
    puts("onnxruntime C smoke: PASS");
    return 0;
}
