#ifndef RAPIDOCR_JSON_PRIVATE_H
#define RAPIDOCR_JSON_PRIVATE_H

#include <stddef.h>

#define RAPIDOCR_MAX_TENSOR_CELLS (4u * 1024u * 1024u)

typedef struct RapidJsonBox {
	int points[8];
	double score;
	int pixels;
} RapidJsonBox;

typedef struct RapidJsonGeometry {
	int count;
	double points[];
} RapidJsonGeometry;

/* 返回的 XRT 字符串由调用方接管；张量数值数组由调用方以 free 释放。 */
void* rapidocr_rec_json_encode(const void* text, size_t size, double score, int tokens);
void* rapidocr_charset_json_encode(const char* path, int count,
    const char* first, const char* last);
void* rapidocr_cls_json_encode(int index, double score, int classes);
void* rapidocr_ocr_items_json_encode(const char* result_json, double min_score);
void* rapidocr_boxes_json_encode(const RapidJsonBox* boxes, size_t count);
int rapidocr_json_shape_dims(const char* source, int* out_dims, int capacity);
int rapidocr_json_tensor_values(const char* source, int* out_h, int* out_w,
	double** out_values);
int rapidocr_json_box_count(const char* source);
int rapidocr_json_box_points_at(const char* source, int index, double points[8]);
RapidJsonGeometry* rapidocr_json_geometry_open(const char* source);

#endif
