#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "third_party/Alloc.h"
#include "third_party/LzmaDec.h"
#include "third_party/LzmaEnc.h"

#if defined(_WIN32) || defined(_WIN64)
#define XLZMA_EXPORT __declspec(dllexport)
#else
#define XLZMA_EXPORT
#endif

#if defined(__GNUC__)
#define XLZMA_THREAD __thread
#elif defined(_MSC_VER)
#define XLZMA_THREAD __declspec(thread)
#else
#define XLZMA_THREAD
#endif

#define XLZMA_HEADER_SIZE 13u
#define XLZMA_RESULT_SLOTS 8u

static XLZMA_THREAD char g_xlzma_error[512];
static XLZMA_THREAD char* g_xlzma_results[XLZMA_RESULT_SLOTS];
static XLZMA_THREAD size_t g_xlzma_result_index;

XLZMA_EXPORT const char* xlzma_compress_hex_level(const char* text, int level);
XLZMA_EXPORT int xlzma_compress_file_level(const char* input_path, const char* output_path, int level);

static void xlzma_set_error(const char* text)
{
    size_t i = 0u;

    if ( text == NULL ) {
        text = "";
    }

    while ( i + 1u < sizeof(g_xlzma_error) && text[i] != '\0' ) {
        g_xlzma_error[i] = text[i];
        ++i;
    }
    g_xlzma_error[i] = '\0';
}

static const char* xlzma_store_result(char* text)
{
	char** slot = &g_xlzma_results[g_xlzma_result_index % XLZMA_RESULT_SLOTS];
	g_xlzma_result_index = (g_xlzma_result_index + 1u) % XLZMA_RESULT_SLOTS;
	free(*slot);
	*slot = text;
	return *slot == NULL ? "" : *slot;
}

static char* xlzma_hex_encode(const unsigned char* data, size_t size)
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

static int xlzma_hex_value(char ch)
{
	if ( ch >= '0' && ch <= '9' ) return ch - '0';
	if ( ch >= 'a' && ch <= 'f' ) return ch - 'a' + 10;
	if ( ch >= 'A' && ch <= 'F' ) return ch - 'A' + 10;
	return -1;
}

static unsigned char* xlzma_hex_decode(const char* text, size_t* out_size)
{
	size_t len;
	size_t i;
	unsigned char* out;
	if ( text == NULL ) text = "";
	len = strlen(text);
	if ( (len & 1u) != 0u ) {
		xlzma_set_error("hex text has odd length");
		return NULL;
	}
	out = (unsigned char*)malloc(len / 2u + 1u);
	if ( out == NULL ) {
		xlzma_set_error("out of memory");
		return NULL;
	}
	for ( i = 0u; i < len; i += 2u ) {
		int hi = xlzma_hex_value(text[i]);
		int lo = xlzma_hex_value(text[i + 1u]);
		if ( hi < 0 || lo < 0 ) {
			free(out);
			xlzma_set_error("hex text contains invalid character");
			return NULL;
		}
		out[i / 2u] = (unsigned char)((hi << 4) | lo);
	}
	*out_size = len / 2u;
	return out;
}

static unsigned char* xlzma_read_file(const char* path, size_t* out_size)
{
	FILE* fp;
	long len;
	unsigned char* data;
	if ( path == NULL || path[0] == '\0' ) {
		xlzma_set_error("empty file path");
		return NULL;
	}
	fp = fopen(path, "rb");
	if ( fp == NULL ) {
		xlzma_set_error("failed to open input file");
		return NULL;
	}
	if ( fseek(fp, 0, SEEK_END) != 0 ) {
		fclose(fp);
		xlzma_set_error("failed to seek input file");
		return NULL;
	}
	len = ftell(fp);
	if ( len < 0 ) {
		fclose(fp);
		xlzma_set_error("failed to measure input file");
		return NULL;
	}
	rewind(fp);
	data = (unsigned char*)malloc((size_t)len + 1u);
	if ( data == NULL ) {
		fclose(fp);
		xlzma_set_error("out of memory");
		return NULL;
	}
	if ( len > 0 && fread(data, 1u, (size_t)len, fp) != (size_t)len ) {
		free(data);
		fclose(fp);
		xlzma_set_error("failed to read input file");
		return NULL;
	}
	fclose(fp);
	*out_size = (size_t)len;
	return data;
}

