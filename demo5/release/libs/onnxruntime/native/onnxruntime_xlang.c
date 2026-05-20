#include "../include/onnxruntime_xlang_c_api.h"
#include "../include/onnxruntime_c_api.h"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

#if defined(_WIN32) || defined(_WIN64)
#define XLORT_EXPORT __declspec(dllexport)
#else
#define XLORT_EXPORT
#endif

typedef struct XlOrtSessionOptionsImpl {
	int intra_threads;
	int inter_threads;
	int graph_optimization_level;
	int execution_mode;
	int profiling_enabled;
	int profiling_disabled;
	int cpu_mem_arena; /* -1 unchanged, 0 disabled, 1 enabled */
	int mem_pattern;   /* -1 unchanged, 0 disabled, 1 enabled */
	int log_verbosity_level;
	int log_severity_level;
	char profiling_prefix[512];
	char log_id[128];
	char provider_names[8][128];
	char provider_options_json[8][1024];
	size_t provider_count;
} XlOrtSessionOptionsImpl;

typedef struct XlOrtSessionImpl {
	OrtEnv* env;
	OrtSessionOptions* options;
	OrtSession* session;
	size_t input_count;
	size_t output_count;
	char input_names[16][128];
	char output_names[16][128];
	char inputs_json[2048];
	char outputs_json[2048];
	char modelmeta_json[4096];
	char profiling_file[1024];
} XlOrtSessionImpl;

typedef struct XlOrtResultImpl {
	char json[65536];
	int64_t float_shape[8];
	size_t float_shape_count;
	size_t float_count;
	float* float_data;
	char float_shape_json[128];
} XlOrtResultImpl;

typedef struct XlOrtTensorImpl {
	int64_t shape[8];
	size_t shape_count;
	size_t data_count;
	float* data;
} XlOrtTensorImpl;

typedef struct XlOrtInputTensor {
	char name[128];
	int64_t shape[8];
	size_t shape_count;
	void* data;
	char** strings;
	size_t data_count;
	size_t data_bytes;
	ONNXTensorElementDataType elem_type;
	OrtValue* value;
} XlOrtInputTensor;

typedef struct XlOrtIoBindingImpl {
	XlOrtSessionImpl* sess;
	OrtIoBinding* binding;
	XlOrtInputTensor inputs[16];
	size_t input_count;
	char output_names[16][128];
	size_t output_count;
	char outputs_json[65536];
} XlOrtIoBindingImpl;

static const OrtApi* g_api = NULL;
static const OrtApiBase* g_api_base = NULL;
static char g_error[1024];
static char g_providers_json[1024];

static const char* xlort_find_json_key(const char* json, const char* key);

static void xlort_set_error(const char* text)
{
	if ( text == NULL ) {
		text = "";
	}
	strncpy(g_error, text, sizeof(g_error) - 1u);
	g_error[sizeof(g_error) - 1u] = '\0';
}

static int xlort_init(void)
{
	const OrtApiBase* base;
	if ( g_api != NULL ) {
		return 1;
	}
	base = OrtGetApiBase();
	if ( base == NULL ) {
		xlort_set_error("OrtGetApiBase returned NULL");
		return 0;
	}
	g_api_base = base;
	g_api = base->GetApi(ORT_API_VERSION);
	if ( g_api == NULL ) {
		xlort_set_error("GetApi returned NULL");
		return 0;
	}
	xlort_set_error("");
	return 1;
}

static int xlort_status_ok(OrtStatus* status, const char* op)
{
	const char* msg;
	if ( status == NULL ) {
		return 1;
	}
	msg = (g_api != NULL) ? g_api->GetErrorMessage(status) : NULL;
	snprintf(g_error, sizeof(g_error), "%s failed%s%s", op != NULL ? op : "onnxruntime", msg != NULL ? ": " : "", msg != NULL ? msg : "");
	if ( g_api != NULL ) {
		g_api->ReleaseStatus(status);
	}
	return 0;
}

static void xlort_json_escape_append(char* out, size_t out_size, size_t* used, const char* text)
{
	const char* p;
	if ( out == NULL || out_size == 0u || used == NULL || text == NULL ) {
		return;
	}
	for ( p = text; *p != '\0' && *used + 2u < out_size; ++p ) {
		if ( *p == '"' || *p == '\\' ) {
			out[(*used)++] = '\\';
		}
		out[(*used)++] = *p;
	}
	out[*used] = '\0';
}

static void xlort_append(char* out, size_t out_size, size_t* used, const char* text)
{
	size_t len;
	if ( out == NULL || used == NULL || text == NULL || *used >= out_size ) {
		return;
	}
	len = strlen(text);
	if ( len >= out_size - *used ) {
		len = out_size - *used - 1u;
	}
	memcpy(out + *used, text, len);
	*used += len;
	out[*used] = '\0';
}

static void xlort_appendf(char* out, size_t out_size, size_t* used, const char* fmt, ...)
{
	va_list args;
	int n;
	if ( out == NULL || used == NULL || fmt == NULL || *used >= out_size ) {
		return;
	}
	va_start(args, fmt);
	n = vsnprintf(out + *used, out_size - *used, fmt, args);
	va_end(args);
	if ( n <= 0 ) {
		return;
	}
	if ( (size_t)n >= out_size - *used ) {
		*used = out_size - 1u;
	} else {
		*used += (size_t)n;
	}
	out[*used] = '\0';
}

static const char* xlort_skip_ws(const char* p)
{
	while ( p != NULL && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ) {
		++p;
	}
	return p;
}

static const char* xlort_dtype_name(ONNXTensorElementDataType type)
{
	switch ( type ) {
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT: return "float32";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE: return "float64";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64: return "int64";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32: return "int32";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16: return "int16";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8: return "int8";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64: return "uint64";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32: return "uint32";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16: return "uint16";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8: return "uint8";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL: return "bool";
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING: return "string";
	default: return "unknown";
	}
}

static size_t xlort_dtype_size(ONNXTensorElementDataType type)
{
	switch ( type ) {
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT: return sizeof(float);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE: return sizeof(double);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64: return sizeof(int64_t);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32: return sizeof(int32_t);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16: return sizeof(int16_t);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8: return sizeof(int8_t);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64: return sizeof(uint64_t);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32: return sizeof(uint32_t);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16: return sizeof(uint16_t);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8: return sizeof(uint8_t);
	case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL: return sizeof(uint8_t);
	default: return 0u;
	}
}

static int xlort_parse_dtype(const char* body, ONNXTensorElementDataType* out)
{
	const char* p;
	char dtype[32];
	size_t used = 0;
	if ( out == NULL ) return 0;
	*out = ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED;
	p = xlort_find_json_key(body, "dtype");
	if ( p == NULL ) {
		xlort_set_error("input tensor is missing dtype");
		return 0;
	}
	p = strchr(p, ':');
	if ( p == NULL ) return 0;
	p = xlort_skip_ws(p + 1);
	if ( *p != '"' ) {
		xlort_set_error("input tensor dtype must be a string");
		return 0;
	}
	++p;
	while ( *p != '\0' && *p != '"' && used + 1u < sizeof(dtype) ) {
		dtype[used++] = *p++;
	}
	dtype[used] = '\0';
	if ( strcmp(dtype, "float32") == 0 || strcmp(dtype, "float") == 0 ) *out = ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT;
	else if ( strcmp(dtype, "float64") == 0 || strcmp(dtype, "double") == 0 ) *out = ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE;
	else if ( strcmp(dtype, "int64") == 0 ) *out = ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64;
	else if ( strcmp(dtype, "int32") == 0 || strcmp(dtype, "int") == 0 ) *out = ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32;
	else if ( strcmp(dtype, "int16") == 0 ) *out = ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16;
	else if ( strcmp(dtype, "int8") == 0 ) *out = ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8;
	else if ( strcmp(dtype, "uint64") == 0 ) *out = ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64;
	else if ( strcmp(dtype, "uint32") == 0 ) *out = ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32;
	else if ( strcmp(dtype, "uint16") == 0 ) *out = ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16;
	else if ( strcmp(dtype, "uint8") == 0 ) *out = ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8;
	else if ( strcmp(dtype, "bool") == 0 || strcmp(dtype, "boolean") == 0 ) *out = ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL;
	else if ( strcmp(dtype, "string") == 0 || strcmp(dtype, "str") == 0 ) *out = ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING;
	else {
		snprintf(g_error, sizeof(g_error), "unsupported input dtype '%s'", dtype);
		return 0;
	}
	return 1;
}

static const char* xlort_find_json_key(const char* json, const char* key)
{
	char pattern[160];
	if ( json == NULL || key == NULL ) {
		return NULL;
	}
	snprintf(pattern, sizeof(pattern), "\"%s\"", key);
	return strstr(json, pattern);
}

static int xlort_parse_number_array_double(const char* start, double* out, size_t cap, size_t* count)
{
	const char* p;
	char* endp;
	size_t n;
	if ( count != NULL ) *count = 0;
	if ( start == NULL || out == NULL || count == NULL ) {
		return 0;
	}
	p = strchr(start, '[');
	if ( p == NULL ) return 0;
	++p;
	n = 0;
	for (;;) {
		double v;
		p = xlort_skip_ws(p);
		if ( *p == ']' ) {
			*count = n;
			return 1;
		}
		if ( n >= cap ) {
			xlort_set_error("JSON numeric array is too large");
			return 0;
		}
		v = strtod(p, &endp);
		if ( endp == p ) {
			xlort_set_error("failed to parse JSON numeric array");
			return 0;
		}
		out[n++] = v;
		p = xlort_skip_ws(endp);
		if ( *p == ',' ) {
			++p;
			continue;
		}
		if ( *p == ']' ) {
			*count = n;
			return 1;
		}
		xlort_set_error("malformed JSON numeric array");
		return 0;
	}
}

static char* xlort_dup_range(const char* start, size_t len)
{
	char* out = (char*)malloc(len + 1u);
	if ( out == NULL ) return NULL;
	memcpy(out, start, len);
	out[len] = '\0';
	return out;
}

