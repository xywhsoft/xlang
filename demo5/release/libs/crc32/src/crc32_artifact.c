#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#define XCRC32_EXPORT __declspec(dllexport)
#else
#define XCRC32_EXPORT
#endif

#if defined(__GNUC__)
#define XCRC32_THREAD __thread
#elif defined(_MSC_VER)
#define XCRC32_THREAD __declspec(thread)
#else
#define XCRC32_THREAD
#endif

static XCRC32_THREAD char g_xcrc32_error[512];
static XCRC32_THREAD char g_xcrc32_hex[9];
static uint32_t g_xcrc32_table[256];
static int g_xcrc32_table_ready = 0;

static void xcrc32_set_error(const char* text)
{
	if ( text == NULL ) text = "";
	strncpy(g_xcrc32_error, text, sizeof(g_xcrc32_error) - 1u);
	g_xcrc32_error[sizeof(g_xcrc32_error) - 1u] = '\0';
}

static void xcrc32_init_table(void)
{
	uint32_t i;
	if ( g_xcrc32_table_ready ) return;
	for ( i = 0u; i < 256u; ++i ) {
		uint32_t crc = i;
		int bit;
		for ( bit = 0; bit < 8; ++bit ) {
			if ( (crc & 1u) != 0u ) crc = (crc >> 1) ^ 0xedb88320u;
			else crc >>= 1;
		}
		g_xcrc32_table[i] = crc;
	}
	g_xcrc32_table_ready = 1;
}

static uint32_t xcrc32_update_value(uint32_t crc, const unsigned char* data, size_t size)
{
	size_t i;
	xcrc32_init_table();
	for ( i = 0u; i < size; ++i ) {
		crc = g_xcrc32_table[(crc ^ (uint32_t)data[i]) & 0xffu] ^ (crc >> 8);
	}
	return crc;
}

static uint32_t xcrc32_string_value(const char* text)
{
	if ( text == NULL ) text = "";
	return xcrc32_update_value(0xffffffffu, (const unsigned char*)text, strlen(text)) ^ 0xffffffffu;
}

static const char* xcrc32_format(uint32_t value)
{
	snprintf(g_xcrc32_hex, sizeof(g_xcrc32_hex), "%08lx", (unsigned long)value);
	xcrc32_set_error("");
	return g_xcrc32_hex;
}

XCRC32_EXPORT const char* xcrc32_version(void)
{
	return "crc32 0.1.0";
}

XCRC32_EXPORT const char* xcrc32_last_error(void)
{
	return g_xcrc32_error;
}

XCRC32_EXPORT int xcrc32_init(void)
{
	xcrc32_set_error("");
	return (int)0xffffffffu;
}

XCRC32_EXPORT int xcrc32_update(int crc, const char* text)
{
	uint32_t value = (uint32_t)crc;
	if ( text == NULL ) text = "";
	xcrc32_set_error("");
	return (int)xcrc32_update_value(value, (const unsigned char*)text, strlen(text));
}

XCRC32_EXPORT int xcrc32_finish(int crc)
{
	xcrc32_set_error("");
	return (int)((uint32_t)crc ^ 0xffffffffu);
}

XCRC32_EXPORT int xcrc32_string(const char* text)
{
	xcrc32_set_error("");
	return (int)xcrc32_string_value(text);
}

XCRC32_EXPORT const char* xcrc32_string_hex(const char* text)
{
	return xcrc32_format(xcrc32_string_value(text));
}

XCRC32_EXPORT int xcrc32_file(const char* path)
{
	unsigned char buffer[32768];
	uint32_t crc = 0xffffffffu;
	FILE* fp;
	size_t n;
	if ( path == NULL || path[0] == '\0' ) {
		xcrc32_set_error("empty file path");
		return 0;
	}
	fp = fopen(path, "rb");
	if ( fp == NULL ) {
		xcrc32_set_error("failed to open file");
		return 0;
	}
	while ( (n = fread(buffer, 1u, sizeof(buffer), fp)) > 0u ) {
		crc = xcrc32_update_value(crc, buffer, n);
	}
	if ( ferror(fp) ) {
		fclose(fp);
		xcrc32_set_error("failed to read file");
		return 0;
	}
	fclose(fp);
	xcrc32_set_error("");
	return (int)(crc ^ 0xffffffffu);
}

XCRC32_EXPORT const char* xcrc32_file_hex(const char* path)
{
	int value = xcrc32_file(path);
	if ( g_xcrc32_error[0] != '\0' ) return "";
	return xcrc32_format((uint32_t)value);
}

XCRC32_EXPORT const char* xcrc32_hex(int crc)
{
	return xcrc32_format((uint32_t)crc);
}
