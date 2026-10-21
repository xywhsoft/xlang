#ifndef XL_OPENCV_H
#define XL_OPENCV_H

#ifdef __cplusplus
extern "C" {
#endif

/* 图像句柄为不透明拥有型指针；每个非空结果均以 opencv_release_native 释放。 */
int opencv_available_native(void);
const char* opencv_backend_native(void);
const char* opencv_last_error_native(void);

void* opencv_imread_native(const char* path, int flags);
int opencv_imwrite_native(const char* path, void* image);
void* opencv_resize_native(void* image, int width, int height);
void* opencv_crop_native(void* image, int x, int y, int width, int height);
void* opencv_perspective_crop_native(void* image, double x0, double y0, double x1, double y1,
                                      double x2, double y2, double x3, double y3);
void* opencv_rotate180_native(void* image);
void* opencv_rotate90ccw_native(void* image);
void* opencv_capture_rect_native(int x, int y, int width, int height);
void* opencv_capture_window_native(void* hwnd);
void* opencv_cvt_color_native(void* image, int code);
void* opencv_copy_make_border_native(void* image, int top, int bottom, int left, int right,
                                      int border_type, double value0, double value1,
                                      double value2, double value3);
void* opencv_rectangle_native(void* image, int x1, int y1, int x2, int y2,
                              double value0, double value1, double value2, double value3,
                              int thickness);
void* opencv_normalize_native(void* image, double scale, double mean0, double mean1,
                              double mean2, double std0, double std1, double std2);
/* 返回的文本由后端持有，调用方不得释放；不保证跨后续调用或线程长期有效。 */
const char* opencv_to_chw_tensor_json_native(void* image, double scale, double mean0,
                                               double mean1, double mean2, double std0,
                                               double std1, double std2);
int opencv_width_native(void* image);
int opencv_height_native(void* image);
int opencv_channels_native(void* image);
int opencv_release_native(void* image);

#ifdef __cplusplus
}
#endif

#endif