static int xlort_parse_json_string_array(const char* start, char*** out_values, size_t cap, size_t* count)
{
	const char* p;
	char** values;
	size_t n = 0;
	if ( count != NULL ) *count = 0;
	if ( out_values != NULL ) *out_values = NULL;
	if ( start == NULL || out_values == NULL || count == NULL ) return 0;
	p = strchr(start, '[');
	if ( p == NULL ) return 0;
	values = (char**)calloc(cap > 0 ? cap : 1u, sizeof(char*));
	if ( values == NULL ) {
		xlort_set_error("out of memory");
		return 0;
	}
	++p;
	for (;;) {
		char tmp[4096];
		size_t used = 0;
		p = xlort_skip_ws(p);
		if ( *p == ']' ) {
			*out_values = values;
			*count = n;
			return 1;
		}
		if ( n >= cap ) {
			xlort_set_error("JSON string array is too large");
			goto fail;
		}
		if ( *p != '"' ) {
			xlort_set_error("JSON string array contains a non-string item");
			goto fail;
		}
		++p;
		while ( *p != '\0' && *p != '"' && used + 1u < sizeof(tmp) ) {
			if ( *p == '\\' && p[1] != '\0' ) {
				++p;
				switch ( *p ) {
				case 'n': tmp[used++] = '\n'; break;
				case 'r': tmp[used++] = '\r'; break;
				case 't': tmp[used++] = '\t'; break;
				default: tmp[used++] = *p; break;
				}
				++p;
			} else {
				tmp[used++] = *p++;
			}
		}
		tmp[used] = '\0';
		if ( *p != '"' ) {
			xlort_set_error("unterminated string in JSON string array");
			goto fail;
		}
		values[n] = xlort_dup_range(tmp, used);
		if ( values[n] == NULL ) {
			xlort_set_error("out of memory");
			goto fail;
		}
		++n;
		++p;
		p = xlort_skip_ws(p);
		if ( *p == ',' ) {
			++p;
			continue;
		}
		if ( *p == ']' ) {
			*out_values = values;
			*count = n;
			return 1;
		}
		xlort_set_error("malformed JSON string array");
		goto fail;
	}
fail:
	while ( n > 0 ) free(values[--n]);
	free(values);
	return 0;
}

static int xlort_parse_json_map_entries(const char* json, char keys[][128], char values[][256], size_t cap, size_t* count)
{
	const char* p;
	size_t n = 0;
	if ( count != NULL ) *count = 0;
	if ( json == NULL || keys == NULL || values == NULL || count == NULL ) return 0;
	p = xlort_skip_ws(json);
	if ( *p == '\0' || strcmp(p, "{}") == 0 ) {
		*count = 0;
		return 1;
	}
	p = strchr(p, '{');
	if ( p == NULL ) {
		xlort_set_error("provider options must be a JSON object");
		return 0;
	}
	++p;
	for (;;) {
		char key[128];
		char value[256];
		size_t used = 0;
		p = xlort_skip_ws(p);
		if ( *p == '}' ) {
			*count = n;
			return 1;
		}
		if ( n >= cap ) {
			xlort_set_error("too many provider option entries");
			return 0;
		}
		if ( *p != '"' ) {
			xlort_set_error("provider option key must be a string");
			return 0;
		}
		++p;
		while ( *p != '\0' && *p != '"' && used + 1u < sizeof(key) ) {
			if ( *p == '\\' && p[1] != '\0' ) ++p;
			key[used++] = *p++;
		}
		key[used] = '\0';
		if ( *p != '"' ) {
			xlort_set_error("unterminated provider option key");
			return 0;
		}
		++p;
		p = xlort_skip_ws(p);
		if ( *p != ':' ) {
			xlort_set_error("provider option entry is missing ':'");
			return 0;
		}
		p = xlort_skip_ws(p + 1);
		used = 0;
		if ( *p == '"' ) {
			++p;
			while ( *p != '\0' && *p != '"' && used + 1u < sizeof(value) ) {
				if ( *p == '\\' && p[1] != '\0' ) ++p;
				value[used++] = *p++;
			}
			if ( *p != '"' ) {
				xlort_set_error("unterminated provider option value");
				return 0;
			}
			++p;
		} else {
			while ( *p != '\0' && *p != ',' && *p != '}' && used + 1u < sizeof(value) ) {
				if ( *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' ) {
					value[used++] = *p;
				}
				++p;
			}
		}
		value[used] = '\0';
		{
			size_t key_len = strlen(key);
			size_t value_len = strlen(value);
			if ( key_len > 127u ) key_len = 127u;
			if ( value_len > 255u ) value_len = 255u;
			memcpy(keys[n], key, key_len);
			keys[n][key_len] = '\0';
			memcpy(values[n], value, value_len);
			values[n][value_len] = '\0';
		}
		++n;
		p = xlort_skip_ws(p);
		if ( *p == ',' ) {
			++p;
			continue;
		}
		if ( *p == '}' ) {
			*count = n;
			return 1;
		}
		xlort_set_error("malformed provider options JSON");
		return 0;
	}
}

static int xlort_parse_input_tensor_body(const char* body, const char* name, XlOrtInputTensor* tensor)
{
	const char* shape_key;
	const char* data_key;
	double shape_values[8];
	double data_values[65536];
	size_t shape_count;
	size_t data_count;
	size_t i;
	size_t elem_size;

	if ( body == NULL || name == NULL || tensor == NULL ) {
		return 0;
	}
	memset(tensor, 0, sizeof(*tensor));
	strncpy(tensor->name, name, sizeof(tensor->name) - 1u);
	if ( !xlort_parse_dtype(body, &tensor->elem_type) ) {
		return 0;
	}
	elem_size = xlort_dtype_size(tensor->elem_type);
	if ( elem_size == 0u && tensor->elem_type != ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING ) {
		snprintf(g_error, sizeof(g_error), "input dtype '%s' is not supported by run_json", xlort_dtype_name(tensor->elem_type));
		return 0;
	}
	shape_key = xlort_find_json_key(body, "shape");
	data_key = xlort_find_json_key(body, "data");
	if ( !xlort_parse_number_array_double(shape_key, shape_values, 8, &shape_count) ) {
		return 0;
	}
	tensor->shape_count = shape_count;
	for ( i = 0; i < shape_count; ++i ) {
		tensor->shape[i] = (int64_t)shape_values[i];
	}
	if ( tensor->elem_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING ) {
		if ( !xlort_parse_json_string_array(data_key, &tensor->strings, 65536, &data_count) ) {
			return 0;
		}
		tensor->data_count = data_count;
		return 1;
	}
	if ( !xlort_parse_number_array_double(data_key, data_values, 65536, &data_count) ) {
		return 0;
	}
	tensor->data_count = data_count;
	tensor->data_bytes = data_count * elem_size;
	tensor->data = calloc(data_count > 0 ? data_count : 1u, elem_size);
	if ( tensor->data == NULL ) {
		xlort_set_error("out of memory");
		return 0;
	}
	for ( i = 0; i < data_count; ++i ) {
		switch ( tensor->elem_type ) {
		case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT: ((float*)tensor->data)[i] = (float)data_values[i]; break;
		case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE: ((double*)tensor->data)[i] = data_values[i]; break;
		case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64: ((int64_t*)tensor->data)[i] = (int64_t)data_values[i]; break;
		case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32: ((int32_t*)tensor->data)[i] = (int32_t)data_values[i]; break;
		case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16: ((int16_t*)tensor->data)[i] = (int16_t)data_values[i]; break;
		case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8: ((int8_t*)tensor->data)[i] = (int8_t)data_values[i]; break;
		case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64: ((uint64_t*)tensor->data)[i] = (uint64_t)data_values[i]; break;
		case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32: ((uint32_t*)tensor->data)[i] = (uint32_t)data_values[i]; break;
		case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16: ((uint16_t*)tensor->data)[i] = (uint16_t)data_values[i]; break;
		case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8: ((uint8_t*)tensor->data)[i] = (uint8_t)data_values[i]; break;
		case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL: ((uint8_t*)tensor->data)[i] = data_values[i] != 0.0 ? 1u : 0u; break;
		default: break;
		}
	}
	return 1;
}

static int xlort_parse_input_tensor(const char* json, const char* name, XlOrtInputTensor* tensor)
{
	const char* key;
	const char* body;

	if ( json == NULL || name == NULL || tensor == NULL ) {
		return 0;
	}
	key = xlort_find_json_key(json, name);
	if ( key == NULL ) {
		snprintf(g_error, sizeof(g_error), "missing input '%s' in feed JSON", name);
		return 0;
	}
	body = strchr(key, '{');
	if ( body == NULL ) {
		xlort_set_error("input feed item must be an object");
		return 0;
	}
	return xlort_parse_input_tensor_body(body, name, tensor);
}

static int xlort_parse_string_array(const char* json, const char names[][128], size_t name_count, const char** selected, size_t* selected_count)
{
	const char* p;
	size_t count = 0;
	if ( selected_count != NULL ) *selected_count = 0;
	if ( selected == NULL || selected_count == NULL ) return 0;
	if ( json == NULL || json[0] == '\0' || strcmp(json, "[]") == 0 ) {
		size_t i;
		for ( i = 0; i < name_count; ++i ) selected[count++] = names[i];
		*selected_count = count;
		return 1;
	}
	p = strchr(json, '[');
	if ( p == NULL ) {
		xlort_set_error("output_names_json must be a JSON string array");
		return 0;
	}
	++p;
	for (;;) {
		char name[128];
		size_t used = 0;
		size_t i;
		p = xlort_skip_ws(p);
		if ( *p == ']' ) {
			*selected_count = count;
			return 1;
		}
		if ( *p != '"' ) {
			xlort_set_error("output_names_json contains a non-string item");
			return 0;
		}
		++p;
		while ( *p != '\0' && *p != '"' && used + 1u < sizeof(name) ) {
			if ( *p == '\\' && p[1] != '\0' ) ++p;
			name[used++] = *p++;
		}
		name[used] = '\0';
		if ( *p != '"' ) {
			xlort_set_error("unterminated string in output_names_json");
			return 0;
		}
		++p;
		for ( i = 0; i < name_count; ++i ) {
			if ( strcmp(names[i], name) == 0 ) {
				if ( count >= 16u ) {
					xlort_set_error("too many output names");
					return 0;
				}
				selected[count++] = names[i];
				break;
			}
		}
		if ( i == name_count ) {
			snprintf(g_error, sizeof(g_error), "unknown output name '%s'", name);
			return 0;
		}
		p = xlort_skip_ws(p);
		if ( *p == ',' ) {
			++p;
			continue;
		}
		if ( *p == ']' ) {
			*selected_count = count;
			return 1;
		}
		xlort_set_error("malformed output_names_json");
		return 0;
	}
}

static void xlort_free_input_tensors(XlOrtInputTensor* tensors, size_t count)
{
	size_t i;
	if ( tensors == NULL ) return;
	for ( i = 0; i < count; ++i ) {
		if ( tensors[i].value != NULL && g_api != NULL ) {
			g_api->ReleaseValue(tensors[i].value);
		}
		if ( tensors[i].strings != NULL ) {
			size_t j;
			for ( j = 0; j < tensors[i].data_count; ++j ) {
				free(tensors[i].strings[j]);
			}
			free(tensors[i].strings);
		}
		free(tensors[i].data);
	}
}