static int xlzma_write_file(const char* path, const unsigned char* data, size_t size)
{
	FILE* fp;
	if ( path == NULL || path[0] == '\0' ) {
		xlzma_set_error("empty output file path");
		return 0;
	}
	fp = fopen(path, "wb");
	if ( fp == NULL ) {
		xlzma_set_error("failed to open output file");
		return 0;
	}
	if ( size > 0u && fwrite(data, 1u, size, fp) != size ) {
		fclose(fp);
		xlzma_set_error("failed to write output file");
		return 0;
	}
	fclose(fp);
	return 1;
}

static void xlzma_write_u64le(unsigned char* dst, size_t value)
{
	unsigned int i;
	for ( i = 0u; i < 8u; ++i ) {
		dst[i] = (unsigned char)(((unsigned long long)value >> (i * 8u)) & 0xffu);
	}
}

static size_t xlzma_read_u64le(const unsigned char* src)
{
	unsigned long long value = 0u;
	unsigned int i;
	for ( i = 0u; i < 8u; ++i ) {
		value |= ((unsigned long long)src[i]) << (i * 8u);
	}
	return (size_t)value;
}

static const char* xlzma_error_name(SRes rc)
{
	switch ( rc ) {
	case SZ_OK: return "";
	case SZ_ERROR_DATA: return "lzma data error";
	case SZ_ERROR_MEM: return "lzma memory error";
	case SZ_ERROR_CRC: return "lzma crc error";
	case SZ_ERROR_UNSUPPORTED: return "lzma unsupported data";
	case SZ_ERROR_PARAM: return "lzma parameter error";
	case SZ_ERROR_INPUT_EOF: return "lzma input eof";
	case SZ_ERROR_OUTPUT_EOF: return "lzma output eof";
	default: return "lzma operation failed";
	}
}

static int xlzma_compress_bytes(const unsigned char* src, size_t src_size, int level, unsigned char** out, size_t* out_size)
{
	CLzmaEncProps props;
	unsigned char props_encoded[LZMA_PROPS_SIZE];
	SizeT props_size = LZMA_PROPS_SIZE;
	SizeT payload_bound = src_size + src_size / 3u + 128u;
	SizeT payload_size = payload_bound;
	SRes rc;
	if ( level < 0 ) level = 5;
	if ( level > 9 ) level = 9;
	*out = (unsigned char*)malloc(XLZMA_HEADER_SIZE + payload_bound);
	if ( *out == NULL ) {
		xlzma_set_error("out of memory");
		return 0;
	}
	LzmaEncProps_Init(&props);
	props.level = level;
	rc = LzmaEncode(*out + XLZMA_HEADER_SIZE, &payload_size, src, src_size, &props, props_encoded, &props_size, 0, NULL, &g_Alloc, &g_Alloc);
	if ( rc != SZ_OK ) {
		free(*out);
		*out = NULL;
		xlzma_set_error(xlzma_error_name(rc));
		return 0;
	}
	if ( props_size != LZMA_PROPS_SIZE ) {
		free(*out);
		*out = NULL;
		xlzma_set_error("unexpected lzma property size");
		return 0;
	}
	memcpy(*out, props_encoded, LZMA_PROPS_SIZE);
	xlzma_write_u64le(*out + LZMA_PROPS_SIZE, src_size);
	*out_size = XLZMA_HEADER_SIZE + payload_size;
	xlzma_set_error("");
	return 1;
}

