#include "include/xrt_dotenv.h"
#include "xrt.h"

#include <stdint.h>

/* XLang 的 point/string ABI 只在这一层转换；解析核心可由 C 独立使用。 */

void* xdotenv_create(void)
{
	return xrtDotenvCreate();
}

void xdotenv_destroy(void* handle)
{
	xrtDotenvDestroy((xrtdotenv*)handle);
}

bool xdotenv_parse(void* handle, const char* text, size_t size, size_t maxBytes)
{
	return xrtDotenvParse((xrtdotenv*)handle, text, size, maxBytes);
}

bool xdotenv_load(void* handle, const char* path, size_t maxBytes)
{
	bool ok = xrtDotenvLoad((xrtdotenv*)handle, path, maxBytes);
	if (!ok && xrtDotenvErrorCode((xrtdotenv*)handle) != XRT_DOTENV_MEMORY)
		xrtClearError();
	return ok;
}

bool xdotenv_set(void* handle, const char* key, const char* value)
{
	return xrtDotenvSet((xrtdotenv*)handle, key, value);
}

const char* xdotenv_get(void* handle, const char* key)
{
	return xrtDotenvGet((xrtdotenv*)handle, key);
}

bool xdotenv_has(void* handle, const char* key)
{
	return xrtDotenvHas((xrtdotenv*)handle, key);
}

int64_t xdotenv_count(void* handle)
{
	return (int64_t)xrtDotenvCount((xrtdotenv*)handle);
}

void xdotenv_clear(void* handle)
{
	xrtDotenvClear((xrtdotenv*)handle);
}

bool xdotenv_apply(void* handle, bool overwrite)
{
	bool ok = xrtDotenvApply((xrtdotenv*)handle, overwrite);
	if (!ok && xrtDotenvErrorCode((xrtdotenv*)handle) != XRT_DOTENV_MEMORY)
		xrtClearError();
	return ok;
}

int64_t xdotenv_error_code(void* handle)
{
	return (int64_t)xrtDotenvErrorCode((xrtdotenv*)handle);
}

int64_t xdotenv_error_line(void* handle)
{
	return (int64_t)xrtDotenvErrorLine((xrtdotenv*)handle);
}

const char* xdotenv_error_text(void* handle)
{
	return xrtDotenvErrorText((xrtdotenv*)handle);
}
