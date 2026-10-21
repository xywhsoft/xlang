#ifndef XFEAT_H
#define XFEAT_H

#ifdef __cplusplus
extern "C" {
#endif

const char* xfeat_version_native(void);
int xfeat_available_native(void);
const char* xfeat_last_error_native(void);
double xfeat_now_ms_native(void);
int xfeat_stats_reset_native(void);
int xfeat_stats_add_native(int kind, double preprocess_ms, double ort_ms, double parse_ms);
/* JSON 返回独立的 XRT 字符串；C 调用方使用 xrtFree 释放。 */
void* xfeat_stats_json_native(void);

void* xfeat_engine_create_native(void* query_session, void* target_session);
void* xfeat_engine_query_session_native(void* handle);
void* xfeat_engine_target_session_native(void* handle);
int xfeat_engine_release_native(void* handle);
void* xfeat_query_create_native(void);
int xfeat_query_add_native(void* handle, void* features, double scale);
int xfeat_query_count_native(void* handle);
void* xfeat_query_features_native(void* handle, int index);
double xfeat_query_scale_native(void* handle, int index);
int xfeat_query_release_native(void* handle);

/* image 为原图尺寸，padded 为模型输入尺寸；补零区的特征不进入结果。 */
void* xfeat_features_from_result_native(void* result_handle, int image_w, int image_h,
                                        int padded_w, int padded_h);
int xfeat_features_release_native(void* handle);
int xfeat_features_count_native(void* handle);
void* xfeat_match_native(void* query_handle, void* target_handle, double min_cossim,
                         int min_inliers, double ransac_reproj, double scale);
int xfeat_location_release_native(void* handle);
int xfeat_location_found_native(void* handle);
double xfeat_location_score_native(void* handle);
int xfeat_location_bbox_x_native(void* handle);
int xfeat_location_bbox_y_native(void* handle);
int xfeat_location_bbox_w_native(void* handle);
int xfeat_location_bbox_h_native(void* handle);
void* xfeat_location_json_native(void* handle, const char* image_name,
                                 const char* output_name);

#ifdef __cplusplus
}
#endif

#endif
