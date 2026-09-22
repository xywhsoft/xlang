#include <stdint.h>
#include <string.h>

#define XRT_MODULE_BUFFER
#define XRT_MODULE_FILE
#include "xrt.h"

#include "third_party/md4c/md4c-html.h"

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

static XMD4C_THREAD char g_xmd4c_error[512];

#define XMD4C_RESULT_SLOTS 8u

static XMD4C_THREAD xbuffer g_xmd4c_html[XMD4C_RESULT_SLOTS];
static XMD4C_THREAD size_t g_xmd4c_html_index;

static void xmd4c_set_error(const char* text)
{
    size_t i = 0u;

    if ( text == NULL ) {
        text = "";
    }

    while ( i + 1u < sizeof(g_xmd4c_error) && text[i] != '\0' ) {
        g_xmd4c_error[i] = text[i];
        ++i;
    }
    g_xmd4c_error[i] = '\0';
}

static void xmd4c_append_html(const MD_CHAR* text, MD_SIZE size, void* userdata)
{
	xbuffer* buffer = (xbuffer*)userdata;

	if ( buffer == NULL || text == NULL ||
		 !xrtBufferAppend(buffer, (xbytesview){ (cbytes)text, (size_t)size }) ) {
		xmd4c_set_error("out of memory");
	}
}

static const char* xmd4c_store_result(xbuffer* buffer)
{
	size_t slot = g_xmd4c_html_index % XMD4C_RESULT_SLOTS;
	xbuffer* output = &g_xmd4c_html[slot];

	g_xmd4c_html_index = (g_xmd4c_html_index + 1u) % XMD4C_RESULT_SLOTS;
	if ( !xrtBufferAppendByte(buffer, 0u) ) {
		xmd4c_set_error("out of memory");
		xrtBufferUnit(buffer);
		return "";
	}
	xrtBufferUnit(output);
	*output = *buffer;
	memset(buffer, 0, sizeof(*buffer));
	xmd4c_set_error("");
	return (const char*)output->Data;
}

static const char* xmd4c_render(const char* markdown, size_t size, unsigned parser_flags, unsigned renderer_flags)
{
	xbuffer buffer;
	int rc;

	if ( !xrtBufferInit(&buffer) ) {
		xmd4c_set_error("out of memory");
		return "";
	}
	xmd4c_set_error("");
	if ( markdown == NULL ) {
		markdown = "";
		size = 0u;
	}
	if ( size > (size_t)((MD_SIZE)-1) ) {
		xmd4c_set_error("markdown input is too large");
		xrtBufferUnit(&buffer);
		return "";
	}
	rc = md_html((const MD_CHAR*)markdown, (MD_SIZE)size, xmd4c_append_html, &buffer, parser_flags, renderer_flags);
	if ( rc != 0 || g_xmd4c_error[0] != '\0' ) {
		xrtBufferUnit(&buffer);
		if ( rc != 0 ) xmd4c_set_error("md4c parse failed");
		return "";
	}
	return xmd4c_store_result(&buffer);
}

XMD4C_EXPORT int xmd4c_available(void)
{
	return 1;
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
	bytes data;
	size_t size;
	const char* result;

	if ( path == NULL || path[0] == '\0' ) {
		xmd4c_set_error("empty file path");
		return "";
	}
	data = xrtFileReadAll(path, &size);
	if ( data == NULL ) {
		xmd4c_set_error("failed to read file");
		return "";
	}
	result = xmd4c_render(data, size, (unsigned)parser_flags, (unsigned)renderer_flags);
	xrtFree(data);
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

XMD4C_EXPORT int xmd4c_no_html_flag(void)
{
	return (int)MD_FLAG_NOHTML;
}
