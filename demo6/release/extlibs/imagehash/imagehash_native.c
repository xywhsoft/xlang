#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STBI_NO_SIMD
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"

#define STBIR_NO_SIMD
#define STB_IMAGE_RESIZE_STATIC
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "third_party/stb_image_resize2.h"

#if defined(_WIN32) || defined(_WIN64)
#define XIHASH_EXPORT __declspec(dllexport)
#else
#define XIHASH_EXPORT
#endif

#if defined(__GNUC__)
#define XIHASH_THREAD __thread
#elif defined(_MSC_VER)
#define XIHASH_THREAD __declspec(thread)
#else
#define XIHASH_THREAD
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static XIHASH_THREAD char g_xihash_error[512];
#define XIHASH_HEX_SLOTS 8u

static XIHASH_THREAD char g_xihash_hex[XIHASH_HEX_SLOTS][17];
static XIHASH_THREAD size_t g_xihash_hex_index;

static void xihash_set_error(const char* text)
{
    size_t i = 0u;

    if ( text == NULL ) {
        text = "";
    }

    while ( i + 1u < sizeof(g_xihash_error) && text[i] != '\0' ) {
        g_xihash_error[i] = text[i];
        ++i;
    }
    g_xihash_error[i] = '\0';
}

static const char* xihash_format(uint64_t value)
{
	snprintf(g_xihash_hex, sizeof(g_xihash_hex), "%016llx", (unsigned long long)value);
	xihash_set_error("");
	return g_xihash_hex;
}

static unsigned char* xihash_load_resized_gray(const char* path, int width, int height)
{
	int src_width = 0;
	int src_height = 0;
	int src_channels = 0;
	unsigned char* src;
	unsigned char* dst;
	if ( path == NULL || path[0] == '\0' ) {
		xihash_set_error("empty image path");
		return NULL;
	}
	src = stbi_load(path, &src_width, &src_height, &src_channels, 1);
	if ( src == NULL ) {
		xihash_set_error(stbi_failure_reason());
		return NULL;
	}
	dst = (unsigned char*)malloc((size_t)width * (size_t)height);
	if ( dst == NULL ) {
		stbi_image_free(src);
		xihash_set_error("out of memory");
		return NULL;
	}
	if ( stbir_resize_uint8_linear(src, src_width, src_height, 0, dst, width, height, 0, STBIR_1CHANNEL) == NULL ) {
		free(dst);
		stbi_image_free(src);
		xihash_set_error("resize failed");
		return NULL;
	}
	stbi_image_free(src);
	return dst;
}

static uint64_t xihash_ahash_value(const unsigned char* pixels)
{
	uint64_t hash = 0u;
	unsigned int sum = 0u;
	unsigned int avg;
	int i;
	for ( i = 0; i < 64; ++i ) sum += pixels[i];
	avg = sum / 64u;
	for ( i = 0; i < 64; ++i ) {
		if ( pixels[i] >= avg ) hash |= ((uint64_t)1u << i);
	}
	return hash;
}

static uint64_t xihash_dhash_value(const unsigned char* pixels)
{
	uint64_t hash = 0u;
	int x;
	int y;
	int bit = 0;
	for ( y = 0; y < 8; ++y ) {
		for ( x = 0; x < 8; ++x ) {
			if ( pixels[y * 9 + x] > pixels[y * 9 + x + 1] ) hash |= ((uint64_t)1u << bit);
			++bit;
		}
	}
	return hash;
}

static double xihash_dct_coeff(const unsigned char* pixels, int u, int v)
{
	double sum = 0.0;
	int x;
	int y;
	for ( y = 0; y < 32; ++y ) {
		for ( x = 0; x < 32; ++x ) {
			double pixel = (double)pixels[y * 32 + x];
			double cx = cos(((2.0 * (double)x + 1.0) * (double)u * M_PI) / 64.0);
			double cy = cos(((2.0 * (double)y + 1.0) * (double)v * M_PI) / 64.0);
			sum += pixel * cx * cy;
		}
	}
	return sum;
}

