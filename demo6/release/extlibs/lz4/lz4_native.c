#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "third_party/lz4.h"

#if defined(_WIN32) || defined(_WIN64)
#define XLZ4_EXPORT __declspec(dllexport)
#else
#define XLZ4_EXPORT
#endif

#if defined(__GNUC__)
#define XLZ4_THREAD __thread
#elif defined(_MSC_VER)
#define XLZ4_THREAD __declspec(thread)
#else
#define XLZ4_THREAD
#endif

static XLZ4_THREAD char g_xlz4_error[512];

#define XLZ4_HEADER_SIZE 4u
#define XLZ4_RESULT_SLOTS 8u

static XLZ4_THREAD char* g_xlz4_results[XLZ4_RESULT_SLOTS];
static XLZ4_THREAD size_t g_xlz4_result_index;

static void xlz4_set_error(const char* text)
{
    size_t i = 0u;

    if ( text == NULL ) {
        text = "";
    }

    while ( i + 1u < sizeof(g_xlz4_error) && text[i] != '\0' ) {
        g_xlz4_error[i] = text[i];
        ++i;
    }
    g_xlz4_error[i] = '\0';
}

static const char* xlz4_store_result(char* text)
{
	char** slot = &g_xlz4_results[g_xlz4_result_index % XLZ4_RESULT_SLOTS];
	g_xlz4_result_index = (g_xlz4_result_index + 1u) % XLZ4_RESULT_SLOTS;
	free(*slot);
	*slot = text;
	return *slot == NULL ? "" : *slot;
}

static char* xlz4_hex_encode(const unsigned char* data, size_t size)
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

static int xlz4_hex_value(char ch)
{
	if ( ch >= '0' && ch <= '9' ) return ch - '0';
	if ( ch >= 'a' && ch <= 'f' ) return ch - 'a' + 10;
	if ( ch >= 'A' && ch <= 'F' ) return ch - 'A' + 10;
	return -1;
}

static unsigned char* xlz4_hex_decode(const char* text, size_t* out_size)
{
	size_t len;
	size_t i;
	unsigned char* out;
	if ( text == NULL ) text = "";
	len = strlen(text);
	if ( (len & 1u) != 0u ) {
		xlz4_set_error("hex text has odd length");
		return NULL;
	}
	out = (unsigned char*)malloc(len / 2u + 1u);
	if ( out == NULL ) {
		xlz4_set_error("out of memory");
		return NULL;
	}
	for ( i = 0u; i < len; i += 2u ) {
		int hi = xlz4_hex_value(text[i]);
		int lo = xlz4_hex_value(text[i + 1u]);
		if ( hi < 0 || lo < 0 ) {
			free(out);
			xlz4_set_error("hex text contains invalid character");
			return NULL;
		}
		out[i / 2u] = (unsigned char)((hi << 4) | lo);
	}
	*out_size = len / 2u;
	return out;
}

static unsigned char* xlz4_read_file(const char* path, size_t* out_size)
{
	FILE* fp;
	long len;
	unsigned char* data;
	if ( path == NULL || path[0] == '\0' ) {
		xlz4_set_error("empty file path");
		return NULL;
	}
	fp = fopen(path, "rb");
	if ( fp == NULL ) {
		xlz4_set_error("failed to open input file");
		return NULL;
	}
	if ( fseek(fp, 0, SEEK_END) != 0 ) {
		fclose(fp);
		xlz4_set_error("failed to seek input file");
		return NULL;
	}
	len = ftell(fp);
	if ( len < 0 ) {
		fclose(fp);
		xlz4_set_error("failed to measure input file");
		return NULL;
	}
	rewind(fp);
	data = (unsigned char*)malloc((size_t)len + 1u);
	if ( data == NULL ) {
		fclose(fp);
		xlz4_set_error("out of memory");
		return NULL;
	}
	if ( len > 0 && fread(data, 1u, (size_t)len, fp) != (size_t)len ) {
		free(data);
		fclose(fp);
		xlz4_set_error("failed to read input file");
		return NULL;
	}
	fclose(fp);
	*out_size = (size_t)len;
	return data;
}

static int xlz4_write_file(const char* path, const unsigned char* data, size_t size)
{
	FILE* fp;
	if ( path == NULL || path[0] == '\0' ) {
		xlz4_set_error("empty output file path");
		return 0;
	}
	fp = fopen(path, "wb");
	if ( fp == NULL ) {
		xlz4_set_error("failed to open output file");
		return 0;
	}
	if ( size > 0u && fwrite(data, 1u, size, fp) != size ) {
		fclose(fp);
		xlz4_set_error("failed to write output file");
		return 0;
	}
	fclose(fp);
	return 1;
}

static void xlz4_write_u32le(unsigned char* dst, size_t value)
{
	dst[0] = (unsigned char)(value & 0xffu);
	dst[1] = (unsigned char)((value >> 8) & 0xffu);
	dst[2] = (unsigned char)((value >> 16) & 0xffu);
	dst[3] = (unsigned char)((value >> 24) & 0xffu);
}

