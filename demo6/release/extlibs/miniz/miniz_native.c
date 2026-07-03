#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "third_party/miniz.h"
#include "third_party/miniz_zip.h"

#if defined(_WIN32) || defined(_WIN64)
#define XMINIZ_EXPORT __declspec(dllexport)
#else
#define XMINIZ_EXPORT
#endif

#if defined(__GNUC__)
#define XMINIZ_THREAD __thread
#elif defined(_MSC_VER)
#define XMINIZ_THREAD __declspec(thread)
#else
#define XMINIZ_THREAD
#endif

static XMINIZ_THREAD char g_xminiz_error[512];

#define XMINIZ_HEADER_SIZE 4u
#define XMINIZ_RESULT_SLOTS 8u

static XMINIZ_THREAD char* g_xminiz_results[XMINIZ_RESULT_SLOTS];
static XMINIZ_THREAD size_t g_xminiz_result_index;

static void xminiz_set_error(const char* text)
{
    size_t i = 0u;

    if ( text == NULL ) {
        text = "";
    }

    while ( i + 1u < sizeof(g_xminiz_error) && text[i] != '\0' ) {
        g_xminiz_error[i] = text[i];
        ++i;
    }
    g_xminiz_error[i] = '\0';
}

static const char* xminiz_store_result(char* text)
{
	char** slot = &g_xminiz_results[g_xminiz_result_index % XMINIZ_RESULT_SLOTS];
	g_xminiz_result_index = (g_xminiz_result_index + 1u) % XMINIZ_RESULT_SLOTS;
	free(*slot);
	*slot = text;
	return *slot == NULL ? "" : *slot;
}

static char* xminiz_hex_encode(const unsigned char* data, size_t size)
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

static int xminiz_hex_value(char ch)
{
	if ( ch >= '0' && ch <= '9' ) return ch - '0';
	if ( ch >= 'a' && ch <= 'f' ) return ch - 'a' + 10;
	if ( ch >= 'A' && ch <= 'F' ) return ch - 'A' + 10;
	return -1;
}

static unsigned char* xminiz_hex_decode(const char* text, size_t* out_size)
{
	size_t len;
	size_t i;
	unsigned char* out;
	if ( text == NULL ) text = "";
	len = strlen(text);
	if ( (len & 1u) != 0u ) {
		xminiz_set_error("hex text has odd length");
		return NULL;
	}
	out = (unsigned char*)malloc(len / 2u + 1u);
	if ( out == NULL ) {
		xminiz_set_error("out of memory");
		return NULL;
	}
	for ( i = 0u; i < len; i += 2u ) {
		int hi = xminiz_hex_value(text[i]);
		int lo = xminiz_hex_value(text[i + 1u]);
		if ( hi < 0 || lo < 0 ) {
			free(out);
			xminiz_set_error("hex text contains invalid character");
			return NULL;
		}
		out[i / 2u] = (unsigned char)((hi << 4) | lo);
	}
	*out_size = len / 2u;
	return out;
}

static unsigned char* xminiz_read_file(const char* path, size_t* out_size)
{
	FILE* fp;
	long len;
	unsigned char* data;
	if ( path == NULL || path[0] == '\0' ) {
		xminiz_set_error("empty file path");
		return NULL;
	}
	fp = fopen(path, "rb");
	if ( fp == NULL ) {
		xminiz_set_error("failed to open input file");
		return NULL;
	}
	if ( fseek(fp, 0, SEEK_END) != 0 ) {
		fclose(fp);
		xminiz_set_error("failed to seek input file");
		return NULL;
	}
	len = ftell(fp);
	if ( len < 0 ) {
		fclose(fp);
		xminiz_set_error("failed to measure input file");
		return NULL;
	}
	rewind(fp);
	data = (unsigned char*)malloc((size_t)len + 1u);
	if ( data == NULL ) {
		fclose(fp);
		xminiz_set_error("out of memory");
		return NULL;
	}
	if ( len > 0 && fread(data, 1u, (size_t)len, fp) != (size_t)len ) {
		free(data);
		fclose(fp);
		xminiz_set_error("failed to read input file");
		return NULL;
	}
	fclose(fp);
	*out_size = (size_t)len;
	return data;
}

static int xminiz_write_file(const char* path, const unsigned char* data, size_t size)
{
	FILE* fp;
	if ( path == NULL || path[0] == '\0' ) {
		xminiz_set_error("empty output file path");
		return 0;
	}
	fp = fopen(path, "wb");
	if ( fp == NULL ) {
		xminiz_set_error("failed to open output file");
		return 0;
	}
	if ( size > 0u && fwrite(data, 1u, size, fp) != size ) {
		fclose(fp);
		xminiz_set_error("failed to write output file");
		return 0;
	}
	fclose(fp);
	return 1;
}

static void xminiz_write_u32le(unsigned char* dst, size_t value)
{
	dst[0] = (unsigned char)(value & 0xffu);
	dst[1] = (unsigned char)((value >> 8) & 0xffu);
	dst[2] = (unsigned char)((value >> 16) & 0xffu);
	dst[3] = (unsigned char)((value >> 24) & 0xffu);
}

