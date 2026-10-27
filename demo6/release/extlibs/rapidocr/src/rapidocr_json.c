#include <math.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "xrt.h"
#include "rapidocr.h"
#include "rapidocr_json_private.h"

#if defined(_WIN32) || defined(_WIN64)
#define RAPIDOCR_EXPORT __declspec(dllexport)
#else
#define RAPIDOCR_EXPORT
#endif

#define RAPIDOCR_MAX_JSON_BYTES (4u * 1024u * 1024u)
#define RAPIDOCR_MAX_RESULTS 256u
#define RAPIDOCR_MAX_TENSOR_JSON_BYTES (64u * 1024u * 1024u)

static int rapidocr_json_number(const xvalue* value, double* out);

int rapidocr_json_shape_dims(const char* source, int* out_dims, int capacity)
{
	xvalue* shape;
	size_t bytes;
	size_t count;
	size_t i;
	if (source == NULL || out_dims == NULL || capacity < 1 || capacity > 8) return 0;
	bytes = strlen(source);
	if (bytes == 0u || bytes > 256u) return 0;
	shape = xrtJsonParse((xstrview){ source, bytes });
	if (xrtValueType(shape) != XVALUE_ARRAY) goto failed;
	count = xrtValueCount(shape);
	if (count == 0u || count > (size_t)capacity) goto failed;
	for (i = 0u; i < count; ++i) {
		int64 value;
		if (!xrtValueGetInt(xrtValueArrayGet(shape, i), &value) ||
			value <= 0 || value > INT_MAX) goto failed;
		out_dims[i] = (int)value;
	}
	xrtValueRelease(shape);
	return (int)count;
failed:
	xrtValueRelease(shape);
	xrtClearError();
	return 0;
}

/* 原始 ONNX JSON 只接受根对象中的完整数值张量，避免误取嵌套字段。 */
int rapidocr_json_tensor_values(const char* source, int* out_h, int* out_w,
	double** out_values)
{
	xvalue* root = NULL;
	xvalue* shape;
	xvalue* data;
	double* values = NULL;
	size_t bytes;
	size_t dims;
	size_t count = 1u;
	size_t i;
	int h = 0;
	int w = 0;
	if (out_h == NULL || out_w == NULL || out_values == NULL) return 0;
	*out_h = 0;
	*out_w = 0;
	*out_values = NULL;
	if (source == NULL) return 0;
	bytes = strlen(source);
	if (bytes == 0u || bytes > RAPIDOCR_MAX_TENSOR_JSON_BYTES) return 0;
	root = xrtJsonParse((xstrview){ source, bytes });
	if (xrtValueType(root) != XVALUE_OBJECT) goto failed;
	shape = xrtValueObjectGet(root, xrtStrView("shape"));
	data = xrtValueObjectGet(root, xrtStrView("data"));
	if (xrtValueType(shape) != XVALUE_ARRAY || xrtValueType(data) != XVALUE_ARRAY) goto failed;
	dims = xrtValueCount(shape);
	if (dims < 2u || dims > 8u) goto failed;
	for (i = 0u; i < dims; ++i) {
		int64 dimension;
		if (!xrtValueGetInt(xrtValueArrayGet(shape, i), &dimension) ||
			dimension <= 0 || dimension > INT_MAX ||
			count > RAPIDOCR_MAX_TENSOR_CELLS / (size_t)dimension) goto failed;
		count *= (size_t)dimension;
		if (i == dims - 2u) h = (int)dimension;
		if (i == dims - 1u) w = (int)dimension;
	}
	if (xrtValueCount(data) != count) goto failed;
	values = (double*)malloc(count * sizeof(double));
	if (values == NULL) goto failed;
	for (i = 0u; i < count; ++i) {
		if (!rapidocr_json_number(xrtValueArrayGet(data, i), &values[i])) goto failed;
	}
	xrtValueRelease(root);
	*out_h = h;
	*out_w = w;
	*out_values = values;
	return 1;

failed:
	free(values);
	xrtValueRelease(root);
	xrtClearError();
	return 0;
}