static int xihash_double_compare(const void* left, const void* right)
{
	double a = *(const double*)left;
	double b = *(const double*)right;
	if ( a < b ) return -1;
	if ( a > b ) return 1;
	return 0;
}

static uint64_t xihash_phash_value(const unsigned char* pixels)
{
	double coeffs[64];
	double sorted[63];
	double median;
	uint64_t hash = 0u;
	int u;
	int v;
	int i = 0;
	int j = 0;
	for ( v = 0; v < 8; ++v ) {
		for ( u = 0; u < 8; ++u ) {
			coeffs[i++] = xihash_dct_coeff(pixels, u, v);
		}
	}
	for ( i = 1; i < 64; ++i ) sorted[j++] = coeffs[i];
	qsort(sorted, 63u, sizeof(double), xihash_double_compare);
	median = sorted[31];
	for ( i = 1; i < 64; ++i ) {
		if ( coeffs[i] > median ) hash |= ((uint64_t)1u << (i - 1));
	}
	return hash;
}

static int xihash_hex_value(int ch)
{
	if ( ch >= '0' && ch <= '9' ) return ch - '0';
	if ( ch >= 'a' && ch <= 'f' ) return ch - 'a' + 10;
	if ( ch >= 'A' && ch <= 'F' ) return ch - 'A' + 10;
	return -1;
}

static int xihash_parse_hex64(const char* text, uint64_t* out)
{
	uint64_t value = 0u;
	int digits = 0;
	if ( text == NULL ) return 0;
	if ( text[0] == '0' && (text[1] == 'x' || text[1] == 'X') ) text += 2;
	while ( *text != '\0' ) {
		int part;
		if ( isspace((unsigned char)*text) ) {
			++text;
			continue;
		}
		part = xihash_hex_value((unsigned char)*text);
		if ( part < 0 || digits >= 16 ) return 0;
		value = (value << 4) | (uint64_t)part;
		++digits;
		++text;
	}
	if ( digits == 0 ) return 0;
	*out = value;
	return 1;
}

static int xihash_popcount64(uint64_t value)
{
	int count = 0;
	while ( value != 0u ) {
		value &= value - 1u;
		++count;
	}
	return count;
}

XIHASH_EXPORT int xihash_available(void)
{
	return 1;
}

XIHASH_EXPORT const char* xihash_version(void)
{
	return "imagehash 0.1.0";
}

XIHASH_EXPORT const char* xihash_last_error(void)
{
	return g_xihash_error;
}

XIHASH_EXPORT const char* xihash_ahash(const char* path)
{
	unsigned char* pixels = xihash_load_resized_gray(path, 8, 8);
	uint64_t hash;
	if ( pixels == NULL ) return "";
	hash = xihash_ahash_value(pixels);
	free(pixels);
	return xihash_format(hash);
}

XIHASH_EXPORT const char* xihash_dhash(const char* path)
{
	unsigned char* pixels = xihash_load_resized_gray(path, 9, 8);
	uint64_t hash;
	if ( pixels == NULL ) return "";
	hash = xihash_dhash_value(pixels);
	free(pixels);
	return xihash_format(hash);
}

XIHASH_EXPORT const char* xihash_phash(const char* path)
{
	unsigned char* pixels = xihash_load_resized_gray(path, 32, 32);
	uint64_t hash;
	if ( pixels == NULL ) return "";
	hash = xihash_phash_value(pixels);
	free(pixels);
	return xihash_format(hash);
}

XIHASH_EXPORT int xihash_hamming(const char* left, const char* right)
{
	uint64_t a = 0u;
	uint64_t b = 0u;
	if ( !xihash_parse_hex64(left, &a) || !xihash_parse_hex64(right, &b) ) {
		xihash_set_error("invalid hash string");
		return -1;
	}
	xihash_set_error("");
	return xihash_popcount64(a ^ b);
}

XIHASH_EXPORT int xihash_valid(const char* hash)
{
	uint64_t value = 0u;
	return xihash_parse_hex64(hash, &value);
}