static void xlort_append_tensor_description_json(char* out, size_t out_size, size_t* used, const char* name, const OrtTensorTypeAndShapeInfo* info)
{
	ONNXTensorElementDataType elem_type = ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED;
	size_t dim_count = 0;
	int64_t dims[16];
	const char* dim_params[16];
	size_t i;

	xlort_append(out, out_size, used, "{\"name\":\"");
	xlort_json_escape_append(out, out_size, used, name != NULL ? name : "");
	xlort_append(out, out_size, used, "\",\"type\":\"tensor\",\"dtype\":\"");
	if ( info != NULL &&
		xlort_status_ok(g_api->GetTensorElementType(info, &elem_type), "GetTensorElementType") ) {
		xlort_json_escape_append(out, out_size, used, xlort_dtype_name(elem_type));
	} else {
		xlort_json_escape_append(out, out_size, used, "unknown");
	}
	xlort_append(out, out_size, used, "\",\"shape\":[");
	if ( info != NULL &&
		xlort_status_ok(g_api->GetDimensionsCount(info, &dim_count), "GetDimensionsCount") &&
		dim_count <= 16u &&
		xlort_status_ok(g_api->GetDimensions(info, dims, dim_count), "GetDimensions") ) {
		for ( i = 0; i < dim_count; ++i ) {
			if ( i > 0 ) xlort_append(out, out_size, used, ",");
			xlort_appendf(out, out_size, used, "%lld", (long long)dims[i]);
		}
	}
	xlort_append(out, out_size, used, "],\"shape_symbols\":[");
	if ( info != NULL && dim_count > 0u && dim_count <= 16u ) {
		memset(dim_params, 0, sizeof(dim_params));
		if ( xlort_status_ok(g_api->GetSymbolicDimensions(info, dim_params, dim_count), "GetSymbolicDimensions") ) {
			for ( i = 0; i < dim_count; ++i ) {
				if ( i > 0 ) xlort_append(out, out_size, used, ",");
				xlort_append(out, out_size, used, "\"");
				xlort_json_escape_append(out, out_size, used, dim_params[i] != NULL ? dim_params[i] : "");
				xlort_append(out, out_size, used, "\"");
			}
		}
	}
	xlort_append(out, out_size, used, "]}");
}

static void xlort_describe_one_io(XlOrtSessionImpl* sess, int is_input, size_t index, OrtAllocator* allocator, char* out, size_t out_size, size_t* used)
{
	char* name = NULL;
	OrtTypeInfo* type_info = NULL;
	const OrtTensorTypeAndShapeInfo* tensor_info = NULL;

	if ( is_input ) {
		if ( !xlort_status_ok(g_api->SessionGetInputName(sess->session, index, allocator, &name), "SessionGetInputName") ) return;
		if ( index < 16u ) strncpy(sess->input_names[index], name != NULL ? name : "", sizeof(sess->input_names[index]) - 1u);
		(void)xlort_status_ok(g_api->SessionGetInputTypeInfo(sess->session, index, &type_info), "SessionGetInputTypeInfo");
	} else {
		if ( !xlort_status_ok(g_api->SessionGetOutputName(sess->session, index, allocator, &name), "SessionGetOutputName") ) return;
		if ( index < 16u ) strncpy(sess->output_names[index], name != NULL ? name : "", sizeof(sess->output_names[index]) - 1u);
		(void)xlort_status_ok(g_api->SessionGetOutputTypeInfo(sess->session, index, &type_info), "SessionGetOutputTypeInfo");
	}
	if ( type_info != NULL ) {
		(void)xlort_status_ok(g_api->CastTypeInfoToTensorInfo(type_info, &tensor_info), "CastTypeInfoToTensorInfo");
	}
	xlort_append_tensor_description_json(out, out_size, used, name != NULL ? name : "", tensor_info);
	if ( type_info != NULL ) {
		g_api->ReleaseTypeInfo(type_info);
	}
	if ( name != NULL ) allocator->Free(allocator, name);
}

static void xlort_describe_names(XlOrtSessionImpl* sess)
{
	size_t count = 0;
	size_t i;
	OrtAllocator* allocator = NULL;
	size_t used;

	strncpy(sess->inputs_json, "[]", sizeof(sess->inputs_json));
	strncpy(sess->outputs_json, "[]", sizeof(sess->outputs_json));

	if ( !xlort_status_ok(g_api->GetAllocatorWithDefaultOptions(&allocator), "GetAllocatorWithDefaultOptions") ) {
		return;
	}

	if ( !xlort_status_ok(g_api->SessionGetInputCount(sess->session, &count), "SessionGetInputCount") ) {
		return;
	}
	sess->input_count = count < 16u ? count : 16u;
	used = 0;
	sess->inputs_json[used++] = '[';
	for ( i = 0; i < count && used + 8u < sizeof(sess->inputs_json); ++i ) {
		if ( i > 0 ) sess->inputs_json[used++] = ',';
		xlort_describe_one_io(sess, 1, i, allocator, sess->inputs_json, sizeof(sess->inputs_json), &used);
	}
	if ( used + 2u < sizeof(sess->inputs_json) ) {
		sess->inputs_json[used++] = ']';
		sess->inputs_json[used] = '\0';
	}

	if ( !xlort_status_ok(g_api->SessionGetOutputCount(sess->session, &count), "SessionGetOutputCount") ) {
		return;
	}
	sess->output_count = count < 16u ? count : 16u;
	used = 0;
	sess->outputs_json[used++] = '[';
	for ( i = 0; i < count && used + 8u < sizeof(sess->outputs_json); ++i ) {
		if ( i > 0 ) sess->outputs_json[used++] = ',';
		xlort_describe_one_io(sess, 0, i, allocator, sess->outputs_json, sizeof(sess->outputs_json), &used);
	}
	if ( used + 2u < sizeof(sess->outputs_json) ) {
		sess->outputs_json[used++] = ']';
		sess->outputs_json[used] = '\0';
	}
}

static void xlort_append_tensor_data_json(char* out, size_t out_size, size_t* used, ONNXTensorElementDataType elem_type, const int64_t* dims, size_t dim_count, const void* data, size_t elem_count)
{
	size_t j;
	xlort_append(out, out_size, used, "\"dtype\":\"");
	xlort_json_escape_append(out, out_size, used, xlort_dtype_name(elem_type));
	xlort_append(out, out_size, used, "\",\"shape\":[");
	for ( j = 0; j < dim_count; ++j ) {
		if ( j > 0 ) xlort_append(out, out_size, used, ",");
		xlort_appendf(out, out_size, used, "%lld", (long long)dims[j]);
	}
	xlort_append(out, out_size, used, "],\"data\":[");
	for ( j = 0; j < elem_count; ++j ) {
		if ( j > 0 ) xlort_append(out, out_size, used, ",");
		switch ( elem_type ) {
		case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT:
			xlort_appendf(out, out_size, used, "%.9g", (double)((const float*)data)[j]);
			break;
		case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE:
			xlort_appendf(out, out_size, used, "%.17g", ((const double*)data)[j]);
			break;
		case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64:
			xlort_appendf(out, out_size, used, "%lld", (long long)((const int64_t*)data)[j]);
			break;
		case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32:
			xlort_appendf(out, out_size, used, "%d", (int)((const int32_t*)data)[j]);
			break;
		case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16:
			xlort_appendf(out, out_size, used, "%d", (int)((const int16_t*)data)[j]);
			break;
		case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8:
			xlort_appendf(out, out_size, used, "%d", (int)((const int8_t*)data)[j]);
			break;
		case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64:
			xlort_appendf(out, out_size, used, "%llu", (unsigned long long)((const uint64_t*)data)[j]);
			break;
		case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32:
			xlort_appendf(out, out_size, used, "%u", (unsigned int)((const uint32_t*)data)[j]);
			break;
		case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16:
			xlort_appendf(out, out_size, used, "%u", (unsigned int)((const uint16_t*)data)[j]);
			break;
		case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8:
			xlort_appendf(out, out_size, used, "%u", (unsigned int)((const uint8_t*)data)[j]);
			break;
		case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL:
			xlort_append(out, out_size, used, ((const uint8_t*)data)[j] ? "true" : "false");
			break;
		default:
			xlort_append(out, out_size, used, "null");
			break;
		}
	}
	xlort_append(out, out_size, used, "]");
}

static void xlort_capture_first_float_output(XlOrtResultImpl* result, const int64_t* dims, size_t dim_count, const void* data, size_t elem_count)
{
	size_t i;
	size_t used = 0;
	if ( result == NULL || result->float_data != NULL || dims == NULL || data == NULL || dim_count == 0u || dim_count > 8u || elem_count == 0u ) {
		return;
	}
	result->float_data = (float*)malloc(sizeof(float) * elem_count);
	if ( result->float_data == NULL ) {
		return;
	}
	memcpy(result->float_data, data, sizeof(float) * elem_count);
	result->float_count = elem_count;
	result->float_shape_count = dim_count;
	xlort_append(result->float_shape_json, sizeof(result->float_shape_json), &used, "[");
	for ( i = 0; i < dim_count; ++i ) {
		result->float_shape[i] = dims[i];
		if ( i > 0 ) xlort_append(result->float_shape_json, sizeof(result->float_shape_json), &used, ",");
		xlort_appendf(result->float_shape_json, sizeof(result->float_shape_json), &used, "%lld", (long long)dims[i]);
	}
	xlort_append(result->float_shape_json, sizeof(result->float_shape_json), &used, "]");
}

static int xlort_append_string_tensor_json(char* out, size_t out_size, size_t* used, OrtValue* value, const int64_t* dims, size_t dim_count, size_t elem_count)
{
	size_t data_len = 0;
	char* buffer = NULL;
	size_t* offsets = NULL;
	size_t i;
	if ( !xlort_status_ok(g_api->GetStringTensorDataLength(value, &data_len), "GetStringTensorDataLength") ) {
		return 0;
	}
	buffer = (char*)calloc(data_len > 0 ? data_len : 1u, 1u);
	offsets = (size_t*)calloc(elem_count > 0 ? elem_count : 1u, sizeof(size_t));
	if ( buffer == NULL || offsets == NULL ) {
		free(buffer);
		free(offsets);
		xlort_set_error("out of memory");
		return 0;
	}
	if ( !xlort_status_ok(g_api->GetStringTensorContent(value, buffer, data_len, offsets, elem_count), "GetStringTensorContent") ) {
		free(buffer);
		free(offsets);
		return 0;
	}
	xlort_append(out, out_size, used, "\"dtype\":\"string\",\"shape\":[");
	for ( i = 0; i < dim_count; ++i ) {
		if ( i > 0 ) xlort_append(out, out_size, used, ",");
		xlort_appendf(out, out_size, used, "%lld", (long long)dims[i]);
	}
	xlort_append(out, out_size, used, "],\"data\":[");
	for ( i = 0; i < elem_count; ++i ) {
		size_t start = offsets[i];
		size_t end = (i + 1u < elem_count) ? offsets[i + 1u] : data_len;
		if ( i > 0 ) xlort_append(out, out_size, used, ",");
		xlort_append(out, out_size, used, "\"");
		if ( start <= end && end <= data_len ) {
			char* item = xlort_dup_range(buffer + start, end - start);
			if ( item != NULL ) {
				xlort_json_escape_append(out, out_size, used, item);
				free(item);
			}
		}
		xlort_append(out, out_size, used, "\"");
	}
	xlort_append(out, out_size, used, "]");
	free(buffer);
	free(offsets);
	return 1;
}