void* rapidocr_rec_json_encode(const void* text, size_t size, double score, int tokens)
{
	xvalue* result;
	char* output = NULL;
	if ((text == NULL && size != 0u) || size > RAPIDOCR_MAX_JSON_BYTES ||
		!isfinite(score) || tokens < 0) return NULL;
	result = xrtValueObject();
	if (result == NULL) return NULL;
	if (xrtValueObjectSetNew(result, xrtStrView("text"),
			xrtValueString((xstrview){ (const char*)text, size })) &&
		xrtValueObjectSetNew(result, xrtStrView("score"), xrtValueFloat(score)) &&
		xrtValueObjectSetNew(result, xrtStrView("tokens"), xrtValueInt(tokens))) {
		output = xrtJsonStringify(result, false, NULL);
	}
	xrtValueRelease(result);
	return output;
}

void* rapidocr_charset_json_encode(const char* path, int count,
	const char* first, const char* last)
{
	xvalue* result;
	char* output = NULL;
	if (path == NULL || first == NULL || last == NULL || count < 0) return NULL;
	result = xrtValueObject();
	if (result == NULL) return NULL;
	if (xrtValueObjectSetNew(result, xrtStrView("path"), xrtValueString(xrtStrView(path))) &&
		xrtValueObjectSetNew(result, xrtStrView("count"), xrtValueInt(count)) &&
		xrtValueObjectSetNew(result, xrtStrView("first"), xrtValueString(xrtStrView(first))) &&
		xrtValueObjectSetNew(result, xrtStrView("last"), xrtValueString(xrtStrView(last)))) {
		output = xrtJsonStringify(result, false, NULL);
	}
	xrtValueRelease(result);
	return output;
}

void* rapidocr_cls_json_encode(int index, double score, int classes)
{
	xvalue* result;
	char* output = NULL;
	if (index < 0 || classes <= index || !isfinite(score)) return NULL;
	result = xrtValueObject();
	if (result == NULL) return NULL;
	if (xrtValueObjectSetNew(result, xrtStrView("label"),
			xrtValueString(xrtStrView(index == 1 ? "180" : "0"))) &&
		xrtValueObjectSetNew(result, xrtStrView("index"), xrtValueInt(index)) &&
		xrtValueObjectSetNew(result, xrtStrView("score"), xrtValueFloat(score)) &&
		xrtValueObjectSetNew(result, xrtStrView("classes"), xrtValueInt(classes))) {
		output = xrtJsonStringify(result, false, NULL);
	}
	xrtValueRelease(result);
	return output;
}

static int rapidocr_json_number(const xvalue* value, double* out)
{
	int64 signed_value;
	uint64 unsigned_value;
	switch (xrtValueType(value)) {
	case XVALUE_FLOAT:
		if (!xrtValueGetFloat(value, out)) return 0;
		break;
	case XVALUE_INT:
		if (!xrtValueGetInt(value, &signed_value)) return 0;
		*out = (double)signed_value;
		break;
	case XVALUE_UINT:
		if (!xrtValueGetUInt(value, &unsigned_value)) return 0;
		*out = (double)unsigned_value;
		break;
	default:
		return 0;
	}
	return isfinite(*out) ? 1 : 0;
}

/* 检测框只从根数组读取；不能把嵌套字符串或其他对象的 box 当成坐标。 */
static int rapidocr_json_box_item(const xvalue* item_value, double points[8])
{
	const xvalue* item = item_value;
	size_t i;
	if (xrtValueType(item) == XVALUE_OBJECT)
		item = xrtValueObjectGet(item, xrtStrView("box"));
	if (xrtValueType(item) != XVALUE_ARRAY || xrtValueCount(item) != 4u) return 0;
	for (i = 0u; i < 4u; ++i) {
		xvalue* pair = xrtValueArrayGet(item, i);
		if (xrtValueType(pair) != XVALUE_ARRAY || xrtValueCount(pair) != 2u ||
			!rapidocr_json_number(xrtValueArrayGet(pair, 0u), &points[i * 2u]) ||
			!rapidocr_json_number(xrtValueArrayGet(pair, 1u), &points[i * 2u + 1u]))
			return 0;
	}
	return 1;
}

