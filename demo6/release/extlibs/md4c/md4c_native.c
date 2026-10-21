#include <string.h>

#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_FILE_WHOLE
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#include "xrt.h"

#include "third_party/md4c/md4c-html.h"
#include "md4c_native.h"

#if defined(_WIN32) || defined(_WIN64)
#define XMD4C_EXPORT __declspec(dllexport)
#else
#define XMD4C_EXPORT
#endif

#define XMD4C_DOMAIN "xlang.extlib.md4c"
#define XMD4C_INPUT_LIMIT (64u * 1024u * 1024u)
#define XMD4C_PARSER_FLAGS (MD_FLAG_COLLAPSEWHITESPACE | \
	MD_FLAG_PERMISSIVEATXHEADERS | MD_FLAG_PERMISSIVEURLAUTOLINKS | \
	MD_FLAG_PERMISSIVEEMAILAUTOLINKS | MD_FLAG_NOINDENTEDCODEBLOCKS | \
	MD_FLAG_NOHTMLBLOCKS | MD_FLAG_NOHTMLSPANS | MD_FLAG_TABLES | \
	MD_FLAG_STRIKETHROUGH | MD_FLAG_PERMISSIVEWWWAUTOLINKS | \
	MD_FLAG_TASKLISTS | MD_FLAG_LATEXMATHSPANS | MD_FLAG_WIKILINKS | \
	MD_FLAG_UNDERLINE | MD_FLAG_HARD_SOFT_BREAKS)
#define XMD4C_RENDER_FLAGS (MD_HTML_FLAG_DEBUG | \
	MD_HTML_FLAG_VERBATIM_ENTITIES | MD_HTML_FLAG_SKIP_UTF8_BOM | \
	MD_HTML_FLAG_XHTML)

typedef struct {
	xbuffer Buffer;
	bool Failed;
} xmd4c_output;

static void xmd4c_fail(xerrkind kind, const char* message)
{
	if ( xrtGetError() == NULL ) xrtSetErrorInfo(kind, XMD4C_DOMAIN, 0, message);
}

static bool xmd4c_flags_valid(int parser_flags, int renderer_flags)
{
	if ( parser_flags < 0 || renderer_flags < 0 ||
		(((unsigned)parser_flags & ~(unsigned)XMD4C_PARSER_FLAGS) != 0u) ||
		(((unsigned)renderer_flags & ~(unsigned)XMD4C_RENDER_FLAGS) != 0u) ) {
		xmd4c_fail(XERR_VALUE, "md4c flags contain unsupported bits");
		return false;
	}
	return true;
}

static void xmd4c_append_html(const MD_CHAR* text, MD_SIZE size, void* userdata)
{
	xmd4c_output* output = (xmd4c_output*)userdata;

	if ( output == NULL || output->Failed ) return;
	if ( text == NULL ||
		 !xrtBufferAppend(&output->Buffer, (xbytesview){ (cbytes)text, (size_t)size }) ) {
		output->Failed = true;
		xmd4c_fail(XERR_MEMORY, "could not grow HTML output");
	}
}

static void* xmd4c_store_result(xbuffer* buffer)
{
	void* result;
	if ( !xrtBufferAppendByte(buffer, 0u) ) {
		xmd4c_fail(XERR_MEMORY, "could not terminate HTML output");
		xrtBufferUnit(buffer);
		return NULL;
	}
	result = xrtBufferTake(buffer, NULL, NULL);
	xrtBufferUnit(buffer);
	if ( result == NULL ) {
		xmd4c_fail(XERR_MEMORY, "could not take HTML output");
		return NULL;
	}
	xrtClearError();
	return result;
}

static void* xmd4c_render(const char* markdown, size_t size, unsigned parser_flags, unsigned renderer_flags)
{
	xmd4c_output output;
	int rc;

	xrtClearError();
	output.Failed = false;
	if ( markdown == NULL ) {
		markdown = "";
		size = 0u;
	}
	if ( size > XMD4C_INPUT_LIMIT || size > (size_t)((MD_SIZE)-1) ) {
		xmd4c_fail(XERR_RANGE, "markdown input exceeds 64 MiB");
		return NULL;
	}
	if ( memchr(markdown, '\0', size) != NULL ) {
		xmd4c_fail(XERR_VALUE, "markdown input contains NUL");
		return NULL;
	}
	if ( !xrtUtf8Valid((xstrview){ markdown, size }, NULL) ) {
		xmd4c_fail(XERR_VALUE, "markdown input is not valid UTF-8");
		return NULL;
	}
	if ( !xrtBufferInit(&output.Buffer) ) {
		xmd4c_fail(XERR_MEMORY, "could not initialize HTML output");
		return NULL;
	}
	rc = md_html((const MD_CHAR*)markdown, (MD_SIZE)size,
		xmd4c_append_html, &output, parser_flags, renderer_flags);
	if ( rc != 0 || output.Failed ) {
		xrtBufferUnit(&output.Buffer);
		if ( rc != 0 ) xmd4c_fail(XERR_VALUE, "md4c parse failed");
		return NULL;
	}
	return xmd4c_store_result(&output.Buffer);
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
	const xerror* error = xrtGetError();
	return error != NULL ? xrtErrorMessage(error) : "";
}

static void* xmd4c_render_string(const char* markdown, int parser_flags, int renderer_flags)
{
	size_t size = 0u;
	xrtClearError();
	if ( !xmd4c_flags_valid(parser_flags, renderer_flags) ) return NULL;
	if ( markdown != NULL )
		while ( size <= XMD4C_INPUT_LIMIT && markdown[size] != '\0' ) ++size;
	return xmd4c_render(markdown, size,
		(unsigned)parser_flags, (unsigned)renderer_flags);
}

XMD4C_EXPORT void* xmd4c_html(const char* markdown)
{
	return xmd4c_render_string(markdown, MD_DIALECT_COMMONMARK, 0u);
}

XMD4C_EXPORT void* xmd4c_html_flags(const char* markdown, int parser_flags, int renderer_flags)
{
	return xmd4c_render_string(markdown, parser_flags, renderer_flags);
}

XMD4C_EXPORT void* xmd4c_file_html(const char* path, int parser_flags, int renderer_flags)
{
	bytes data;
	size_t size;
	void* result;

	xrtClearError();
	if ( !xmd4c_flags_valid(parser_flags, renderer_flags) ) return NULL;
	if ( path == NULL || path[0] == '\0' ) {
		xmd4c_fail(XERR_ARGUMENT, "empty file path");
		return NULL;
	}
	data = xrtFileReadAllLimit(path, XMD4C_INPUT_LIMIT, &size);
	if ( data == NULL ) {
		xmd4c_fail(XERR_IO, "failed to read file");
		return NULL;
	}
	result = xmd4c_render((const char*)data, size, (unsigned)parser_flags, (unsigned)renderer_flags);
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