static void xlort_append_ort_value_json(char* out, size_t out_size, size_t* used, const char* output_name, OrtValue* value)
{
	OrtTensorTypeAndShapeInfo* info = NULL;
	ONNXTensorElementDataType elem_type = ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED;
	size_t dim_count = 0;
	size_t elem_count = 0;
	int64_t dims[8];
	void* data = NULL;

	xlort_append(out, out_size, used, "\"");
	xlort_json_escape_append(out, out_size, used, output_name != NULL ? output_name : "");
	xlort_append(out, out_size, used, "\":{");
	if ( value == NULL ||
		!xlort_status_ok(g_api->GetTensorTypeAndShape(value, &info), "GetTensorTypeAndShape") ||
		!xlort_status_ok(g_api->GetTensorElementType(info, &elem_type), "GetTensorElementType") ||
		!xlort_status_ok(g_api->GetDimensionsCount(info, &dim_count), "GetDimensionsCount") ||
		dim_count > 8u ||
		!xlort_status_ok(g_api->GetDimensions(info, dims, dim_count), "GetDimensions") ||
		!xlort_status_ok(g_api->GetTensorShapeElementCount(info, &elem_count), "GetTensorShapeElementCount") ) {
		xlort_append(out, out_size, used, "\"error\":\"failed to read output tensor\"");
		if ( info != NULL ) g_api->ReleaseTensorTypeAndShapeInfo(info);
		xlort_append(out, out_size, used, "}");
		return;
	}
	if ( elem_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING ) {
		if ( !xlort_append_string_tensor_json(out, out_size, used, value, dims, dim_count, elem_count) ) {
			xlort_append(out, out_size, used, "\"error\":\"failed to read string output tensor\"");
		}
	} else if ( xlort_dtype_size(elem_type) != 0u &&
		xlort_status_ok(g_api->GetTensorMutableData(value, &data), "GetTensorMutableData") ) {
		xlort_append_tensor_data_json(out, out_size, used, elem_type, dims, dim_count, data, elem_count);
	} else {
		xlort_appendf(out, out_size, used, "\"error\":\"unsupported output dtype %d\"", (int)elem_type);
	}
	xlort_append(out, out_size, used, "}");
	if ( info != NULL ) g_api->ReleaseTensorTypeAndShapeInfo(info);
}

static int xlort_create_ort_value_from_tensor(XlOrtInputTensor* tensor, OrtMemoryInfo* memory_info)
{
	OrtStatus* status;
	if ( tensor == NULL ) return 0;
	if ( tensor->elem_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING ) {
		OrtAllocator* allocator = NULL;
		if ( !xlort_status_ok(g_api->GetAllocatorWithDefaultOptions(&allocator), "GetAllocatorWithDefaultOptions") ) {
			return 0;
		}
		status = g_api->CreateTensorAsOrtValue(
			allocator,
			tensor->shape,
			tensor->shape_count,
			ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING,
			&tensor->value
		);
		return xlort_status_ok(status, "CreateTensorAsOrtValue") &&
			xlort_status_ok(g_api->FillStringTensor(tensor->value, (const char* const*)tensor->strings, tensor->data_count), "FillStringTensor");
	}
	status = g_api->CreateTensorWithDataAsOrtValue(
		memory_info,
		tensor->data,
		tensor->data_bytes,
		tensor->shape,
		tensor->shape_count,
		tensor->elem_type,
		&tensor->value
	);
	return xlort_status_ok(status, "CreateTensorWithDataAsOrtValue");
}

XLORT_EXPORT int onnxruntime_xl_available(void)
{
	return xlort_init();
}

XLORT_EXPORT const char* onnxruntime_xl_get_version_string(void)
{
	if ( !xlort_init() ) {
		return "";
	}
	return g_api_base != NULL ? g_api_base->GetVersionString() : "";
}

XLORT_EXPORT const char* onnxruntime_xl_get_available_providers_json(void)
{
	char** providers = NULL;
	int provider_count = 0;
	int i;
	size_t used = 0;
	if ( !xlort_init() ) {
		return "[]";
	}
	g_providers_json[0] = '\0';
	if ( !xlort_status_ok(g_api->GetAvailableProviders(&providers, &provider_count), "GetAvailableProviders") ) {
		return "[]";
	}
	xlort_append(g_providers_json, sizeof(g_providers_json), &used, "[");
	for ( i = 0; i < provider_count; ++i ) {
		if ( i > 0 ) xlort_append(g_providers_json, sizeof(g_providers_json), &used, ",");
		xlort_append(g_providers_json, sizeof(g_providers_json), &used, "\"");
		xlort_json_escape_append(g_providers_json, sizeof(g_providers_json), &used, providers[i] != NULL ? providers[i] : "");
		xlort_append(g_providers_json, sizeof(g_providers_json), &used, "\"");
	}
	xlort_append(g_providers_json, sizeof(g_providers_json), &used, "]");
	(void)xlort_status_ok(g_api->ReleaseAvailableProviders(providers, provider_count), "ReleaseAvailableProviders");
	return g_providers_json;
}

XLORT_EXPORT const char* onnxruntime_xl_last_error(void)
{
	return g_error;
}

XLORT_EXPORT XlOrtSessionOptions onnxruntime_xl_create_session_options(void)
{
	XlOrtSessionOptionsImpl* options = (XlOrtSessionOptionsImpl*)calloc(1, sizeof(*options));
	if ( options == NULL ) {
		xlort_set_error("out of memory");
		return NULL;
	}
	options->intra_threads = 0;
	options->inter_threads = 0;
	options->graph_optimization_level = ORT_ENABLE_ALL;
	options->execution_mode = -1;
	options->cpu_mem_arena = -1;
	options->mem_pattern = -1;
	options->log_verbosity_level = -1;
	options->log_severity_level = -1;
	return options;
}

XLORT_EXPORT int onnxruntime_xl_session_options_set_intra_op_num_threads(XlOrtSessionOptions options, int value)
{
	XlOrtSessionOptionsImpl* opt = (XlOrtSessionOptionsImpl*)options;
	if ( opt == NULL ) return 0;
	opt->intra_threads = value;
	return 1;
}

XLORT_EXPORT int onnxruntime_xl_session_options_set_inter_op_num_threads(XlOrtSessionOptions options, int value)
{
	XlOrtSessionOptionsImpl* opt = (XlOrtSessionOptionsImpl*)options;
	if ( opt == NULL ) return 0;
	opt->inter_threads = value;
	return 1;
}

XLORT_EXPORT int onnxruntime_xl_session_options_set_graph_optimization_level(XlOrtSessionOptions options, int value)
{
	XlOrtSessionOptionsImpl* opt = (XlOrtSessionOptionsImpl*)options;
	if ( opt == NULL ) return 0;
	opt->graph_optimization_level = value;
	return 1;
}

XLORT_EXPORT int onnxruntime_xl_session_options_set_execution_mode(XlOrtSessionOptions options, int value)
{
	XlOrtSessionOptionsImpl* opt = (XlOrtSessionOptionsImpl*)options;
	if ( opt == NULL ) return 0;
	opt->execution_mode = value;
	return 1;
}

XLORT_EXPORT int onnxruntime_xl_session_options_enable_profiling(XlOrtSessionOptions options, const char* prefix)
{
	XlOrtSessionOptionsImpl* opt = (XlOrtSessionOptionsImpl*)options;
	if ( opt == NULL ) return 0;
	opt->profiling_enabled = 1;
	opt->profiling_disabled = 0;
	strncpy(opt->profiling_prefix, prefix != NULL ? prefix : "onnxruntime_profile", sizeof(opt->profiling_prefix) - 1u);
	opt->profiling_prefix[sizeof(opt->profiling_prefix) - 1u] = '\0';
	return 1;
}

XLORT_EXPORT int onnxruntime_xl_session_options_disable_profiling(XlOrtSessionOptions options)
{
	XlOrtSessionOptionsImpl* opt = (XlOrtSessionOptionsImpl*)options;
	if ( opt == NULL ) return 0;
	opt->profiling_enabled = 0;
	opt->profiling_disabled = 1;
	return 1;
}

XLORT_EXPORT int onnxruntime_xl_session_options_enable_cpu_mem_arena(XlOrtSessionOptions options)
{
	XlOrtSessionOptionsImpl* opt = (XlOrtSessionOptionsImpl*)options;
	if ( opt == NULL ) return 0;
	opt->cpu_mem_arena = 1;
	return 1;
}

XLORT_EXPORT int onnxruntime_xl_session_options_disable_cpu_mem_arena(XlOrtSessionOptions options)
{
	XlOrtSessionOptionsImpl* opt = (XlOrtSessionOptionsImpl*)options;
	if ( opt == NULL ) return 0;
	opt->cpu_mem_arena = 0;
	return 1;
}

XLORT_EXPORT int onnxruntime_xl_session_options_enable_mem_pattern(XlOrtSessionOptions options)
{
	XlOrtSessionOptionsImpl* opt = (XlOrtSessionOptionsImpl*)options;
	if ( opt == NULL ) return 0;
	opt->mem_pattern = 1;
	return 1;
}

XLORT_EXPORT int onnxruntime_xl_session_options_disable_mem_pattern(XlOrtSessionOptions options)
{
	XlOrtSessionOptionsImpl* opt = (XlOrtSessionOptionsImpl*)options;
	if ( opt == NULL ) return 0;
	opt->mem_pattern = 0;
	return 1;
}

XLORT_EXPORT int onnxruntime_xl_session_options_set_log_id(XlOrtSessionOptions options, const char* value)
{
	XlOrtSessionOptionsImpl* opt = (XlOrtSessionOptionsImpl*)options;
	if ( opt == NULL ) return 0;
	strncpy(opt->log_id, value != NULL ? value : "", sizeof(opt->log_id) - 1u);
	opt->log_id[sizeof(opt->log_id) - 1u] = '\0';
	return 1;
}

XLORT_EXPORT int onnxruntime_xl_session_options_set_log_verbosity_level(XlOrtSessionOptions options, int value)
{
	XlOrtSessionOptionsImpl* opt = (XlOrtSessionOptionsImpl*)options;
	if ( opt == NULL ) return 0;
	opt->log_verbosity_level = value;
	return 1;
}

XLORT_EXPORT int onnxruntime_xl_session_options_set_log_severity_level(XlOrtSessionOptions options, int value)
{
	XlOrtSessionOptionsImpl* opt = (XlOrtSessionOptionsImpl*)options;
	if ( opt == NULL ) return 0;
	opt->log_severity_level = value;
	return 1;
}

XLORT_EXPORT int onnxruntime_xl_session_options_append_execution_provider(XlOrtSessionOptions options, const char* provider_name)
{
	return onnxruntime_xl_session_options_append_execution_provider_with_options(options, provider_name, "{}");
}