RapidJsonGeometry* rapidocr_json_geometry_open(const char* source)
{
	xvalue* root;
	size_t count;
	size_t i;
	RapidJsonGeometry* geometry = NULL;
	if (source == NULL || strlen(source) > RAPIDOCR_MAX_JSON_BYTES) return NULL;
	root = xrtJsonParse(xrtStrView(source));
	if (xrtValueType(root) != XVALUE_ARRAY) goto cleanup;
	count = xrtValueCount(root);
	if (count > RAPIDOCR_MAX_RESULTS) goto cleanup;
	geometry = (RapidJsonGeometry*)xrtMalloc(sizeof(*geometry) + count * 8u * sizeof(double));
	if (geometry == NULL) goto cleanup;
	geometry->count = (int)count;
	for (i = 0u; i < count; ++i) {
		if (!rapidocr_json_box_item(xrtValueArrayGet(root, i), geometry->points + i * 8u))
			goto cleanup;
	}
	xrtValueRelease(root);
	return geometry;
cleanup:
	xrtValueRelease(root);
	xrtFree(geometry);
	xrtClearError();
	return NULL;
}

int rapidocr_json_box_count(const char* source)
{
	RapidJsonGeometry* geometry = rapidocr_json_geometry_open(source);
	int count = geometry != NULL ? geometry->count : 0;
	xrtFree(geometry);
	return count;
}

int rapidocr_json_box_points_at(const char* source, int index, double points[8])
{
	RapidJsonGeometry* geometry = rapidocr_json_geometry_open(source);
	int ok = geometry != NULL && points != NULL && index >= 0 && index < geometry->count;
	if (ok) memcpy(points, geometry->points + (size_t)index * 8u, 8u * sizeof(double));
	xrtFree(geometry);
	return ok;
}