static size_t xminiz_read_u32le(const unsigned char* src)
{
	return ((size_t)src[0]) | ((size_t)src[1] << 8) | ((size_t)src[2] << 16) | ((size_t)src[3] << 24);
}

static int xminiz_compress_bytes(const unsigned char* src, size_t src_size, int level, unsigned char** out, size_t* out_size)
{
	mz_ulong bound;
	mz_ulong written;
	int status;
	if ( src_size > 0x7e000000u ) {
		xminiz_set_error("input too large");
		return 0;
	}
	bound = mz_compressBound((mz_ulong)src_size);
	if ( bound == 0u ) {
		xminiz_set_error("input too large");
		return 0;
	}
	*out = (unsigned char*)malloc(XMINIZ_HEADER_SIZE + (size_t)bound);
	if ( *out == NULL ) {
		xminiz_set_error("out of memory");
		return 0;
	}
	xminiz_write_u32le(*out, src_size);
	written = bound;
	status = mz_compress2(*out + XMINIZ_HEADER_SIZE, &written, src, (mz_ulong)src_size, level);
	if ( status != MZ_OK ) {
		free(*out);
		*out = NULL;
		xminiz_set_error(mz_error(status));
		return 0;
	}
	*out_size = XMINIZ_HEADER_SIZE + (size_t)written;
	xminiz_set_error("");
	return 1;
}

static int xminiz_decompress_bytes(const unsigned char* src, size_t src_size, unsigned char** out, size_t* out_size)
{
	size_t plain_size;
	mz_ulong written;
	int status;
	if ( src_size < XMINIZ_HEADER_SIZE ) {
		xminiz_set_error("miniz data is too small");
		return 0;
	}
	plain_size = xminiz_read_u32le(src);
	if ( plain_size > 0x7e000000u ) {
		xminiz_set_error("decompressed size is too large");
		return 0;
	}
	*out = (unsigned char*)malloc(plain_size + 1u);
	if ( *out == NULL ) {
		xminiz_set_error("out of memory");
		return 0;
	}
	written = (mz_ulong)plain_size;
	status = mz_uncompress(*out, &written, src + XMINIZ_HEADER_SIZE, (mz_ulong)(src_size - XMINIZ_HEADER_SIZE));
	if ( status != MZ_OK || (size_t)written != plain_size ) {
		free(*out);
		*out = NULL;
		xminiz_set_error(status == MZ_OK ? "miniz decompression size mismatch" : mz_error(status));
		return 0;
	}
	*out_size = plain_size;
	xminiz_set_error("");
	return 1;
}

XMINIZ_EXPORT int xminiz_available(void)
{
	return 1;
}

XMINIZ_EXPORT const char* xminiz_version(void)
{
	return mz_version();
}

XMINIZ_EXPORT const char* xminiz_last_error(void)
{
	return g_xminiz_error;
}

XMINIZ_EXPORT const char* xminiz_compress_hex_level(const char* text, int level)
{
	unsigned char* packed = NULL;
	size_t packed_size = 0u;
	char* hex;
	if ( text == NULL ) text = "";
	if ( !xminiz_compress_bytes((const unsigned char*)text, strlen(text), level, &packed, &packed_size) ) return "";
	hex = xminiz_hex_encode(packed, packed_size);
	free(packed);
	if ( hex == NULL ) {
		xminiz_set_error("out of memory");
		return "";
	}
	return xminiz_store_result(hex);
}

XMINIZ_EXPORT const char* xminiz_compress_hex(const char* text)
{
	return xminiz_compress_hex_level(text, MZ_DEFAULT_COMPRESSION);
}

XMINIZ_EXPORT const char* xminiz_decompress_hex(const char* hex_text)
{
	unsigned char* packed;
	unsigned char* plain = NULL;
	size_t packed_size = 0u;
	size_t plain_size = 0u;
	char* text;
	packed = xminiz_hex_decode(hex_text, &packed_size);
	if ( packed == NULL ) return "";
	if ( !xminiz_decompress_bytes(packed, packed_size, &plain, &plain_size) ) {
		free(packed);
		return "";
	}
	free(packed);
	text = (char*)malloc(plain_size + 1u);
	if ( text == NULL ) {
		free(plain);
		xminiz_set_error("out of memory");
		return "";
	}
	memcpy(text, plain, plain_size);
	text[plain_size] = '\0';
	free(plain);
	return xminiz_store_result(text);
}

XMINIZ_EXPORT int xminiz_compress_file_level(const char* input_path, const char* output_path, int level)
{
	unsigned char* src;
	unsigned char* packed = NULL;
	size_t src_size = 0u;
	size_t packed_size = 0u;
	int ok;
	src = xminiz_read_file(input_path, &src_size);
	if ( src == NULL ) return 0;
	ok = xminiz_compress_bytes(src, src_size, level, &packed, &packed_size);
	free(src);
	if ( !ok ) return 0;
	ok = xminiz_write_file(output_path, packed, packed_size);
	free(packed);
	return ok;
}