XLORT_EXPORT int onnxruntime_xl_session_options_append_execution_provider_with_options(XlOrtSessionOptions options, const char* provider_name, const char* options_json)
{
	XlOrtSessionOptionsImpl* opt = (XlOrtSessionOptionsImpl*)options;
	if ( opt == NULL || provider_name == NULL || provider_name[0] == '\0' || opt->provider_count >= 8u ) return 0;
	strncpy(opt->provider_names[opt->provider_count], provider_name, sizeof(opt->provider_names[opt->provider_count]) - 1u);
	opt->provider_names[opt->provider_count][sizeof(opt->provider_names[opt->provider_count]) - 1u] = '\0';
	strncpy(opt->provider_options_json[opt->provider_count], options_json != NULL ? options_json : "{}", sizeof(opt->provider_options_json[opt->provider_count]) - 1u);
	opt->provider_options_json[opt->provider_count][sizeof(opt->provider_options_json[opt->provider_count]) - 1u] = '\0';
	opt->provider_count++;
	return 1;
}

XLORT_EXPORT void onnxruntime_xl_release_session_options(XlOrtSessionOptions options)
{
	free(options);
}

XLORT_EXPORT XlOrtRunOptions onnxruntime_xl_create_run_options(void)
{
	OrtRunOptions* options = NULL;
	if ( !xlort_init() ) {
		return NULL;
	}
	if ( !xlort_status_ok(g_api->CreateRunOptions(&options), "CreateRunOptions") ) {
		return NULL;
	}
	return options;
}

XLORT_EXPORT int onnxruntime_xl_run_options_set_log_verbosity_level(XlOrtRunOptions options, int value)
{
	if ( options == NULL || !xlort_init() ) return 0;
	return xlort_status_ok(g_api->RunOptionsSetRunLogVerbosityLevel((OrtRunOptions*)options, value), "RunOptionsSetRunLogVerbosityLevel");
}

XLORT_EXPORT int onnxruntime_xl_run_options_set_log_severity_level(XlOrtRunOptions options, int value)
{
	if ( options == NULL || !xlort_init() ) return 0;
	return xlort_status_ok(g_api->RunOptionsSetRunLogSeverityLevel((OrtRunOptions*)options, value), "RunOptionsSetRunLogSeverityLevel");
}

XLORT_EXPORT int onnxruntime_xl_run_options_set_run_tag(XlOrtRunOptions options, const char* value)
{
	if ( options == NULL || !xlort_init() ) return 0;
	return xlort_status_ok(g_api->RunOptionsSetRunTag((OrtRunOptions*)options, value != NULL ? value : ""), "RunOptionsSetRunTag");
}

XLORT_EXPORT int onnxruntime_xl_run_options_set_terminate(XlOrtRunOptions options)
{
	if ( options == NULL || !xlort_init() ) return 0;
	return xlort_status_ok(g_api->RunOptionsSetTerminate((OrtRunOptions*)options), "RunOptionsSetTerminate");
}

XLORT_EXPORT int onnxruntime_xl_run_options_unset_terminate(XlOrtRunOptions options)
{
	if ( options == NULL || !xlort_init() ) return 0;
	return xlort_status_ok(g_api->RunOptionsUnsetTerminate((OrtRunOptions*)options), "RunOptionsUnsetTerminate");
}

XLORT_EXPORT void onnxruntime_xl_release_run_options(XlOrtRunOptions options)
{
	if ( options != NULL && g_api != NULL ) {
		g_api->ReleaseRunOptions((OrtRunOptions*)options);
	}
}

static void xlort_modelmeta_append_string(OrtModelMetadata* meta, OrtAllocator* allocator, char* out, size_t out_size, size_t* used, const char* key, OrtStatus* (*fn)(const OrtModelMetadata*, OrtAllocator*, char**))
{
	char* value = NULL;
	xlort_append(out, out_size, used, "\"");
	xlort_json_escape_append(out, out_size, used, key);
	xlort_append(out, out_size, used, "\":\"");
	if ( meta != NULL && allocator != NULL && fn != NULL &&
		xlort_status_ok(fn(meta, allocator, &value), key) ) {
		xlort_json_escape_append(out, out_size, used, value != NULL ? value : "");
	}
	xlort_append(out, out_size, used, "\"");
	if ( value != NULL ) allocator->Free(allocator, value);
}

static void xlort_describe_modelmeta(XlOrtSessionImpl* sess)
{
	OrtAllocator* allocator = NULL;
	OrtModelMetadata* meta = NULL;
	char** keys = NULL;
	int64_t num_keys = 0;
	int64_t version = 0;
	size_t used = 0;
	int64_t i;

	strncpy(sess->modelmeta_json, "{}", sizeof(sess->modelmeta_json));
	if ( sess == NULL || sess->session == NULL ) return;
	if ( !xlort_status_ok(g_api->GetAllocatorWithDefaultOptions(&allocator), "GetAllocatorWithDefaultOptions") ) return;
	if ( !xlort_status_ok(g_api->SessionGetModelMetadata(sess->session, &meta), "SessionGetModelMetadata") ) return;

	xlort_append(sess->modelmeta_json, sizeof(sess->modelmeta_json), &used, "{");
	xlort_modelmeta_append_string(meta, allocator, sess->modelmeta_json, sizeof(sess->modelmeta_json), &used, "producer_name", g_api->ModelMetadataGetProducerName);
	xlort_append(sess->modelmeta_json, sizeof(sess->modelmeta_json), &used, ",");
	xlort_modelmeta_append_string(meta, allocator, sess->modelmeta_json, sizeof(sess->modelmeta_json), &used, "graph_name", g_api->ModelMetadataGetGraphName);
	xlort_append(sess->modelmeta_json, sizeof(sess->modelmeta_json), &used, ",");
	xlort_modelmeta_append_string(meta, allocator, sess->modelmeta_json, sizeof(sess->modelmeta_json), &used, "domain", g_api->ModelMetadataGetDomain);
	xlort_append(sess->modelmeta_json, sizeof(sess->modelmeta_json), &used, ",");
	xlort_modelmeta_append_string(meta, allocator, sess->modelmeta_json, sizeof(sess->modelmeta_json), &used, "description", g_api->ModelMetadataGetDescription);
	xlort_append(sess->modelmeta_json, sizeof(sess->modelmeta_json), &used, ",\"version\":");
	if ( xlort_status_ok(g_api->ModelMetadataGetVersion(meta, &version), "ModelMetadataGetVersion") ) {
		xlort_appendf(sess->modelmeta_json, sizeof(sess->modelmeta_json), &used, "%lld", (long long)version);
	} else {
		xlort_append(sess->modelmeta_json, sizeof(sess->modelmeta_json), &used, "0");
	}
	xlort_append(sess->modelmeta_json, sizeof(sess->modelmeta_json), &used, ",\"custom_metadata_map\":{");
	if ( xlort_status_ok(g_api->ModelMetadataGetCustomMetadataMapKeys(meta, allocator, &keys, &num_keys), "ModelMetadataGetCustomMetadataMapKeys") &&
		keys != NULL && num_keys > 0 ) {
		for ( i = 0; i < num_keys; ++i ) {
			char* value = NULL;
			if ( i > 0 ) xlort_append(sess->modelmeta_json, sizeof(sess->modelmeta_json), &used, ",");
			xlort_append(sess->modelmeta_json, sizeof(sess->modelmeta_json), &used, "\"");
			xlort_json_escape_append(sess->modelmeta_json, sizeof(sess->modelmeta_json), &used, keys[i] != NULL ? keys[i] : "");
			xlort_append(sess->modelmeta_json, sizeof(sess->modelmeta_json), &used, "\":\"");
			if ( keys[i] != NULL &&
				xlort_status_ok(g_api->ModelMetadataLookupCustomMetadataMap(meta, allocator, keys[i], &value), "ModelMetadataLookupCustomMetadataMap") ) {
				xlort_json_escape_append(sess->modelmeta_json, sizeof(sess->modelmeta_json), &used, value != NULL ? value : "");
			}
			xlort_append(sess->modelmeta_json, sizeof(sess->modelmeta_json), &used, "\"");
			if ( value != NULL ) allocator->Free(allocator, value);
		}
	}
	xlort_append(sess->modelmeta_json, sizeof(sess->modelmeta_json), &used, "}");
	xlort_append(sess->modelmeta_json, sizeof(sess->modelmeta_json), &used, "}");
	if ( keys != NULL ) {
		for ( i = 0; i < num_keys; ++i ) {
			if ( keys[i] != NULL ) allocator->Free(allocator, keys[i]);
		}
		allocator->Free(allocator, keys);
	}
	g_api->ReleaseModelMetadata(meta);
	xlort_set_error("");
}

XLORT_EXPORT XlOrtSession onnxruntime_xl_create_session(const char* model_path, XlOrtSessionOptions options)
{
	XlOrtSessionImpl* sess;
	OrtStatus* status;
	XlOrtSessionOptionsImpl* xopts = (XlOrtSessionOptionsImpl*)options;

	if ( !xlort_init() ) {
		return NULL;
	}
	if ( model_path == NULL || model_path[0] == '\0' ) {
		xlort_set_error("empty model path");
		return NULL;
	}

	sess = (XlOrtSessionImpl*)calloc(1, sizeof(*sess));
	if ( sess == NULL ) {
		xlort_set_error("out of memory");
		return NULL;
	}
	status = g_api->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "xlang-onnxruntime", &sess->env);
	if ( !xlort_status_ok(status, "CreateEnv") ) {
		free(sess);
		return NULL;
	}
	status = g_api->CreateSessionOptions(&sess->options);
	if ( !xlort_status_ok(status, "CreateSessionOptions") ) {
		onnxruntime_xl_release_session(sess);
		return NULL;
	}
	if ( xopts != NULL ) {
		if ( xopts->intra_threads > 0 ) (void)g_api->SetIntraOpNumThreads(sess->options, xopts->intra_threads);
		if ( xopts->inter_threads > 0 ) (void)g_api->SetInterOpNumThreads(sess->options, xopts->inter_threads);
		(void)g_api->SetSessionGraphOptimizationLevel(sess->options, (GraphOptimizationLevel)xopts->graph_optimization_level);
		if ( xopts->execution_mode >= 0 ) (void)g_api->SetSessionExecutionMode(sess->options, (ExecutionMode)xopts->execution_mode);
		if ( xopts->cpu_mem_arena == 1 ) (void)g_api->EnableCpuMemArena(sess->options);
		if ( xopts->cpu_mem_arena == 0 ) (void)g_api->DisableCpuMemArena(sess->options);
		if ( xopts->mem_pattern == 1 ) (void)g_api->EnableMemPattern(sess->options);
		if ( xopts->mem_pattern == 0 ) (void)g_api->DisableMemPattern(sess->options);
		if ( xopts->log_id[0] != '\0' ) (void)g_api->SetSessionLogId(sess->options, xopts->log_id);
		if ( xopts->log_verbosity_level >= 0 ) (void)g_api->SetSessionLogVerbosityLevel(sess->options, xopts->log_verbosity_level);
		if ( xopts->log_severity_level >= 0 ) (void)g_api->SetSessionLogSeverityLevel(sess->options, xopts->log_severity_level);
		if ( xopts->profiling_enabled ) {
#if defined(_WIN32) || defined(_WIN64)
			wchar_t wide_prefix[1024];
			MultiByteToWideChar(CP_UTF8, 0, xopts->profiling_prefix, -1, wide_prefix, 1024);
			(void)g_api->EnableProfiling(sess->options, wide_prefix);
#else
			(void)g_api->EnableProfiling(sess->options, xopts->profiling_prefix);
#endif
		}
		if ( xopts->profiling_disabled ) (void)g_api->DisableProfiling(sess->options);
		{
			size_t pi;
			for ( pi = 0; pi < xopts->provider_count; ++pi ) {
				char keys_buf[16][128];
				char values_buf[16][256];
				const char* keys[16];
				const char* values[16];
				size_t option_count = 0;
				size_t oi;
				if ( !xlort_parse_json_map_entries(xopts->provider_options_json[pi], keys_buf, values_buf, 16u, &option_count) ) {
					onnxruntime_xl_release_session(sess);
					return NULL;
				}
				if ( strcmp(xopts->provider_names[pi], "CPUExecutionProvider") == 0 || strcmp(xopts->provider_names[pi], "CPU") == 0 ) {
					for ( oi = 0; oi < option_count; ++oi ) {
						if ( strcmp(keys_buf[oi], "use_arena") == 0 ) {
							if ( strcmp(values_buf[oi], "0") == 0 || strcmp(values_buf[oi], "false") == 0 ) {
								(void)g_api->DisableCpuMemArena(sess->options);
							} else {
								(void)g_api->EnableCpuMemArena(sess->options);
							}
						}
					}
					continue;
				}
				for ( oi = 0; oi < option_count; ++oi ) {
					keys[oi] = keys_buf[oi];
					values[oi] = values_buf[oi];
				}
				if ( !xlort_status_ok(g_api->SessionOptionsAppendExecutionProvider(sess->options, xopts->provider_names[pi], keys, values, option_count), "SessionOptionsAppendExecutionProvider") ) {
					onnxruntime_xl_release_session(sess);
					return NULL;
				}
			}
		}
	} else {
		(void)g_api->SetSessionGraphOptimizationLevel(sess->options, ORT_ENABLE_ALL);
	}