/* OCR 对照 JSON 从三个并列数组组装，文本和结果均不借用输入缓冲。 */
void* rapidocr_ocr_items_json_encode(const char* result_json, double min_score)
{
	xvalue* root = NULL;
	xvalue* boxes;
	xvalue* texts;
	xvalue* scores;
	xvalue* items = NULL;
	char* output = NULL;
	size_t count = 0u;
	size_t i;
	if (!isfinite(min_score)) return NULL;
	if (min_score < 0.0) min_score = 0.0;
	items = xrtValueArray();
	if (items == NULL) return NULL;
	if (result_json == NULL || result_json[0] == '\0') goto serialize;
	if (strlen(result_json) > RAPIDOCR_MAX_JSON_BYTES) goto cleanup;
	root = xrtJsonParse(xrtStrView(result_json));
	if (xrtValueType(root) != XVALUE_OBJECT) goto cleanup;
	boxes = xrtValueObjectGet(root, xrtStrView("boxes"));
	texts = xrtValueObjectGet(root, xrtStrView("txts"));
	scores = xrtValueObjectGet(root, xrtStrView("scores"));
	if (xrtValueType(boxes) != XVALUE_ARRAY ||
		xrtValueType(texts) != XVALUE_ARRAY ||
		xrtValueType(scores) != XVALUE_ARRAY) goto cleanup;
	count = xrtValueCount(boxes);
	if (count > RAPIDOCR_MAX_RESULTS ||
		xrtValueCount(texts) < count || xrtValueCount(scores) < count) goto cleanup;
	for (i = 0u; i < count; ++i) {
		xvalue* box = xrtValueArrayGet(boxes, i);
		xvalue* text = xrtValueArrayGet(texts, i);
		xvalue* score_value = xrtValueArrayGet(scores, i);
		xvalue* item;
		xstrview word;
		double score;
		double min_x = INFINITY, min_y = INFINITY;
		double max_x = -INFINITY, max_y = -INFINITY;
		double center_x, center_y;
		size_t point_index;
		if (xrtValueType(box) == XVALUE_OBJECT) {
			box = xrtValueObjectGet(box, xrtStrView("box"));
		}
		if (xrtValueType(box) != XVALUE_ARRAY || xrtValueCount(box) != 4u ||
			!xrtValueGetString(text, &word) ||
			!rapidocr_json_number(score_value, &score)) goto cleanup;
		for (point_index = 0u; point_index < 4u; ++point_index) {
			xvalue* point = xrtValueArrayGet(box, point_index);
			double x, y;
			if (xrtValueType(point) != XVALUE_ARRAY || xrtValueCount(point) != 2u ||
				!rapidocr_json_number(xrtValueArrayGet(point, 0u), &x) ||
				!rapidocr_json_number(xrtValueArrayGet(point, 1u), &y)) goto cleanup;
			if (x < min_x) min_x = x;
			if (x > max_x) max_x = x;
			if (y < min_y) min_y = y;
			if (y > max_y) max_y = y;
		}
		if (word.Size == 0u || score < min_score) continue;
		center_x = min_x + (max_x - min_x) / 2.0;
		center_y = min_y + (max_y - min_y) / 2.0;
		if (!isfinite(center_x) || !isfinite(center_y) ||
			center_x < INT_MIN || center_x > INT_MAX ||
			center_y < INT_MIN || center_y > INT_MAX ||
			!isfinite(max_x - min_x) || !isfinite(max_y - min_y)) goto cleanup;
		item = xrtValueObject();
		if (item == NULL) goto cleanup;
		if (!xrtValueObjectSetNew(item, xrtStrView("Ratio"), xrtValueFloat(score)) ||
			!xrtValueObjectSetNew(item, xrtStrView("Text"), xrtValueString(word)) ||
			!xrtValueObjectSetNew(item, xrtStrView("cx"), xrtValueInt((int)center_x)) ||
			!xrtValueObjectSetNew(item, xrtStrView("cy"), xrtValueInt((int)center_y)) ||
			!xrtValueObjectSetNew(item, xrtStrView("h"), xrtValueFloat(max_y - min_y)) ||
			!xrtValueObjectSetNew(item, xrtStrView("w"), xrtValueFloat(max_x - min_x)) ||
			!xrtValueObjectSetNew(item, xrtStrView("x"), xrtValueFloat(min_x)) ||
			!xrtValueObjectSetNew(item, xrtStrView("y"), xrtValueFloat(min_y))) {
			xrtValueRelease(item);
			goto cleanup;
		}
		if (!xrtValueArrayAppendNew(items, item)) goto cleanup;
	}
serialize:
	output = xrtJsonStringify(items, false, NULL);
cleanup:
	xrtValueRelease(items);
	xrtValueRelease(root);
	return output;
}

/* 三条检测路径共用同一结果所有权和 JSON 序列化规则。 */
void* rapidocr_boxes_json_encode(const RapidJsonBox* boxes, size_t count)
{
	xvalue* result;
	char* output = NULL;
	size_t i;
	if ((boxes == NULL && count != 0u) || count > RAPIDOCR_MAX_RESULTS) return NULL;
	result = xrtValueArray();
	if (result == NULL) return NULL;
	for (i = 0u; i < count; ++i) {
		xvalue* item;
		xvalue* points;
		size_t j;
		if (!isfinite(boxes[i].score) || boxes[i].pixels < 0) goto cleanup;
		points = xrtValueArray();
		if (points == NULL) goto cleanup;
		for (j = 0u; j < 4u; ++j) {
			xvalue* pair = xrtValueArray();
			if (pair == NULL ||
				!xrtValueArrayAppendNew(pair, xrtValueInt(boxes[i].points[j * 2u])) ||
				!xrtValueArrayAppendNew(pair, xrtValueInt(boxes[i].points[j * 2u + 1u]))) {
				xrtValueRelease(pair);
				xrtValueRelease(points);
				goto cleanup;
			}
			if (!xrtValueArrayAppendNew(points, pair)) {
				xrtValueRelease(points);
				goto cleanup;
			}
		}
		item = xrtValueObject();
		if (item == NULL ||
			!xrtValueObjectSet(item, xrtStrView("box"), points) ||
			!xrtValueObjectSetNew(item, xrtStrView("score"), xrtValueFloat(boxes[i].score)) ||
			!xrtValueObjectSetNew(item, xrtStrView("pixels"), xrtValueInt(boxes[i].pixels))) {
			xrtValueRelease(item);
			xrtValueRelease(points);
			goto cleanup;
		}
		xrtValueRelease(points);
		if (!xrtValueArrayAppendNew(result, item)) goto cleanup;
	}
	output = xrtJsonStringify(result, false, NULL);
cleanup:
	xrtValueRelease(result);
	return output;
}