XMINIZ_EXPORT int xminiz_compress_file(const char* input_path, const char* output_path)
{
	return xminiz_compress_file_level(input_path, output_path, MZ_DEFAULT_COMPRESSION);
}

XMINIZ_EXPORT int xminiz_decompress_file(const char* input_path, const char* output_path)
{
	unsigned char* src;
	unsigned char* plain = NULL;
	size_t src_size = 0u;
	size_t plain_size = 0u;
	int ok;
	src = xminiz_read_file(input_path, &src_size);
	if ( src == NULL ) return 0;
	ok = xminiz_decompress_bytes(src, src_size, &plain, &plain_size);
	free(src);
	if ( !ok ) return 0;
	ok = xminiz_write_file(output_path, plain, plain_size);
	free(plain);
	return ok;
}

XMINIZ_EXPORT int xminiz_zip_write_text(const char* zip_path, const char* entry_name, const char* text)
{
	mz_zip_archive zip;
	mz_bool ok;
	if ( zip_path == NULL || zip_path[0] == '\0' || entry_name == NULL || entry_name[0] == '\0' ) {
		xminiz_set_error("empty zip path or entry name");
		return 0;
	}
	if ( text == NULL ) text = "";
	memset(&zip, 0, sizeof(zip));
	if ( !mz_zip_writer_init_file(&zip, zip_path, 0) ) {
		xminiz_set_error("failed to open zip for writing");
		return 0;
	}
	ok = mz_zip_writer_add_mem(&zip, entry_name, text, strlen(text), MZ_DEFAULT_COMPRESSION);
	if ( ok ) ok = mz_zip_writer_finalize_archive(&zip);
	mz_zip_writer_end(&zip);
	if ( !ok ) {
		xminiz_set_error("failed to write zip archive");
		return 0;
	}
	xminiz_set_error("");
	return 1;
}

XMINIZ_EXPORT int xminiz_zip_count(const char* zip_path)
{
	mz_zip_archive zip;
	int count;
	if ( zip_path == NULL || zip_path[0] == '\0' ) {
		xminiz_set_error("empty zip path");
		return 0;
	}
	memset(&zip, 0, sizeof(zip));
	if ( !mz_zip_reader_init_file(&zip, zip_path, 0) ) {
		xminiz_set_error("failed to open zip for reading");
		return 0;
	}
	count = (int)mz_zip_reader_get_num_files(&zip);
	mz_zip_reader_end(&zip);
	xminiz_set_error("");
	return count;
}

XMINIZ_EXPORT const char* xminiz_zip_first_name(const char* zip_path)
{
	mz_zip_archive zip;
	mz_uint needed;
	char* name;
	if ( zip_path == NULL || zip_path[0] == '\0' ) {
		xminiz_set_error("empty zip path");
		return "";
	}
	memset(&zip, 0, sizeof(zip));
	if ( !mz_zip_reader_init_file(&zip, zip_path, 0) ) {
		xminiz_set_error("failed to open zip for reading");
		return "";
	}
	if ( mz_zip_reader_get_num_files(&zip) == 0u ) {
		mz_zip_reader_end(&zip);
		xminiz_set_error("zip archive is empty");
		return "";
	}
	needed = mz_zip_reader_get_filename(&zip, 0, NULL, 0);
	name = (char*)malloc((size_t)needed + 1u);
	if ( name == NULL ) {
		mz_zip_reader_end(&zip);
		xminiz_set_error("out of memory");
		return "";
	}
	if ( mz_zip_reader_get_filename(&zip, 0, name, needed) == 0u ) {
		free(name);
		mz_zip_reader_end(&zip);
		xminiz_set_error("failed to read zip filename");
		return "";
	}
	mz_zip_reader_end(&zip);
	xminiz_set_error("");
	return xminiz_store_result(name);
}

XMINIZ_EXPORT const char* xminiz_zip_read_text(const char* zip_path, const char* entry_name)
{
	mz_zip_archive zip;
	size_t size = 0u;
	void* data;
	char* text;
	if ( zip_path == NULL || zip_path[0] == '\0' || entry_name == NULL || entry_name[0] == '\0' ) {
		xminiz_set_error("empty zip path or entry name");
		return "";
	}
	memset(&zip, 0, sizeof(zip));
	if ( !mz_zip_reader_init_file(&zip, zip_path, 0) ) {
		xminiz_set_error("failed to open zip for reading");
		return "";
	}
	data = mz_zip_reader_extract_file_to_heap(&zip, entry_name, &size, 0);
	mz_zip_reader_end(&zip);
	if ( data == NULL ) {
		xminiz_set_error("failed to read zip entry");
		return "";
	}
	text = (char*)malloc(size + 1u);
	if ( text == NULL ) {
		mz_free(data);
		xminiz_set_error("out of memory");
		return "";
	}
	memcpy(text, data, size);
	text[size] = '\0';
	mz_free(data);
	xminiz_set_error("");
	return xminiz_store_result(text);
}