#if defined(_WIN32) || defined(_WIN64)
	{
		wchar_t wide_path[1024];
		MultiByteToWideChar(CP_UTF8, 0, model_path, -1, wide_path, 1024);
		status = g_api->CreateSession(sess->env, wide_path, sess->options, &sess->session);
	}
#else
	status = g_api->CreateSession(sess->env, model_path, sess->options, &sess->session);
#endif
	if ( !xlort_status_ok(status, "CreateSession") ) {
		onnxruntime_xl_release_session(sess);
		return NULL;
	}
	xlort_describe_names(sess);
	xlort_describe_modelmeta(sess);
	return sess;
}

XLORT_EXPORT void onnxruntime_xl_release_session(XlOrtSession session)
{
	XlOrtSessionImpl* sess = (XlOrtSessionImpl*)session;
	if ( sess == NULL ) {
		return;
	}
	if ( g_api != NULL ) {
		if ( sess->session != NULL ) g_api->ReleaseSession(sess->session);
		if ( sess->options != NULL ) g_api->ReleaseSessionOptions(sess->options);
		if ( sess->env != NULL ) g_api->ReleaseEnv(sess->env);
	}
	free(sess);
}

XLORT_EXPORT const char* onnxruntime_xl_get_inputs_json(XlOrtSession session)
{
	XlOrtSessionImpl* sess = (XlOrtSessionImpl*)session;
	return sess != NULL ? sess->inputs_json : "[]";
}

XLORT_EXPORT const char* onnxruntime_xl_get_outputs_json(XlOrtSession session)
{
	XlOrtSessionImpl* sess = (XlOrtSessionImpl*)session;
	return sess != NULL ? sess->outputs_json : "[]";
}

XLORT_EXPORT const char* onnxruntime_xl_get_modelmeta_json(XlOrtSession session)
{
	XlOrtSessionImpl* sess = (XlOrtSessionImpl*)session;
	return sess != NULL ? sess->modelmeta_json : "{}";
}

XLORT_EXPORT const char* onnxruntime_xl_session_end_profiling(XlOrtSession session)
{
	XlOrtSessionImpl* sess = (XlOrtSessionImpl*)session;
	OrtAllocator* allocator = NULL;
	char* out = NULL;
	if ( sess == NULL || sess->session == NULL || !xlort_init() ) {
		xlort_set_error("invalid InferenceSession");
		return "";
	}
	sess->profiling_file[0] = '\0';
	if ( !xlort_status_ok(g_api->GetAllocatorWithDefaultOptions(&allocator), "GetAllocatorWithDefaultOptions") ||
		!xlort_status_ok(g_api->SessionEndProfiling(sess->session, allocator, &out), "SessionEndProfiling") ) {
		return "";
	}
	strncpy(sess->profiling_file, out != NULL ? out : "", sizeof(sess->profiling_file) - 1u);
	sess->profiling_file[sizeof(sess->profiling_file) - 1u] = '\0';
	if ( out != NULL ) allocator->Free(allocator, out);
	xlort_set_error("");
	return sess->profiling_file;
}

XLORT_EXPORT XlOrtIoBinding onnxruntime_xl_create_iobinding(XlOrtSession session)
{
	XlOrtSessionImpl* sess = (XlOrtSessionImpl*)session;
	XlOrtIoBindingImpl* b;
	if ( sess == NULL || sess->session == NULL || !xlort_init() ) {
		xlort_set_error("invalid InferenceSession");
		return NULL;
	}
	b = (XlOrtIoBindingImpl*)calloc(1, sizeof(*b));
	if ( b == NULL ) {
		xlort_set_error("out of memory");
		return NULL;
	}
	b->sess = sess;
	strncpy(b->outputs_json, "{}", sizeof(b->outputs_json));
	if ( !xlort_status_ok(g_api->CreateIoBinding(sess->session, &b->binding), "CreateIoBinding") ) {
		free(b);
		return NULL;
	}
	return b;
}

XLORT_EXPORT int onnxruntime_xl_iobinding_bind_cpu_input_json(XlOrtIoBinding binding, const char* name, const char* tensor_json)
{
	XlOrtIoBindingImpl* b = (XlOrtIoBindingImpl*)binding;
	XlOrtInputTensor* tensor;
	OrtMemoryInfo* memory_info = NULL;
	const char* body;
	if ( b == NULL || b->binding == NULL || name == NULL || tensor_json == NULL || b->input_count >= 16u ) return 0;
	body = strchr(tensor_json, '{');
	if ( body == NULL ) {
		xlort_set_error("tensor_json must be an object");
		return 0;
	}
	tensor = &b->inputs[b->input_count];
	if ( !xlort_parse_input_tensor_body(body, name, tensor) ) return 0;
	if ( !xlort_status_ok(g_api->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &memory_info), "CreateCpuMemoryInfo") ) {
		xlort_free_input_tensors(tensor, 1);
		return 0;
	}
	if ( !xlort_create_ort_value_from_tensor(tensor, memory_info) ) {
		g_api->ReleaseMemoryInfo(memory_info);
		xlort_free_input_tensors(tensor, 1);
		return 0;
	}
	g_api->ReleaseMemoryInfo(memory_info);
	if ( !xlort_status_ok(g_api->BindInput(b->binding, name, tensor->value), "BindInput") ) {
		xlort_free_input_tensors(tensor, 1);
		return 0;
	}
	b->input_count++;
	return 1;
}

XLORT_EXPORT int onnxruntime_xl_iobinding_bind_output(XlOrtIoBinding binding, const char* name)
{
	XlOrtIoBindingImpl* b = (XlOrtIoBindingImpl*)binding;
	OrtMemoryInfo* memory_info = NULL;
	if ( b == NULL || b->binding == NULL || name == NULL || b->output_count >= 16u ) return 0;
	if ( !xlort_status_ok(g_api->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &memory_info), "CreateCpuMemoryInfo") ) return 0;
	if ( !xlort_status_ok(g_api->BindOutputToDevice(b->binding, name, memory_info), "BindOutputToDevice") ) {
		g_api->ReleaseMemoryInfo(memory_info);
		return 0;
	}
	g_api->ReleaseMemoryInfo(memory_info);
	strncpy(b->output_names[b->output_count], name, sizeof(b->output_names[b->output_count]) - 1u);
	b->output_names[b->output_count][sizeof(b->output_names[b->output_count]) - 1u] = '\0';
	b->output_count++;
	return 1;
}

XLORT_EXPORT int onnxruntime_xl_iobinding_clear_inputs(XlOrtIoBinding binding)
{
	XlOrtIoBindingImpl* b = (XlOrtIoBindingImpl*)binding;
	if ( b == NULL || b->binding == NULL ) return 0;
	g_api->ClearBoundInputs(b->binding);
	xlort_free_input_tensors(b->inputs, b->input_count);
	memset(b->inputs, 0, sizeof(b->inputs));
	b->input_count = 0;
	return 1;
}

XLORT_EXPORT int onnxruntime_xl_iobinding_clear_outputs(XlOrtIoBinding binding)
{
	XlOrtIoBindingImpl* b = (XlOrtIoBindingImpl*)binding;
	if ( b == NULL || b->binding == NULL ) return 0;
	g_api->ClearBoundOutputs(b->binding);
	memset(b->output_names, 0, sizeof(b->output_names));
	b->output_count = 0;
	strncpy(b->outputs_json, "{}", sizeof(b->outputs_json));
	return 1;
}

XLORT_EXPORT int onnxruntime_xl_iobinding_synchronize_inputs(XlOrtIoBinding binding)
{
	XlOrtIoBindingImpl* b = (XlOrtIoBindingImpl*)binding;
	if ( b == NULL || b->binding == NULL ) return 0;
	return xlort_status_ok(g_api->SynchronizeBoundInputs(b->binding), "SynchronizeBoundInputs");
}

XLORT_EXPORT int onnxruntime_xl_iobinding_synchronize_outputs(XlOrtIoBinding binding)
{
	XlOrtIoBindingImpl* b = (XlOrtIoBindingImpl*)binding;
	if ( b == NULL || b->binding == NULL ) return 0;
	return xlort_status_ok(g_api->SynchronizeBoundOutputs(b->binding), "SynchronizeBoundOutputs");
}

