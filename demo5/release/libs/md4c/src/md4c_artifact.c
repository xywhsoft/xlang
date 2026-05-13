#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "md4c/md4c-html.h"

#if defined(_WIN32) || defined(_WIN64)
#define XMD4C_EXPORT __declspec(dllexport)
#else
#define XMD4C_EXPORT
#endif

#if defined(__GNUC__)
#define XMD4C_THREAD __thread
#elif defined(_MSC_VER)
#define XMD4C_THREAD __declspec(thread)
#else
#define XMD4C_THREAD
#endif

typedef struct XMd4cBuffer {
	char* data;
	size_t size;
	size_t capacity;
	int failed;
} XMd4cBuffer;

static XMD4C_THREAD char g_xmd4c_error[512];
static XMD4C_THREAD char* g_xmd4c_html;
static XMD4C_THREAD size_t g_xmd4c_html_capacity;

static void xmd4c_set_error(const char* text)
{
	if ( text == NULL ) text = "";
	strncpy(g_xmd4c_error, text, sizeof(g_xmd4c_error) - 1u);
	g_xmd4c_error[sizeof(g_xmd4c_error) - 1u] = '\0';
}

static int xmd4c_buffer_reserve(XMd4cBuffer* buffer, size_t need)
{
	char* next;
	size_t capacity;
	if ( need <= buffer->capacity ) return 1;
	capacity = (buffer->capacity != 0u) ? buffer->capacity : 1024u;
	while ( capacity < need ) {
		if ( capacity > ((size_t)-1) / 2u ) {
			buffer->failed = 1;
			return 0;
		}
		capacity *= 2u;
	}
	next = (char*)realloc(buffer->data, capacity);
	if ( next == NULL ) {
		buffer->failed = 1;
		return 0;
	}
	buffer->data = next;
	buffer->capacity = capacity;
	return 1;
}

static void xmd4c_append_html(const MD_CHAR* text, MD_SIZE size, void* userdata)
{
	XMd4cBuffer* buffer = (XMd4cBuffer*)userdata;
	size_t n = (size_t)size;
	if ( buffer == NULL || buffer->failed ) return;
	if ( n > ((size_t)-1) - buffer->size - 1u ) {
		buffer->failed = 1;
		return;
	}
	if ( !xmd4c_buffer_reserve(buffer, buffer->size + n + 1u) ) return;
	memcpy(buffer->data + buffer->size, text, n);
	buffer->size += n;
	buffer->data[buffer->size] = '\0';
}

static const char* xmd4c_store_result(XMd4cBuffer* buffer)
{
	char* next;
	if ( buffer->data == NULL ) {
		if ( !xmd4c_buffer_reserve(buffer, 1u) ) return "";
		buffer->data[0] = '\0';
	}
	if ( buffer->size + 1u > g_xmd4c_html_capacity ) {
		next = (char*)realloc(g_xmd4c_html, buffer->size + 1u);
		if ( next == NULL ) {
			xmd4c_set_error("out of memory");
			return "";
		}
		g_xmd4c_html = next;
		g_xmd4c_html_capacity = buffer->size + 1u;
	}
	memcpy(g_xmd4c_html, buffer->data, buffer->size + 1u);
	xmd4c_set_error("");
	return g_xmd4c_html;
}

static const char* xmd4c_render(const char* markdown, size_t size, unsigned parser_flags, unsigned renderer_flags)
{
	XMd4cBuffer buffer;
	int rc;
	memset(&buffer, 0, sizeof(buffer));
	if ( markdown == NULL ) {
		markdown = "";
		size = 0u;
	}
	if ( size > (size_t)((MD_SIZE)-1) ) {
		xmd4c_set_error("markdown input is too large");
		return "";
	}
	rc = md_html((const MD_CHAR*)markdown, (MD_SIZE)size, xmd4c_append_html, &buffer, parser_flags, renderer_flags);
	if ( rc != 0 || buffer.failed ) {
		free(buffer.data);
		xmd4c_set_error((rc != 0) ? "md4c parse failed" : "out of memory");
		return "";
	}
	return xmd4c_store_result(&buffer);
}

static char* xmd4c_read_file(const char* path, size_t* out_size)
{
	FILE* fp;
	long end_pos;
	char* data;
	size_t read_size;
	if ( out_size != NULL ) *out_size = 0u;
	if ( path == NULL || path[0] == '\0' ) {
		xmd4c_set_error("empty file path");
		return NULL;
	}
	fp = fopen(path, "rb");
	if ( fp == NULL ) {
		xmd4c_set_error("failed to open file");
		return NULL;
	}
	if ( fseek(fp, 0, SEEK_END) != 0 ) {
		fclose(fp);
		xmd4c_set_error("failed to seek file");
		return NULL;
	}
	end_pos = ftell(fp);
	if ( end_pos < 0 ) {
		fclose(fp);
		xmd4c_set_error("failed to get file size");
		return NULL;
	}
	if ( fseek(fp, 0, SEEK_SET) != 0 ) {
		fclose(fp);
		xmd4c_set_error("failed to rewind file");
		return NULL;
	}
	data = (char*)malloc((size_t)end_pos + 1u);
	if ( data == NULL ) {
		fclose(fp);
		xmd4c_set_error("out of memory");
		return NULL;
	}
	read_size = fread(data, 1u, (size_t)end_pos, fp);
	if ( read_size != (size_t)end_pos || ferror(fp) ) {
		free(data);
		fclose(fp);
		xmd4c_set_error("failed to read file");
		return NULL;
	}
	fclose(fp);
	data[read_size] = '\0';
	if ( out_size != NULL ) *out_size = read_size;
	return data;
}

XMD4C_EXPORT const char* xmd4c_version(void)
{
	return "md4c 0.5.2";
}

XMD4C_EXPORT const char* xmd4c_last_error(void)
{
	return g_xmd4c_error;
}

XMD4C_EXPORT const char* xmd4c_html(const char* markdown)
{
	return xmd4c_render(markdown, (markdown != NULL) ? strlen(markdown) : 0u, MD_DIALECT_COMMONMARK, 0u);
}

XMD4C_EXPORT const char* xmd4c_html_flags(const char* markdown, int parser_flags, int renderer_flags)
{
	return xmd4c_render(markdown, (markdown != NULL) ? strlen(markdown) : 0u, (unsigned)parser_flags, (unsigned)renderer_flags);
}

XMD4C_EXPORT const char* xmd4c_file_html(const char* path, int parser_flags, int renderer_flags)
{
	char* data;
	size_t size;
	const char* result;
	data = xmd4c_read_file(path, &size);
	if ( data == NULL ) return "";
	result = xmd4c_render(data, size, (unsigned)parser_flags, (unsigned)renderer_flags);
	free(data);
	return result;
}

XMD4C_EXPORT int xmd4c_commonmark_flags(void)
{
	return (int)MD_DIALECT_COMMONMARK;
}

XMD4C_EXPORT int xmd4c_github_flags(void)
{
	return (int)MD_DIALECT_GITHUB;
}

XMD4C_EXPORT int xmd4c_xhtml_flag(void)
{
	return (int)MD_HTML_FLAG_XHTML;
}

XMD4C_EXPORT int xmd4c_skip_utf8_bom_flag(void)
{
	return (int)MD_HTML_FLAG_SKIP_UTF8_BOM;
}

XMD4C_EXPORT int xmd4c_verbatim_entities_flag(void)
{
	return (int)MD_HTML_FLAG_VERBATIM_ENTITIES;
}
