#ifndef RAPIDOCR_H
#define RAPIDOCR_H

#ifdef __cplusplus
extern "C" {
#endif

/* C 后处理文本返回独立 char*，调用方使用 xrtFree 释放。 */
int rapidocr_available_native(void);
/* backend/last_error 返回借用字符串；extlib_path 返回需 xrtFree 的独立字符串。 */
const char* rapidocr_backend_native(void);
const char* rapidocr_last_error_native(void);
char* rapidocr_extlib_path_native(const char* relative);
int rapidocr_box_count_native(const char* boxes_json);
int rapidocr_first_box_x_native(const char* boxes_json);
int rapidocr_first_box_y_native(const char* boxes_json);
int rapidocr_first_box_width_native(const char* boxes_json);
int rapidocr_first_box_height_native(const char* boxes_json);
int rapidocr_box_x_native(const char* boxes_json, int index);
int rapidocr_box_y_native(const char* boxes_json, int index);
int rapidocr_box_width_native(const char* boxes_json, int index);
int rapidocr_box_height_native(const char* boxes_json, int index);
double rapidocr_box_point_x_native(const char* boxes_json, int index, int point_index);
double rapidocr_box_point_y_native(const char* boxes_json, int index, int point_index);
int rapidocr_box_rec_target_width_native(const char* boxes_json, int index,
    int rec_img_w, int rec_img_h);
/* 几何快照复制坐标；原 JSON 可在 open 后释放。句柄由 release 归还。 */
void* rapidocr_boxes_open_native(const char* boxes_json);
int rapidocr_boxes_release_native(void* handle);
int rapidocr_boxes_count_handle_native(void* handle);
double rapidocr_boxes_coord_native(void* handle, int index, int coordinate);
int rapidocr_boxes_rec_target_width_native(void* handle, int index,
    int rec_img_w, int rec_img_h);
int rapidocr_vis_image_native(const char* source_path, const char* boxes_json,
    const char* save_path);
int rapidocr_cls_should_rotate_native(const char* cls_json, double threshold);
char* rapidocr_charset_info_native(const char* path);
/* 句柄在 release 前可复用于多个识别结果；字典文件在 open 时读取一次。 */
void* rapidocr_charset_open_native(const char* path);
int rapidocr_charset_release_native(void* charset);
/* 三种识别解码入口同样返回 xrtFree 释放的独立字符串；失败返回 NULL。 */
char* rapidocr_rec_decode_native(const char* output_json, const char* keys_path);
char* rapidocr_rec_decode_result_native(void* result_handle, const char* keys_path);
char* rapidocr_rec_decode_result_at_native(void* result_handle,
    const char* keys_path, int batch_index);
char* rapidocr_rec_decode_result_at_charset_native(void* result_handle,
    void* charset, int batch_index);
char* rapidocr_cls_decode_result_native(void* result_handle);
char* rapidocr_cls_decode_result_at_native(void* result_handle, int batch_index);
char* rapidocr_to_ocr_items_json_native(const char* result_json);
char* rapidocr_to_ocr_items_json2_native(const char* result_json, double text_score);
char* rapidocr_det_boxes_from_output_native(const char* output_json,
    int image_width, int image_height, double threshold);
char* rapidocr_det_boxes_from_result_native(void* result_handle,
    int image_width, int image_height, double threshold);
char* rapidocr_det_boxes_from_result_db_native(void* result_handle,
    int image_width, int image_height, double threshold, double box_thresh,
    double unclip_ratio, int use_dilation, int min_box_size, int filter_small_boxes);
/* assemble 系列返回独立的 XRT 字符串，调用方用 xrtFree 释放；失败返回 NULL。 */
char* rapidocr_assemble_result_native(const char* boxes_json,
    const char* rec_json, double elapsed_seconds);
char* rapidocr_assemble_result2_native(const char* boxes_json,
    const char* rec0_json, const char* rec1_json, double elapsed_seconds);
char* rapidocr_assemble_result4_native(const char* boxes_json,
    const char* rec0_json, const char* rec1_json, const char* rec2_json,
    const char* rec3_json, double elapsed_seconds);
char* rapidocr_assemble_result_list_native(const char* boxes_json,
    const char* recs_json, double elapsed_seconds);

/* 可选原生桥接层入口；缺失或符号不全时 create/call 返回 NULL，不创建伪结果。 */
void* rapidocr_create_native(const char* config_path);
int rapidocr_release_native(void* handle);
void* rapidocr_call_native(void* handle, const char* image_path,
    int use_det, int use_cls, int use_rec, int return_word_box);
int rapidocr_output_release_native(void* handle);
int rapidocr_output_ok_native(void* handle);
const char* rapidocr_output_json_native(void* handle);
const char* rapidocr_output_text_native(void* handle);
const char* rapidocr_output_boxes_native(void* handle);
const char* rapidocr_output_txts_native(void* handle);
const char* rapidocr_output_scores_native(void* handle);
const char* rapidocr_output_word_results_native(void* handle);
const char* rapidocr_output_elapse_list_native(void* handle);
double rapidocr_output_elapse_native(void* handle);
int rapidocr_output_vis_native(void* handle, const char* save_path);

#ifdef __cplusplus
}
#endif

#endif