XLORT_EXPORT XlOrtResult onnxruntime_xl_run_with_iobinding(XlOrtSession session, XlOrtIoBinding binding, XlOrtRunOptions options)
{
	XlOrtSessionImpl* sess = (XlOrtSessionImpl*)session;
	XlOrtIoBindingImpl* b = (XlOrtIoBindingImpl*)binding;
	XlOrtResultImpl* result;
	OrtAllocator* allocator = NULL;
	OrtValue** values = NULL;
	size_t value_count = 0;
	size_t used = 0;
	size_t i;
	if ( sess == NULL || sess->session == NULL || b == NULL || b->binding == NULL ) {
		xlort_set_error("invalid InferenceSession or IoBinding");
		return NULL;
	}
	if ( !xlort_status_ok(g_api->RunWithBinding(sess->session, (OrtRunOptions*)options, b->binding), "RunWithBinding") ||
		!xlort_status_ok(g_api->SynchronizeBoundOutputs(b->binding), "SynchronizeBoundOutputs") ||
		!xlort_status_ok(g_api->GetAllocatorWithDefaultOptions(&allocator), "GetAllocatorWithDefaultOptions") ||
		!xlort_status_ok(g_api->GetBoundOutputValues(b->binding, allocator, &values, &value_count), "GetBoundOutputValues") ) {
		return NULL;
	}
	result = (XlOrtResultImpl*)calloc(1, sizeof(*result));
	if ( result == NULL ) {
		if ( values != NULL ) allocator->Free(allocator, values);
		xlort_set_error("out of memory");
		return NULL;
	}
	xlort_append(result->json, sizeof(result->json), &used, "{\"outputs\":{");
	for ( i = 0; i < value_count; ++i ) {
		if ( i > 0 ) xlort_append(result->json, sizeof(result->json), &used, ",");
		xlort_append_ort_value_json(result->json, sizeof(result->json), &used, i < b->output_count ? b->output_names[i] : "", values[i]);
	}
	xlort_append(result->json, sizeof(result->json), &used, "}}");
	strncpy(b->outputs_json, result->json, sizeof(b->outputs_json) - 1u);
	b->outputs_json[sizeof(b->outputs_json) - 1u] = '\0';
	for ( i = 0; i < value_count; ++i ) {
		if ( values[i] != NULL ) g_api->ReleaseValue(values[i]);
	}
	if ( values != NULL ) allocator->Free(allocator, values);
	xlort_set_error("");
	return result;
}

XLORT_EXPORT const char* onnxruntime_xl_iobinding_outputs_json(XlOrtIoBinding binding)
{
	XlOrtIoBindingImpl* b = (XlOrtIoBindingImpl*)binding;
	return b != NULL ? b->outputs_json : "{}";
}

XLORT_EXPORT void onnxruntime_xl_release_iobinding(XlOrtIoBinding binding)
{
	XlOrtIoBindingImpl* b = (XlOrtIoBindingImpl*)binding;
	if ( b == NULL ) return;
	if ( b->binding != NULL && g_api != NULL ) g_api->ReleaseIoBinding(b->binding);
	xlort_free_input_tensors(b->inputs, b->input_count);
	free(b);
}

XLORT_EXPORT XlOrtTensor onnxruntime_xl_tensor_from_f32(const float* data, int data_count, const long long* shape, int shape_count)
{
	XlOrtTensorImpl* tensor;
	int i;
	size_t expected = 1u;
	if ( data == NULL || data_count <= 0 || shape == NULL || shape_count <= 0 || shape_count > 8 ) {
		xlort_set_error("invalid float32 tensor input");
		return NULL;
	}
	tensor = (XlOrtTensorImpl*)calloc(1, sizeof(*tensor));
	if ( tensor == NULL ) {
		xlort_set_error("out of memory");
		return NULL;
	}
	tensor->shape_count = (size_t)shape_count;
	for ( i = 0; i < shape_count; ++i ) {
		if ( shape[i] <= 0 ) {
			free(tensor);
			xlort_set_error("tensor shape dimensions must be positive");
			return NULL;
		}
		tensor->shape[i] = (int64_t)shape[i];
		expected *= (size_t)shape[i];
	}
	if ( expected != (size_t)data_count ) {
		free(tensor);
		xlort_set_error("tensor data_count does not match shape");
		return NULL;
	}
	tensor->data_count = (size_t)data_count;
	tensor->data = (float*)malloc(sizeof(float) * tensor->data_count);
	if ( tensor->data == NULL ) {
		free(tensor);
		xlort_set_error("out of memory");
		return NULL;
	}
	memcpy(tensor->data, data, sizeof(float) * tensor->data_count);
	xlort_set_error("");
	return tensor;
}

XLORT_EXPORT XlOrtTensor onnxruntime_xl_tensor_adopt_f32(float* data, int data_count, const long long* shape, int shape_count)
{
	XlOrtTensorImpl* tensor;
	int i;
	size_t expected = 1u;
	if ( data == NULL || data_count <= 0 || shape == NULL || shape_count <= 0 || shape_count > 8 ) {
		xlort_set_error("invalid float32 tensor input");
		return NULL;
	}
	tensor = (XlOrtTensorImpl*)calloc(1, sizeof(*tensor));
	if ( tensor == NULL ) {
		xlort_set_error("out of memory");
		return NULL;
	}
	tensor->shape_count = (size_t)shape_count;
	for ( i = 0; i < shape_count; ++i ) {
		if ( shape[i] <= 0 ) {
			free(tensor);
			xlort_set_error("tensor shape dimensions must be positive");
			return NULL;
		}
		tensor->shape[i] = (int64_t)shape[i];
		expected *= (size_t)shape[i];
	}
	if ( expected != (size_t)data_count ) {
		free(tensor);
		xlort_set_error("tensor data_count does not match shape");
		return NULL;
	}
	tensor->data_count = (size_t)data_count;
	tensor->data = data;
	xlort_set_error("");
	return tensor;
}

XLORT_EXPORT XlOrtTensor onnxruntime_xl_tensor_zeros_f32(const char* shape_json)
{
	double dims[8];
	size_t dim_count = 0;
	long long shape[8];
	size_t count = 1u;
	size_t i;
	float* zeros;
	XlOrtTensor tensor;
	if ( shape_json == NULL || !xlort_parse_number_array_double(shape_json, dims, 8u, &dim_count) || dim_count == 0u ) {
		xlort_set_error("invalid shape_json");
		return NULL;
	}
	for ( i = 0; i < dim_count; ++i ) {
		if ( dims[i] <= 0.0 ) {
			xlort_set_error("tensor shape dimensions must be positive");
			return NULL;
		}
		shape[i] = (long long)dims[i];
		count *= (size_t)shape[i];
	}
	zeros = (float*)calloc(count, sizeof(float));
	if ( zeros == NULL ) {
		xlort_set_error("out of memory");
		return NULL;
	}
	tensor = onnxruntime_xl_tensor_from_f32(zeros, (int)count, shape, (int)dim_count);
	free(zeros);
	return tensor;
}

XLORT_EXPORT void onnxruntime_xl_release_tensor(XlOrtTensor tensor)
{
	XlOrtTensorImpl* t = (XlOrtTensorImpl*)tensor;
	if ( t != NULL ) {
		free(t->data);
		free(t);
	}
}

static XlOrtResult xlort_run_tensor_impl(XlOrtSession session, const char* input_name, XlOrtTensor tensor, const char* output_names_json, int include_data_json)
{
	XlOrtSessionImpl* sess = (XlOrtSessionImpl*)session;
	XlOrtTensorImpl* t = (XlOrtTensorImpl*)tensor;
	XlOrtResultImpl* result;
	const char* input_names[1];
	const char* output_names[16];
	OrtValue* input_value = NULL;
	OrtValue* output_values[16];
	OrtMemoryInfo* memory_info = NULL;
	OrtStatus* status;
	size_t output_count;
	size_t i;
	size_t used;
	if ( sess == NULL || sess->session == NULL || input_name == NULL || input_name[0] == '\0' || t == NULL || t->data == NULL ) {
		xlort_set_error("invalid session, input name, or tensor");
		return NULL;
	}
	memset(output_values, 0, sizeof(output_values));
	if ( !xlort_parse_string_array(output_names_json, sess->output_names, sess->output_count, output_names, &output_count) ) {
		return NULL;
	}
	result = (XlOrtResultImpl*)calloc(1, sizeof(*result));
	if ( result == NULL ) {
		xlort_set_error("out of memory");
		return NULL;
	}
	status = g_api->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &memory_info);
	if ( !xlort_status_ok(status, "CreateCpuMemoryInfo") ) {
		free(result);
		return NULL;
	}
	status = g_api->CreateTensorWithDataAsOrtValue(
		memory_info,
		t->data,
		sizeof(float) * t->data_count,
		t->shape,
		t->shape_count,
		ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
		&input_value
	);
	g_api->ReleaseMemoryInfo(memory_info);
	if ( !xlort_status_ok(status, "CreateTensorWithDataAsOrtValue") ) {
		free(result);
		return NULL;
	}
	input_names[0] = input_name;
	status = g_api->Run(sess->session, NULL, input_names, (const OrtValue* const*)&input_value, 1u, output_names, output_count, output_values);
	g_api->ReleaseValue(input_value);
	if ( !xlort_status_ok(status, "Run") ) {
		free(result);
		return NULL;
	}
	used = 0;
	xlort_append(result->json, sizeof(result->json), &used, "{\"outputs\":{");
	for ( i = 0; i < output_count; ++i ) {
		OrtTensorTypeAndShapeInfo* info = NULL;
		ONNXTensorElementDataType elem_type = ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED;
		size_t dim_count = 0;
		size_t elem_count = 0;
		int64_t dims[8];
		void* data = NULL;
		if ( i > 0 ) xlort_append(result->json, sizeof(result->json), &used, ",");
		xlort_append(result->json, sizeof(result->json), &used, "\"");
		xlort_json_escape_append(result->json, sizeof(result->json), &used, output_names[i]);
		xlort_append(result->json, sizeof(result->json), &used, "\":{");
		if ( output_values[i] == NULL ||
			!xlort_status_ok(g_api->GetTensorTypeAndShape(output_values[i], &info), "GetTensorTypeAndShape") ||
			!xlort_status_ok(g_api->GetTensorElementType(info, &elem_type), "GetTensorElementType") ||
			!xlort_status_ok(g_api->GetDimensionsCount(info, &dim_count), "GetDimensionsCount") ||
			dim_count > 8u ||
			!xlort_status_ok(g_api->GetDimensions(info, dims, dim_count), "GetDimensions") ||
			!xlort_status_ok(g_api->GetTensorShapeElementCount(info, &elem_count), "GetTensorShapeElementCount") ) {
			xlort_append(result->json, sizeof(result->json), &used, "\"error\":\"failed to read output tensor\"");
			if ( info != NULL ) g_api->ReleaseTensorTypeAndShapeInfo(info);
			continue;
		}
		if ( xlort_dtype_size(elem_type) != 0u &&
			xlort_status_ok(g_api->GetTensorMutableData(output_values[i], &data), "GetTensorMutableData") ) {
			if ( elem_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ) {
				xlort_capture_first_float_output(result, dims, dim_count, data, elem_count);
			}
			if ( include_data_json ) {
				xlort_append_tensor_data_json(result->json, sizeof(result->json), &used, elem_type, dims, dim_count, data, elem_count);
			} else {
				size_t di;
				xlort_appendf(result->json, sizeof(result->json), &used, "\"dtype\":\"%s\",\"shape\":[", xlort_dtype_name(elem_type));
				for ( di = 0; di < dim_count; ++di ) {
					if ( di > 0 ) xlort_append(result->json, sizeof(result->json), &used, ",");
					xlort_appendf(result->json, sizeof(result->json), &used, "%lld", (long long)dims[di]);
				}
				xlort_append(result->json, sizeof(result->json), &used, "]");
			}
		} else {
			xlort_appendf(result->json, sizeof(result->json), &used, "\"error\":\"unsupported output dtype %d\"", (int)elem_type);
		}
		xlort_append(result->json, sizeof(result->json), &used, "}");
		if ( info != NULL ) g_api->ReleaseTensorTypeAndShapeInfo(info);
	}
	xlort_append(result->json, sizeof(result->json), &used, "}}");
	for ( i = 0; i < output_count; ++i ) {
		if ( output_values[i] != NULL ) g_api->ReleaseValue(output_values[i]);
	}
	xlort_set_error("");
	return result;
}

