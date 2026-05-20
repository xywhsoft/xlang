#ifndef OPENCV_XLANG_C_API_H
#define OPENCV_XLANG_C_API_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void* XlCvImage;

int opencv_xl_available(void);
const char* opencv_xl_version(void);
const char* opencv_xl_last_error(void);
XlCvImage opencv_xl_imread(const char* path, int flags);
int opencv_xl_imwrite(const char* path, XlCvImage image);
XlCvImage opencv_xl_resize(XlCvImage image, int width, int height);
XlCvImage opencv_xl_crop(XlCvImage image, int x, int y, int width, int height);
XlCvImage opencv_xl_perspective_crop(XlCvImage image, double x0, double y0, double x1, double y1, double x2, double y2, double x3, double y3);
XlCvImage opencv_xl_rotate180(XlCvImage image);
XlCvImage opencv_xl_rotate90ccw(XlCvImage image);
XlCvImage opencv_xl_cvt_color(XlCvImage image, int code);
XlCvImage opencv_xl_copy_make_border(XlCvImage image, int top, int bottom, int left, int right, int border_type, double value0, double value1, double value2, double value3);
XlCvImage opencv_xl_rectangle(XlCvImage image, int x1, int y1, int x2, int y2, double value0, double value1, double value2, double value3, int thickness);
XlCvImage opencv_xl_normalize(XlCvImage image, double scale, double mean0, double mean1, double mean2, double std0, double std1, double std2);
const char* opencv_xl_to_chw_tensor_json(XlCvImage image, double scale, double mean0, double mean1, double mean2, double std0, double std1, double std2);
int opencv_xl_to_chw_tensor_f32(XlCvImage image, double scale, double mean0, double mean1, double mean2, double std0, double std1, double std2, float** out_data, long long* out_shape, int* out_count);
int opencv_xl_to_chw_tensor_f32_resized(XlCvImage image, int width, int height, double scale, double mean0, double mean1, double mean2, double std0, double std1, double std2, float** out_data, long long* out_shape, int* out_count);
void opencv_xl_release_tensor_f32(float* data);
int opencv_xl_width(XlCvImage image);
int opencv_xl_height(XlCvImage image);
int opencv_xl_channels(XlCvImage image);
void opencv_xl_release(XlCvImage image);

#ifdef __cplusplus
}
#endif

#endif
