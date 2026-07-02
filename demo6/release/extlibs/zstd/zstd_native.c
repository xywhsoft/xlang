#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "third_party/zstd.h"

#if defined(_WIN32) || defined(_WIN64)
#define XZSTD_EXPORT __declspec(dllexport)
#else
#define XZSTD_EXPORT
#endif

#if defined(__GNUC__)
#define XZSTD_THREAD __thread
#elif defined(_MSC_VER)
#define XZSTD_THREAD __declspec(thread)
#else
#define XZSTD_THREAD
#endif

static XZSTD_THREAD char g_xzstd_error[512];
#define XZSTD_RESULT_SLOTS 8u

static XZSTD_THREAD char* g_xzstd_results[XZSTD_RESULT_SLOTS];
static XZSTD_THREAD size_t g_xzstd_result_index;

XZSTD_EXPORT const char* xzstd_compress_hex_level(const char* text, int level);
XZSTD_EXPORT int xzstd_compress_file_level(const char* input_path, const char* output_path, int level);

static void xzstd_set_error(const char* text)
{
    size_t i = 0u;

    if ( text == NULL ) {
        text = "";
    }

    while ( i + 1u < sizeof(g_xzstd_error) && text[i] != '\0' ) {
        g_xzstd_error[i] = text[i];
        ++i;
    }
    g_xzstd_error[i] = '\0';
}

static const char* xzstd_store_result(char* text)
{
	char** slot = &g_xzstd_results[g_xzstd_result_index % XZSTD_RESULT_SLOTS];
	g_xzstd_result_index = (g_xzstd_result_index + 1u) % XZSTD_RESULT_SLOTS;
	free(*slot);
	*slot = text;
	return *slot == NULL ? "" : *slot;
}

static char* xzstd_hex_encode(const unsigned char* data, size_t size)
{
	static const char digits[] = "0123456789abcdef";
	char* out = (char*)malloc(size * 2u + 1u);
	size_t i;
	if ( out == NULL ) return NULL;
	for ( i = 0u; i < size; ++i ) {
		out[i * 2u] = digits[data[i] >> 4];
		out[i * 2u + 1u] = digits[data[i] & 15u];
	}
	out[size * 2u] = '\0';
	return out;
}

static int xzstd_hex_value(char ch)
{
	if ( ch >= '0' && ch <= '9' ) return ch - '0';
	if ( ch >= 'a' && ch <= 'f' ) return ch - 'a' + 10;
	if ( ch >= 'A' && ch <= 'F' ) return ch - 'A' + 10;
	return -1;
}

static unsigned char* xzstd_hex_decode(const char* text, size_t* out_size)
{
	size_t len;
	size_t i;
	unsigned char* out;
	if ( text == NULL ) text = "";
	len = strlen(text);
	if ( (len & 1u) != 0u ) {
		xzstd_set_error("hex text has odd length");
		return NULL;
	}
	out = (unsigned char*)malloc(len / 2u + 1u);
	if ( out == NULL ) {
		xzstd_set_error("out of memory");
		return NULL;
	}
	for ( i = 0u; i < len; i += 2u ) {
		int hi = xzstd_hex_value(text[i]);
		int lo = xzstd_hex_value(text[i + 1u]);
		if ( hi < 0 || lo < 0 ) {
			free(out);
			xzstd_set_error("hex text contains invalid character");
			return NULL;
		}
		out[i / 2u] = (unsigned char)((hi << 4) | lo);
	}
	*out_size = len / 2u;
	return out;
}

static unsigned char* xzstd_read_file(const char* path, size_t* out_size)
{
	FILE* fp;
	long len;
	unsigned char* data;
	if ( path == NULL || path[0] == '\0' ) {
		xzstd_set_error("empty file path");
		return NULL;
	}
	fp = fopen(path, "rb");
	if ( fp == NULL ) {
		xzstd_set_error("failed to open input file");
		return NULL;
	}
	if ( fseek(fp, 0, SEEK_END) != 0 ) {
		fclose(fp);
		xzstd_set_error("failed to seek input file");
		return NULL;
	}
	len = ftell(fp);
	if ( len < 0 ) {
		fclose(fp);
		xzstd_set_error("failed to measure input file");
		return NULL;
	}
	rewind(fp);
	data = (unsigned char*)malloc((size_t)len + 1u);
	if ( data == NULL ) {
		fclose(fp);
		xzstd_set_error("out of memory");
		return NULL;
	}
	if ( len > 0 && fread(data, 1u, (size_t)len, fp) != (size_t)len ) {
		free(data);
		fclose(fp);
		xzstd_set_error("failed to read input file");
		return NULL;
	}
	fclose(fp);
	*out_size = (size_t)len;
	return data;
}

static int xzstd_write_file(const char* path, const unsigned char* data, size_t size)
{
	FILE* fp;
	if ( path == NULL || path[0] == '\0' ) {
		xzstd_set_error("empty output file path");
		return 0;
	}
	fp = fopen(path, "wb");
	if ( fp == NULL ) {
		xzstd_set_error("failed to open output file");
		return 0;
	}
	if ( size > 0u && fwrite(data, 1u, size, fp) != size ) {
		fclose(fp);
		xzstd_set_error("failed to write output file");
		return 0;
	}
	fclose(fp);
	return 1;
}

