#ifndef DINOV3_H
#define DINOV3_H

#ifdef __cplusplus
extern "C" {
#endif

const char* dinov3_version_native(void);
int dinov3_available_native(void);
const char* dinov3_last_error_native(void);

/* JSON 为独立 XRT 字符串；C 调用方使用 xrtFree 释放。 */
void* dinov3_embedding_json_native(void* result_handle);
void* dinov3_output_json_native(void* result_handle, const char* source_path);
double dinov3_l2_norm_native(void* result_handle);
double dinov3_cosine_native(void* left_handle, void* right_handle);

#ifdef __cplusplus
}
#endif

#endif
