#ifndef XL_ONNXRUNTIME_H
#define XL_ONNXRUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

/* 文本结果由桥接层持有；返回的句柄由对应 release 函数释放。 */
int ort_available_native(void);
const char* ort_backend_native(void);
const char* ort_get_version_string_native(void);
const char* ort_get_available_providers_json_native(void);
const char* ort_last_error_native(void);

/* 会话与运行配置。 */
void* ort_session_options_create_native(void);
int ort_session_options_set_intra_op_num_threads_native(void* handle, int value);
int ort_session_options_set_inter_op_num_threads_native(void* handle, int value);
int ort_session_options_set_graph_optimization_level_native(void* handle, int value);
int ort_session_options_set_execution_mode_native(void* handle, int value);
int ort_session_options_enable_profiling_native(void* handle, const char* value);
int ort_session_options_disable_profiling_native(void* handle);
int ort_session_options_enable_cpu_mem_arena_native(void* handle);
int ort_session_options_disable_cpu_mem_arena_native(void* handle);
int ort_session_options_enable_mem_pattern_native(void* handle);
int ort_session_options_disable_mem_pattern_native(void* handle);
int ort_session_options_set_log_id_native(void* handle, const char* value);
int ort_session_options_set_log_verbosity_level_native(void* handle, int value);
int ort_session_options_set_log_severity_level_native(void* handle, int value);
int ort_session_options_append_execution_provider_native(void* handle, const char* value);
int ort_session_options_append_execution_provider_with_options_native(void* handle,
    const char* value, const char* options_json);
int ort_session_options_release_native(void* handle);

void* ort_run_options_create_native(void);
int ort_run_options_set_log_verbosity_level_native(void* handle, int value);
int ort_run_options_set_log_severity_level_native(void* handle, int value);
int ort_run_options_set_run_tag_native(void* handle, const char* value);
int ort_run_options_set_terminate_native(void* handle);
int ort_run_options_unset_terminate_native(void* handle);
int ort_run_options_release_native(void* handle);

/* 会话、张量与输出结果。 */
void* ort_session_create_native(const char* model_path, void* options);
int ort_session_release_native(void* handle);
const char* ort_session_inputs_native(void* handle);
const char* ort_session_outputs_native(void* handle);
const char* ort_session_modelmeta_native(void* handle);
const char* ort_session_end_profiling_native(void* handle);
void* ort_session_run_json_native(void* handle, const char* outputs, const char* inputs);
void* ort_session_run_json_with_options_native(void* handle, const char* outputs,
    const char* inputs, void* options);

void* ort_tensor_zeros_f32_native(const char* shape_json);
void* ort_tensor_from_opencv_native(void* image, double scale, double mean0,
    double mean1, double mean2, double std0, double std1, double std2);
void* ort_tensor_from_opencv_resized_native(void* image, int width, int height,
    double scale, double mean0, double mean1, double mean2, double std0,
    double std1, double std2);
void* ort_tensor_from_opencv_batch_native(int batch_count, void* image0,
    void* image1, void* image2, void* image3, void* image4, void* image5,
    double scale, double mean0, double mean1, double mean2, double std0,
    double std1, double std2);
int ort_tensor_release_native(void* handle);
void* ort_session_run_tensor_native(void* handle, const char* input_name,
    void* tensor, const char* outputs);
void* ort_session_run_tensor_fast_native(void* handle, const char* input_name,
    void* tensor, const char* outputs);
const char* ort_result_json_native(void* handle);
const char* ort_result_float_shape_json_native(void* handle);
int ort_result_float_count_native(void* handle);
double ort_result_float_at_native(void* handle, int index);
int ort_result_release_native(void* handle);

/* 与会话绑定的 IO。 */
void* ort_iobinding_create_native(void* session);
int ort_iobinding_bind_cpu_input_native(void* binding, const char* name,
    const char* tensor_json);
int ort_iobinding_bind_output_native(void* binding, const char* name);
int ort_iobinding_clear_inputs_native(void* binding);
int ort_iobinding_clear_outputs_native(void* binding);
int ort_iobinding_synchronize_inputs_native(void* binding);
int ort_iobinding_synchronize_outputs_native(void* binding);
void* ort_session_run_with_iobinding_native(void* session, void* binding,
    void* options);
const char* ort_iobinding_outputs_native(void* binding);
int ort_iobinding_release_native(void* binding);

#ifdef __cplusplus
}
#endif

#endif