/* 结果组装只处理 JSON 值，返回值由调用方以 xrtFree 释放。 */
static xvalue* rapidocr_parse_json(const char* text, const char* fallback)
{
	const char* source = text != NULL && text[0] != '\0' ? text : fallback;
	size_t size = strlen(source);
	if (size > RAPIDOCR_MAX_JSON_BYTES) return NULL;
	{
		xvalue* parsed = xrtJsonParse((xstrview){ source, size });
		if (parsed == NULL) xrtClearError();
		return parsed;
	}
}

RAPIDOCR_EXPORT int rapidocr_cls_should_rotate_native(const char* cls_json, double threshold)
{
	xvalue* root = rapidocr_parse_json(cls_json, "{}");
	xvalue* label;
	xvalue* score;
	xstrview name;
	double confidence = 0.0;
	int64 integer;
	int rotate = 0;
	if (root == NULL) {
		xrtClearError();
		return 0;
	}
	if (threshold <= 0.0) threshold = 0.9;
	if (xrtValueType(root) != XVALUE_OBJECT) goto cleanup;
	label = xrtValueObjectGet(root, xrtStrView("label"));
	score = xrtValueObjectGet(root, xrtStrView("score"));
	if (xrtValueType(label) != XVALUE_STRING ||
		!xrtValueGetString(label, &name) || name.Size != 3u ||
		memcmp(name.Data, "180", 3u) != 0) goto cleanup;
	if (xrtValueType(score) == XVALUE_FLOAT) {
		if (!xrtValueGetFloat(score, &confidence)) goto cleanup;
	} else if (xrtValueType(score) == XVALUE_INT) {
		if (!xrtValueGetInt(score, &integer)) goto cleanup;
		confidence = (double)integer;
	} else goto cleanup;
	rotate = confidence > threshold ? 1 : 0;
cleanup:
	xrtValueRelease(root);
	return rotate;
}

static int rapidocr_append_rec(xvalue* texts, xvalue* scores, const xvalue* rec)
{
	xvalue* text;
	xvalue* score;
	xstrview text_view = { "", 0u };
	double score_value = 0.0;
	int64 score_int;
	uint64 score_uint;

	if (xrtValueType(rec) != XVALUE_OBJECT) return 0;
	text = xrtValueObjectGet(rec, xrtStrView("text"));
	score = xrtValueObjectGet(rec, xrtStrView("score"));
	if (text != NULL && !xrtValueGetString(text, &text_view)) return 0;
	if (score != NULL) {
		switch (xrtValueType(score)) {
		case XVALUE_FLOAT:
			if (!xrtValueGetFloat(score, &score_value)) return 0;
			break;
		case XVALUE_INT:
			if (!xrtValueGetInt(score, &score_int)) return 0;
			score_value = (double)score_int;
			break;
		case XVALUE_UINT:
			if (!xrtValueGetUInt(score, &score_uint)) return 0;
			score_value = (double)score_uint;
			break;
		default:
			return 0;
		}
	}
	if (!isfinite(score_value) ||
		!xrtValueArrayAppendNew(texts, xrtValueString(text_view)) ||
		!xrtValueArrayAppendNew(scores, xrtValueFloat(score_value))) return 0;
	return 1;
}