static int xlzma_decompress_bytes(const unsigned char* src, size_t src_size, unsigned char** out, size_t* out_size)
{
	size_t plain_size;
	SizeT dst_len;
	SizeT src_len;
	ELzmaStatus status;
	SRes rc;
	if ( src_size < XLZMA_HEADER_SIZE ) {
		xlzma_set_error("lzma data is too small");
		return 0;
	}
	plain_size = xlzma_read_u64le(src + LZMA_PROPS_SIZE);
	*out = (unsigned char*)malloc(plain_size + 1u);
	if ( *out == NULL ) {
		xlzma_set_error("out of memory");
		return 0;
	}
	dst_len = plain_size;
	src_len = src_size - XLZMA_HEADER_SIZE;
	rc = LzmaDecode(*out, &dst_len, src + XLZMA_HEADER_SIZE, &src_len, src, LZMA_PROPS_SIZE, LZMA_FINISH_END, &status, &g_Alloc);
	if ( rc != SZ_OK ) {
		free(*out);
		*out = NULL;
		xlzma_set_error(xlzma_error_name(rc));
		return 0;
	}
	if ( dst_len != plain_size ) {
		free(*out);
		*out = NULL;
		xlzma_set_error("unexpected decompressed size");
		return 0;
	}
	*out_size = dst_len;
	xlzma_set_error("");
	return 1;
}

XLZMA_EXPORT int xlzma_available(void)
{
	return 1;
}

XLZMA_EXPORT const char* xlzma_version(void)
{
	return "lzma 0.1.0";
}

XLZMA_EXPORT const char* xlzma_last_error(void)
{
	return g_xlzma_error;
}

XLZMA_EXPORT const char* xlzma_compress_hex(const char* text)
{
	return xlzma_compress_hex_level(text, 5);
}

XLZMA_EXPORT const char* xlzma_compress_hex_level(const char* text, int level)
{
	unsigned char* packed = NULL;
	size_t packed_size = 0u;
	char* hex;
	if ( text == NULL ) text = "";
	if ( !xlzma_compress_bytes((const unsigned char*)text, strlen(text), level, &packed, &packed_size) ) return "";
	hex = xlzma_hex_encode(packed, packed_size);
	free(packed);
	if ( hex == NULL ) {
		xlzma_set_error("out of memory");
		return "";
	}
	return xlzma_store_result(hex);
}

XLZMA_EXPORT const char* xlzma_decompress_hex(const char* hex_text)
{
	unsigned char* packed;
	unsigned char* plain = NULL;
	size_t packed_size = 0u;
	size_t plain_size = 0u;
	char* text;
	packed = xlzma_hex_decode(hex_text, &packed_size);
	if ( packed == NULL ) return "";
	if ( !xlzma_decompress_bytes(packed, packed_size, &plain, &plain_size) ) {
		free(packed);
		return "";
	}
	free(packed);
	text = (char*)malloc(plain_size + 1u);
	if ( text == NULL ) {
		free(plain);
		xlzma_set_error("out of memory");
		return "";
	}
	memcpy(text, plain, plain_size);
	text[plain_size] = '\0';
	free(plain);
	return xlzma_store_result(text);
}

XLZMA_EXPORT int xlzma_compress_file(const char* input_path, const char* output_path)
{
	return xlzma_compress_file_level(input_path, output_path, 5);
}

XLZMA_EXPORT int xlzma_compress_file_level(const char* input_path, const char* output_path, int level)
{
	unsigned char* src;
	unsigned char* packed = NULL;
	size_t src_size = 0u;
	size_t packed_size = 0u;
	int ok;
	src = xlzma_read_file(input_path, &src_size);
	if ( src == NULL ) return 0;
	ok = xlzma_compress_bytes(src, src_size, level, &packed, &packed_size);
	free(src);
	if ( !ok ) return 0;
	ok = xlzma_write_file(output_path, packed, packed_size);
	free(packed);
	return ok;
}

XLZMA_EXPORT int xlzma_decompress_file(const char* input_path, const char* output_path)
{
	unsigned char* src;
	unsigned char* plain = NULL;
	size_t src_size = 0u;
	size_t plain_size = 0u;
	int ok;
	src = xlzma_read_file(input_path, &src_size);
	if ( src == NULL ) return 0;
	ok = xlzma_decompress_bytes(src, src_size, &plain, &plain_size);
	free(src);
	if ( !ok ) return 0;
	ok = xlzma_write_file(output_path, plain, plain_size);
	free(plain);
	return ok;
}