XLORT_EXPORT XlOrtResult onnxruntime_xl_run_tensor(XlOrtSession session, const char* input_name, XlOrtTensor tensor, const char* output_names_json)
{
	return xlort_run_tensor_impl(session, input_name, tensor, output_names_json, 1);
}

XLORT_EXPORT XlOrtResult onnxruntime_xl_run_tensor_fast(XlOrtSession session, const char* input_name, XlOrtTensor tensor, const char* output_names_json)
{
	return xlort_run_tensor_impl(session, input_name, tensor, output_names_json, 0);
}

XLORT_EXPORT XlOrtResult onnxruntime_xl_run_json_with_options(XlOrtSession session, const char* output_names_json, const char* input_feed_json, XlOrtRunOptions run_options)
{
	XlOrtSessionImpl* sess = (XlOrtSessionImpl*)session;
	XlOrtResultImpl* result;
	XlOrtInputTensor inputs[16];
	const char* input_names[16];
	const char* output_names[16];
	OrtValue* input_values[16];
	OrtValue* output_values[16];
	OrtMemoryInfo* memory_info = NULL;
	OrtStatus* status;
	size_t input_count;
	size_t output_count;
	size_t i;
	size_t used;

	if ( sess == NULL || sess->session == NULL ) {
		xlort_set_error("invalid InferenceSession");
		return NULL;
	}
	if ( input_feed_json == NULL || input_feed_json[0] == '\0' ) {
		xlort_set_error("input_feed_json is empty");
		return NULL;
	}
	if ( sess->input_count > 16u || sess->output_count > 16u ) {
		xlort_set_error("models with more than 16 inputs or outputs are not supported by run_json yet");
		return NULL;
	}

	result = (XlOrtResultImpl*)calloc(1, sizeof(*result));
	if ( result == NULL ) {
		xlort_set_error("out of memory");
		return NULL;
	}

	memset(inputs, 0, sizeof(inputs));
	memset(input_names, 0, sizeof(input_names));
	memset(output_names, 0, sizeof(output_names));
	memset(input_values, 0, sizeof(input_values));
	memset(output_values, 0, sizeof(output_values));

	input_count = sess->input_count;
	output_count = sess->output_count;
	for ( i = 0; i < input_count; ++i ) {
		if ( !xlort_parse_input_tensor(input_feed_json, sess->input_names[i], &inputs[i]) ) {
			xlort_free_input_tensors(inputs, input_count);
			free(result);
			return NULL;
		}
		input_names[i] = inputs[i].name;
	}

	status = g_api->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &memory_info);
	if ( !xlort_status_ok(status, "CreateCpuMemoryInfo") ) {
		xlort_free_input_tensors(inputs, input_count);
		free(result);
		return NULL;
	}
	for ( i = 0; i < input_count; ++i ) {
		if ( inputs[i].elem_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING ) {
			OrtAllocator* allocator = NULL;
			if ( !xlort_status_ok(g_api->GetAllocatorWithDefaultOptions(&allocator), "GetAllocatorWithDefaultOptions") ) {
				g_api->ReleaseMemoryInfo(memory_info);
				xlort_free_input_tensors(inputs, input_count);
				free(result);
				return NULL;
			}
			status = g_api->CreateTensorAsOrtValue(
				allocator,
				inputs[i].shape,
				inputs[i].shape_count,
				ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING,
				&inputs[i].value
			);
			if ( !xlort_status_ok(status, "CreateTensorAsOrtValue") ||
				!xlort_status_ok(g_api->FillStringTensor(inputs[i].value, (const char* const*)inputs[i].strings, inputs[i].data_count), "FillStringTensor") ) {
				g_api->ReleaseMemoryInfo(memory_info);
				xlort_free_input_tensors(inputs, input_count);
				free(result);
				return NULL;
			}
		} else {
			status = g_api->CreateTensorWithDataAsOrtValue(
				memory_info,
				inputs[i].data,
				inputs[i].data_bytes,
				inputs[i].shape,
				inputs[i].shape_count,
				inputs[i].elem_type,
				&inputs[i].value
			);
			if ( !xlort_status_ok(status, "CreateTensorWithDataAsOrtValue") ) {
				g_api->ReleaseMemoryInfo(memory_info);
				xlort_free_input_tensors(inputs, input_count);
				free(result);
				return NULL;
			}
		}
		input_values[i] = inputs[i].value;
	}
	g_api->ReleaseMemoryInfo(memory_info);

	if ( !xlort_parse_string_array(output_names_json, sess->output_names, sess->output_count, output_names, &output_count) ) {
		xlort_free_input_tensors(inputs, input_count);
		free(result);
		return NULL;
	}

	status = g_api->Run(
		sess->session,
		(OrtRunOptions*)run_options,
		input_names,
		(const OrtValue* const*)input_values,
		input_count,
		output_names,
		output_count,
		output_values
	);
	if ( !xlort_status_ok(status, "Run") ) {
		xlort_free_input_tensors(inputs, input_count);
		free(result);
		return NULL;
	}

	used = 0;
	xlort_append(result->json, sizeof(result->json), &used, "{\"outputs\":{");
	for ( i = 0; i < output_count; ++i ) {
		OrtTensorTypeAndShapeInfo* info = NULL;
		ONNXTensorElementDataType elem_type = ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED;
		size_t dim_count = 0;
		size_t elem_count = 0;
		int64_t dims[8];
		void* data = NULL;

		if ( i > 0 ) xlort_append(result->json, sizeof(result->json), &used, ",");
		xlort_append(result->json, sizeof(result->json), &used, "\"");
		xlort_json_escape_append(result->json, sizeof(result->json), &used, output_names[i]);
		xlort_append(result->json, sizeof(result->json), &used, "\":{");

		if ( output_values[i] == NULL ||
			!xlort_status_ok(g_api->GetTensorTypeAndShape(output_values[i], &info), "GetTensorTypeAndShape") ||
			!xlort_status_ok(g_api->GetTensorElementType(info, &elem_type), "GetTensorElementType") ||
			!xlort_status_ok(g_api->GetDimensionsCount(info, &dim_count), "GetDimensionsCount") ||
			dim_count > 8u ||
			!xlort_status_ok(g_api->GetDimensions(info, dims, dim_count), "GetDimensions") ||
			!xlort_status_ok(g_api->GetTensorShapeElementCount(info, &elem_count), "GetTensorShapeElementCount") ) {
			xlort_append(result->json, sizeof(result->json), &used, "\"error\":\"failed to read output tensor\"");
			if ( info != NULL ) g_api->ReleaseTensorTypeAndShapeInfo(info);
			continue;
		}

		if ( elem_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING ) {
			if ( !xlort_append_string_tensor_json(result->json, sizeof(result->json), &used, output_values[i], dims, dim_count, elem_count) ) {
				xlort_append(result->json, sizeof(result->json), &used, "\"error\":\"failed to read string output tensor\"");
			}
		} else if ( xlort_dtype_size(elem_type) != 0u &&
			xlort_status_ok(g_api->GetTensorMutableData(output_values[i], &data), "GetTensorMutableData") ) {
			if ( elem_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ) {
				xlort_capture_first_float_output(result, dims, dim_count, data, elem_count);
			}
			xlort_append_tensor_data_json(result->json, sizeof(result->json), &used, elem_type, dims, dim_count, data, elem_count);
		} else {
			xlort_appendf(result->json, sizeof(result->json), &used, "\"error\":\"unsupported output dtype %d\"", (int)elem_type);
		}
		xlort_append(result->json, sizeof(result->json), &used, "}");
		if ( info != NULL ) g_api->ReleaseTensorTypeAndShapeInfo(info);
	}
	xlort_append(result->json, sizeof(result->json), &used, "}}");

	for ( i = 0; i < output_count; ++i ) {
		if ( output_values[i] != NULL ) {
			g_api->ReleaseValue(output_values[i]);
		}
	}
	xlort_free_input_tensors(inputs, input_count);
	xlort_set_error("");
	return result;
}

XLORT_EXPORT XlOrtResult onnxruntime_xl_run_json(XlOrtSession session, const char* output_names_json, const char* input_feed_json)
{
	return onnxruntime_xl_run_json_with_options(session, output_names_json, input_feed_json, NULL);
}

XLORT_EXPORT const char* onnxruntime_xl_result_json(XlOrtResult result)
{
	XlOrtResultImpl* r = (XlOrtResultImpl*)result;
	return r != NULL ? r->json : "{}";
}

XLORT_EXPORT const char* onnxruntime_xl_result_float_shape_json(XlOrtResult result)
{
	XlOrtResultImpl* r = (XlOrtResultImpl*)result;
	return (r != NULL && r->float_shape_json[0] != '\0') ? r->float_shape_json : "[]";
}

XLORT_EXPORT int onnxruntime_xl_result_float_count(XlOrtResult result)
{
	XlOrtResultImpl* r = (XlOrtResultImpl*)result;
	return (r != NULL && r->float_count <= (size_t)2147483647) ? (int)r->float_count : 0;
}

XLORT_EXPORT float onnxruntime_xl_result_float_at(XlOrtResult result, int index)
{
	XlOrtResultImpl* r = (XlOrtResultImpl*)result;
	if ( r == NULL || r->float_data == NULL || index < 0 || (size_t)index >= r->float_count ) return 0.0f;
	return r->float_data[index];
}

XLORT_EXPORT const float* onnxruntime_xl_result_float_data(XlOrtResult result)
{
	XlOrtResultImpl* r = (XlOrtResultImpl*)result;
	return r != NULL ? r->float_data : NULL;
}

XLORT_EXPORT void onnxruntime_xl_release_result(XlOrtResult result)
{
	XlOrtResultImpl* r = (XlOrtResultImpl*)result;
	if ( r != NULL ) {
		free(r->float_data);
		free(r);
	}
}