static size_t xlz4_read_u32le(const unsigned char* src)
{
	return ((size_t)src[0]) | ((size_t)src[1] << 8) | ((size_t)src[2] << 16) | ((size_t)src[3] << 24);
}

static int xlz4_compress_bytes(const unsigned char* src, size_t src_size, unsigned char** out, size_t* out_size)
{
	int bound;
	int written;
	if ( src_size > 0x7e000000u ) {
		xlz4_set_error("input too large");
		return 0;
	}
	bound = LZ4_compressBound((int)src_size);
	if ( bound <= 0 ) {
		xlz4_set_error("input too large");
		return 0;
	}
	*out = (unsigned char*)malloc(XLZ4_HEADER_SIZE + (size_t)bound);
	if ( *out == NULL ) {
		xlz4_set_error("out of memory");
		return 0;
	}
	xlz4_write_u32le(*out, src_size);
	written = LZ4_compress_default((const char*)src, (char*)(*out + XLZ4_HEADER_SIZE), (int)src_size, bound);
	if ( written <= 0 ) {
		free(*out);
		*out = NULL;
		xlz4_set_error("lz4 compression failed");
		return 0;
	}
	*out_size = XLZ4_HEADER_SIZE + (size_t)written;
	xlz4_set_error("");
	return 1;
}

static int xlz4_decompress_bytes(const unsigned char* src, size_t src_size, unsigned char** out, size_t* out_size)
{
	size_t plain_size;
	int written;
	if ( src_size < XLZ4_HEADER_SIZE ) {
		xlz4_set_error("lz4 data is too small");
		return 0;
	}
	plain_size = xlz4_read_u32le(src);
	if ( plain_size > 0x7e000000u ) {
		xlz4_set_error("decompressed size is too large");
		return 0;
	}
	*out = (unsigned char*)malloc(plain_size + 1u);
	if ( *out == NULL ) {
		xlz4_set_error("out of memory");
		return 0;
	}
	written = LZ4_decompress_safe((const char*)(src + XLZ4_HEADER_SIZE), (char*)*out, (int)(src_size - XLZ4_HEADER_SIZE), (int)plain_size);
	if ( written < 0 || (size_t)written != plain_size ) {
		free(*out);
		*out = NULL;
		xlz4_set_error("lz4 decompression failed");
		return 0;
	}
	*out_size = plain_size;
	xlz4_set_error("");
	return 1;
}

XLZ4_EXPORT int xlz4_available(void)
{
	return 1;
}

XLZ4_EXPORT const char* xlz4_version(void)
{
	return LZ4_versionString();
}

XLZ4_EXPORT const char* xlz4_last_error(void)
{
	return g_xlz4_error;
}

XLZ4_EXPORT const char* xlz4_compress_hex(const char* text)
{
	unsigned char* packed = NULL;
	size_t packed_size = 0u;
	char* hex;
	if ( text == NULL ) text = "";
	if ( !xlz4_compress_bytes((const unsigned char*)text, strlen(text), &packed, &packed_size) ) return "";
	hex = xlz4_hex_encode(packed, packed_size);
	free(packed);
	if ( hex == NULL ) {
		xlz4_set_error("out of memory");
		return "";
	}
	return xlz4_store_result(hex);
}

XLZ4_EXPORT const char* xlz4_decompress_hex(const char* hex_text)
{
	unsigned char* packed;
	unsigned char* plain = NULL;
	size_t packed_size = 0u;
	size_t plain_size = 0u;
	char* text;
	packed = xlz4_hex_decode(hex_text, &packed_size);
	if ( packed == NULL ) return "";
	if ( !xlz4_decompress_bytes(packed, packed_size, &plain, &plain_size) ) {
		free(packed);
		return "";
	}
	free(packed);
	text = (char*)malloc(plain_size + 1u);
	if ( text == NULL ) {
		free(plain);
		xlz4_set_error("out of memory");
		return "";
	}
	memcpy(text, plain, plain_size);
	text[plain_size] = '\0';
	free(plain);
	return xlz4_store_result(text);
}

XLZ4_EXPORT int xlz4_compress_file(const char* input_path, const char* output_path)
{
	unsigned char* src;
	unsigned char* packed = NULL;
	size_t src_size = 0u;
	size_t packed_size = 0u;
	int ok;
	src = xlz4_read_file(input_path, &src_size);
	if ( src == NULL ) return 0;
	ok = xlz4_compress_bytes(src, src_size, &packed, &packed_size);
	free(src);
	if ( !ok ) return 0;
	ok = xlz4_write_file(output_path, packed, packed_size);
	free(packed);
	return ok;
}

XLZ4_EXPORT int xlz4_decompress_file(const char* input_path, const char* output_path)
{
	unsigned char* src;
	unsigned char* plain = NULL;
	size_t src_size = 0u;
	size_t plain_size = 0u;
	int ok;
	src = xlz4_read_file(input_path, &src_size);
	if ( src == NULL ) return 0;
	ok = xlz4_decompress_bytes(src, src_size, &plain, &plain_size);
	free(src);
	if ( !ok ) return 0;
	ok = xlz4_write_file(output_path, plain, plain_size);
	free(plain);
	return ok;
}