static int xzstd_compress_bytes(const unsigned char* src, size_t src_size, int level, unsigned char** out, size_t* out_size)
{
	size_t bound = ZSTD_compressBound(src_size);
	size_t written;
	if ( ZSTD_isError(bound) || bound == 0u ) {
		xzstd_set_error("input too large");
		return 0;
	}
	*out = (unsigned char*)malloc(bound);
	if ( *out == NULL ) {
		xzstd_set_error("out of memory");
		return 0;
	}
	written = ZSTD_compress(*out, bound, src, src_size, level);
	if ( ZSTD_isError(written) ) {
		xzstd_set_error(ZSTD_getErrorName(written));
		free(*out);
		*out = NULL;
		return 0;
	}
	*out_size = written;
	xzstd_set_error("");
	return 1;
}

static int xzstd_decompress_bytes(const unsigned char* src, size_t src_size, unsigned char** out, size_t* out_size)
{
	unsigned long long content_size = ZSTD_getFrameContentSize(src, src_size);
	size_t written;
	if ( content_size == ZSTD_CONTENTSIZE_ERROR ) {
		xzstd_set_error("not a zstd frame");
		return 0;
	}
	if ( content_size == ZSTD_CONTENTSIZE_UNKNOWN ) {
		xzstd_set_error("zstd frame has unknown content size");
		return 0;
	}
	*out = (unsigned char*)malloc((size_t)content_size + 1u);
	if ( *out == NULL ) {
		xzstd_set_error("out of memory");
		return 0;
	}
	written = ZSTD_decompress(*out, (size_t)content_size, src, src_size);
	if ( ZSTD_isError(written) ) {
		xzstd_set_error(ZSTD_getErrorName(written));
		free(*out);
		*out = NULL;
		return 0;
	}
	*out_size = written;
	xzstd_set_error("");
	return 1;
}

XZSTD_EXPORT const char* xzstd_version(void)
{
	return ZSTD_versionString();
}

XZSTD_EXPORT const char* xzstd_last_error(void)
{
	return g_xzstd_error;
}

XZSTD_EXPORT const char* xzstd_compress_hex(const char* text)
{
	return xzstd_compress_hex_level(text, ZSTD_defaultCLevel());
}

XZSTD_EXPORT const char* xzstd_compress_hex_level(const char* text, int level)
{
	unsigned char* packed = NULL;
	size_t packed_size = 0u;
	char* hex;
	if ( text == NULL ) text = "";
	if ( !xzstd_compress_bytes((const unsigned char*)text, strlen(text), level, &packed, &packed_size) ) return "";
	hex = xzstd_hex_encode(packed, packed_size);
	free(packed);
	if ( hex == NULL ) {
		xzstd_set_error("out of memory");
		return "";
	}
	return xzstd_store_result(hex);
}

XZSTD_EXPORT const char* xzstd_decompress_hex(const char* hex_text)
{
	unsigned char* packed;
	unsigned char* plain = NULL;
	size_t packed_size = 0u;
	size_t plain_size = 0u;
	char* text;
	packed = xzstd_hex_decode(hex_text, &packed_size);
	if ( packed == NULL ) return "";
	if ( !xzstd_decompress_bytes(packed, packed_size, &plain, &plain_size) ) {
		free(packed);
		return "";
	}
	free(packed);
	text = (char*)malloc(plain_size + 1u);
	if ( text == NULL ) {
		free(plain);
		xzstd_set_error("out of memory");
		return "";
	}
	memcpy(text, plain, plain_size);
	text[plain_size] = '\0';
	free(plain);
	return xzstd_store_result(text);
}

XZSTD_EXPORT int xzstd_compress_file(const char* input_path, const char* output_path)
{
	return xzstd_compress_file_level(input_path, output_path, ZSTD_defaultCLevel());
}

XZSTD_EXPORT int xzstd_compress_file_level(const char* input_path, const char* output_path, int level)
{
	unsigned char* src;
	unsigned char* packed = NULL;
	size_t src_size = 0u;
	size_t packed_size = 0u;
	int ok;
	src = xzstd_read_file(input_path, &src_size);
	if ( src == NULL ) return 0;
	ok = xzstd_compress_bytes(src, src_size, level, &packed, &packed_size);
	free(src);
	if ( !ok ) return 0;
	ok = xzstd_write_file(output_path, packed, packed_size);
	free(packed);
	return ok;
}

XZSTD_EXPORT int xzstd_decompress_file(const char* input_path, const char* output_path)
{
	unsigned char* src;
	unsigned char* plain = NULL;
	size_t src_size = 0u;
	size_t plain_size = 0u;
	int ok;
	src = xzstd_read_file(input_path, &src_size);
	if ( src == NULL ) return 0;
	ok = xzstd_decompress_bytes(src, src_size, &plain, &plain_size);
	free(src);
	if ( !ok ) return 0;
	ok = xzstd_write_file(output_path, plain, plain_size);
	free(plain);
	return ok;
}