static char* rapidocr_assemble(const char* boxes_json, const xvalue* recs, double elapse)
{
	xvalue* boxes = NULL;
	xvalue* result = NULL;
	xvalue* texts = NULL;
	xvalue* scores = NULL;
	xvalue* word_results = NULL;
	xvalue* elapse_list = NULL;
	char* output = NULL;
	size_t i;

	if (!isfinite(elapse) || xrtValueType(recs) != XVALUE_ARRAY ||
		xrtValueCount(recs) > RAPIDOCR_MAX_RESULTS) return NULL;
	boxes = rapidocr_parse_json(boxes_json, "[]");
	if (xrtValueType(boxes) != XVALUE_ARRAY) goto cleanup;
	result = xrtValueObject();
	texts = xrtValueArray();
	scores = xrtValueArray();
	word_results = xrtValueArray();
	elapse_list = xrtValueArray();
	if (result == NULL || texts == NULL || scores == NULL ||
		word_results == NULL || elapse_list == NULL) goto cleanup;

	for (i = 0; i < xrtValueCount(recs); ++i) {
		if (!rapidocr_append_rec(texts, scores, xrtValueArrayGet(recs, i))) goto cleanup;
	}
	if (!xrtValueObjectSet(result, xrtStrView("boxes"), boxes) ||
		!xrtValueObjectSet(result, xrtStrView("txts"), texts) ||
		!xrtValueObjectSet(result, xrtStrView("scores"), scores) ||
		!xrtValueObjectSet(result, xrtStrView("word_results"), word_results) ||
		!xrtValueObjectSet(result, xrtStrView("elapse_list"), elapse_list) ||
		!xrtValueObjectSetNew(result, xrtStrView("elapse"), xrtValueFloat(elapse))) goto cleanup;
	output = xrtJsonStringify(result, false, NULL);

cleanup:
	xrtValueRelease(elapse_list);
	xrtValueRelease(word_results);
	xrtValueRelease(scores);
	xrtValueRelease(texts);
	xrtValueRelease(result);
	xrtValueRelease(boxes);
	return output;
}

static char* rapidocr_assemble_many(const char* boxes_json,
	const char* const* rec_json, size_t count, double elapse)
{
	xvalue* recs = xrtValueArray();
	char* output = NULL;
	size_t i;
	if (recs == NULL) return NULL;
	for (i = 0; i < count; ++i) {
		xvalue* rec = rapidocr_parse_json(rec_json[i], "{}");
		if (rec == NULL) goto cleanup;
		if (!xrtValueArrayAppendNew(recs, rec)) goto cleanup;
	}
	output = rapidocr_assemble(boxes_json, recs, elapse);
cleanup:
	xrtValueRelease(recs);
	return output;
}

RAPIDOCR_EXPORT char* rapidocr_assemble_result_native(const char* boxes_json,
	const char* rec_json, double elapse)
{
	const char* recs[] = { rec_json };
	return rapidocr_assemble_many(boxes_json, recs, 1u, elapse);
}

RAPIDOCR_EXPORT char* rapidocr_assemble_result2_native(const char* boxes_json,
	const char* rec0_json, const char* rec1_json, double elapse)
{
	const char* recs[] = { rec0_json, rec1_json };
	return rapidocr_assemble_many(boxes_json, recs, 2u, elapse);
}

RAPIDOCR_EXPORT char* rapidocr_assemble_result4_native(const char* boxes_json,
	const char* rec0_json, const char* rec1_json, const char* rec2_json,
	const char* rec3_json, double elapse)
{
	const char* recs[] = { rec0_json, rec1_json, rec2_json, rec3_json };
	return rapidocr_assemble_many(boxes_json, recs, 4u, elapse);
}

RAPIDOCR_EXPORT char* rapidocr_assemble_result_list_native(const char* boxes_json,
	const char* recs_json, double elapse)
{
	xvalue* recs = rapidocr_parse_json(recs_json, "[]");
	char* output = recs != NULL ? rapidocr_assemble(boxes_json, recs, elapse) : NULL;
	xrtValueRelease(recs);
	return output;
}
