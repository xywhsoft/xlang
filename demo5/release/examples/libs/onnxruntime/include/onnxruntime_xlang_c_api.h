#ifndef ONNXRUNTIME_XLANG_C_API_H
#define ONNXRUNTIME_XLANG_C_API_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void* XlOrtSession;
typedef void* XlOrtSessionOptions;
typedef void* XlOrtRunOptions;
typedef void* XlOrtIoBinding;
typedef void* XlOrtTensor;
typedef void* XlOrtResult;

int onnxruntime_xl_available(void);
const char* onnxruntime_xl_get_version_string(void);
const char* onnxruntime_xl_get_available_providers_json(void);
const char* onnxruntime_xl_last_error(void);
XlOrtSessionOptions onnxruntime_xl_create_session_options(void);
int onnxruntime_xl_session_options_set_intra_op_num_threads(XlOrtSessionOptions options, int value);
int onnxruntime_xl_session_options_set_inter_op_num_threads(XlOrtSessionOptions options, int value);
int onnxruntime_xl_session_options_set_graph_optimization_level(XlOrtSessionOptions options, int value);
int onnxruntime_xl_session_options_set_execution_mode(XlOrtSessionOptions options, int value);
int onnxruntime_xl_session_options_enable_profiling(XlOrtSessionOptions options, const char* prefix);
int onnxruntime_xl_session_options_disable_profiling(XlOrtSessionOptions options);
int onnxruntime_xl_session_options_enable_cpu_mem_arena(XlOrtSessionOptions options);
int onnxruntime_xl_session_options_disable_cpu_mem_arena(XlOrtSessionOptions options);
int onnxruntime_xl_session_options_enable_mem_pattern(XlOrtSessionOptions options);
int onnxruntime_xl_session_options_disable_mem_pattern(XlOrtSessionOptions options);
int onnxruntime_xl_session_options_set_log_id(XlOrtSessionOptions options, const char* value);
int onnxruntime_xl_session_options_set_log_verbosity_level(XlOrtSessionOptions options, int value);
int onnxruntime_xl_session_options_set_log_severity_level(XlOrtSessionOptions options, int value);
int onnxruntime_xl_session_options_append_execution_provider(XlOrtSessionOptions options, const char* provider_name);
int onnxruntime_xl_session_options_append_execution_provider_with_options(XlOrtSessionOptions options, const char* provider_name, const char* options_json);
void onnxruntime_xl_release_session_options(XlOrtSessionOptions options);
XlOrtRunOptions onnxruntime_xl_create_run_options(void);
int onnxruntime_xl_run_options_set_log_verbosity_level(XlOrtRunOptions options, int value);
int onnxruntime_xl_run_options_set_log_severity_level(XlOrtRunOptions options, int value);
int onnxruntime_xl_run_options_set_run_tag(XlOrtRunOptions options, const char* value);
int onnxruntime_xl_run_options_set_terminate(XlOrtRunOptions options);
int onnxruntime_xl_run_options_unset_terminate(XlOrtRunOptions options);
void onnxruntime_xl_release_run_options(XlOrtRunOptions options);
XlOrtSession onnxruntime_xl_create_session(const char* model_path, XlOrtSessionOptions options);
void onnxruntime_xl_release_session(XlOrtSession session);
const char* onnxruntime_xl_get_inputs_json(XlOrtSession session);
const char* onnxruntime_xl_get_outputs_json(XlOrtSession session);
const char* onnxruntime_xl_get_modelmeta_json(XlOrtSession session);
const char* onnxruntime_xl_session_end_profiling(XlOrtSession session);
XlOrtIoBinding onnxruntime_xl_create_iobinding(XlOrtSession session);
int onnxruntime_xl_iobinding_bind_cpu_input_json(XlOrtIoBinding binding, const char* name, const char* tensor_json);
int onnxruntime_xl_iobinding_bind_output(XlOrtIoBinding binding, const char* name);
int onnxruntime_xl_iobinding_clear_inputs(XlOrtIoBinding binding);
int onnxruntime_xl_iobinding_clear_outputs(XlOrtIoBinding binding);
int onnxruntime_xl_iobinding_synchronize_inputs(XlOrtIoBinding binding);
int onnxruntime_xl_iobinding_synchronize_outputs(XlOrtIoBinding binding);
XlOrtResult onnxruntime_xl_run_with_iobinding(XlOrtSession session, XlOrtIoBinding binding, XlOrtRunOptions options);
const char* onnxruntime_xl_iobinding_outputs_json(XlOrtIoBinding binding);
void onnxruntime_xl_release_iobinding(XlOrtIoBinding binding);
XlOrtTensor onnxruntime_xl_tensor_from_f32(const float* data, int data_count, const long long* shape, int shape_count);
XlOrtTensor onnxruntime_xl_tensor_zeros_f32(const char* shape_json);
void onnxruntime_xl_release_tensor(XlOrtTensor tensor);
XlOrtResult onnxruntime_xl_run_tensor(XlOrtSession session, const char* input_name, XlOrtTensor tensor, const char* output_names_json);
XlOrtResult onnxruntime_xl_run_json(XlOrtSession session, const char* output_names_json, const char* input_feed_json);
XlOrtResult onnxruntime_xl_run_json_with_options(XlOrtSession session, const char* output_names_json, const char* input_feed_json, XlOrtRunOptions options);
const char* onnxruntime_xl_result_json(XlOrtResult result);
const char* onnxruntime_xl_result_float_shape_json(XlOrtResult result);
int onnxruntime_xl_result_float_count(XlOrtResult result);
float onnxruntime_xl_result_float_at(XlOrtResult result, int index);
void onnxruntime_xl_release_result(XlOrtResult result);

#ifdef __cplusplus
}
#endif

#endif
