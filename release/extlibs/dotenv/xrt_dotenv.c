#include "include/xrt_dotenv.h"
#include "xrt.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct {
	char* Key;
	char* Value;
} xrt_dotenv_pair;

struct xrtdotenv {
	xmap Items;
	int ErrorCode;
	size_t ErrorLine;
	char ErrorText[192];
};

/* ---------- 对象与错误 ---------- */

static void dotenv_error(xrtdotenv* config, int code, size_t line,
	const char* format, ...)
{
	va_list args;
	config->ErrorCode = code;
	config->ErrorLine = line;
	va_start(args, format);
	vsnprintf(config->ErrorText, sizeof(config->ErrorText), format, args);
	va_end(args);
}

static void dotenv_clear_error(xrtdotenv* config)
{
	config->ErrorCode = XRT_DOTENV_OK;
	config->ErrorLine = 0;
	config->ErrorText[0] = '\0';
}

static void dotenv_drop(xbytesview key, ptr value, ptr user)
{
	xrt_dotenv_pair* pair = (xrt_dotenv_pair*)value;
	(void)key;
	(void)user;
	xrtFree(pair->Key);
	xrtFree(pair->Value);
}

static bool dotenv_init_items(xrtdotenv* config)
{
	if (!xrtMapInit(&config->Items, sizeof(xrt_dotenv_pair))) return false;
	if (!xrtMapSetDrop(&config->Items, dotenv_drop, NULL)) {
		xrtMapUnit(&config->Items);
		return false;
	}
	return true;
}

xrtdotenv* xrtDotenvCreate(void)
{
	xrtdotenv* config;
	/* 公开操作只报告本次调用的线程错误；错误查询与析构不擦除诊断。 */
	xrtClearError();
	config = (xrtdotenv*)xrtMalloc(sizeof(*config));
	if (config != NULL) {
		memset(config, 0, sizeof(*config));
		if (!dotenv_init_items(config)) {
			xrtFree(config);
			return NULL;
		}
	}
	return config;
}

void xrtDotenvDestroy(xrtdotenv* config)
{
	if (config == NULL) return;
	xrtMapUnit(&config->Items);
	xrtFree(config);
}

void xrtDotenvClear(xrtdotenv* config)
{
	xrtClearError();
	if (config == NULL) return;
	xrtMapClear(&config->Items);
	xrtMapTrim(&config->Items);
	dotenv_clear_error(config);
}

int xrtDotenvErrorCode(const xrtdotenv* config)
{
	return config != NULL ? config->ErrorCode : XRT_DOTENV_ARGUMENT;
}

size_t xrtDotenvErrorLine(const xrtdotenv* config)
{
	return config != NULL ? config->ErrorLine : 0;
}

const char* xrtDotenvErrorText(const xrtdotenv* config)
{
	return config != NULL && config->ErrorText[0] != '\0' ?
		config->ErrorText : "dotenv configuration is unavailable";
}

/* ---------- 键值存储 ---------- */

static bool dotenv_key_valid(const char* key)
{
	const unsigned char* p = (const unsigned char*)key;
	if (p == NULL || !((*p >= 'A' && *p <= 'Z') ||
		(*p >= 'a' && *p <= 'z') || *p == '_')) return false;
	for (++p; *p != '\0'; ++p) {
		if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
			(*p >= '0' && *p <= '9') || *p == '_')) return false;
	}
	return true;
}

static xbytesview dotenv_key_view(const char* key)
{
	return (xbytesview){(const unsigned char*)key, strlen(key)};
}

static bool dotenv_put_owned(xrtdotenv* config, char* key, char* value, size_t line)
{
	xrt_dotenv_pair pair = {key, value};
	if (!xrtMapSet(&config->Items, dotenv_key_view(key), &pair)) {
		const xerror* error = xrtGetError();
		int code = error != NULL && xrtErrorKind(error) == XERR_RANGE ?
			XRT_DOTENV_LIMIT : XRT_DOTENV_MEMORY;
		dotenv_error(config, code, line, "cannot store dotenv entry");
		return false;
	}
	return true;
}

bool xrtDotenvSet(xrtdotenv* config, const char* key, const char* value)
{
	char* name;
	char* text;
	xrtClearError();
	if (config == NULL) return false;
	dotenv_clear_error(config);
	if (!dotenv_key_valid(key) || value == NULL ||
		!xrtUtf8Valid(xrtStrView(value), NULL)) {
		dotenv_error(config, XRT_DOTENV_ARGUMENT, 0, "invalid dotenv key or value");
		return false;
	}
	name = xrtStrDup(key);
	text = xrtStrDup(value);
	if (name == NULL || text == NULL) {
		xrtFree(name);
		xrtFree(text);
		dotenv_error(config, XRT_DOTENV_MEMORY, 0, "cannot allocate dotenv value");
		return false;
	}
	if (!dotenv_put_owned(config, name, text, 0)) {
		xrtFree(name);
		xrtFree(text);
		return false;
	}
	return true;
}

const char* xrtDotenvGet(const xrtdotenv* config, const char* key)
{
	const xrt_dotenv_pair* pair;
	xrtClearError();
	if (config == NULL || key == NULL) return NULL;
	pair = (const xrt_dotenv_pair*)xrtMapConstGet(&config->Items,
		dotenv_key_view(key));
	return pair != NULL ? pair->Value : NULL;
}

bool xrtDotenvHas(const xrtdotenv* config, const char* key)
{
	xrtClearError();
	return config != NULL && key != NULL &&
		xrtMapHas(&config->Items, dotenv_key_view(key));
}

size_t xrtDotenvCount(const xrtdotenv* config)
{
	xrtClearError();
	return config != NULL ? xrtMapCount(&config->Items) : 0;
}

/* ---------- UTF-8 文本解析 ---------- */

static bool dotenv_space(char c)
{
	return c == ' ' || c == '\t';
}

static bool dotenv_line(xrtdotenv* config, const char* start, const char* end,
	size_t line)
{
	const char* p = start;
	const char* nameStart;
	const char* nameEnd;
	char* name = NULL;
	char* value = NULL;
	size_t used = 0;
	while (p < end && dotenv_space(*p)) ++p;
	if (p == end || *p == '#') return true;
	if (end - p >= 7 && memcmp(p, "export", 6) == 0 && dotenv_space(p[6])) {
		p += 7;
		while (p < end && dotenv_space(*p)) ++p;
	}
	nameStart = p;
	if (p == end || !((*p >= 'A' && *p <= 'Z') ||
		(*p >= 'a' && *p <= 'z') || *p == '_')) goto invalid;
	for (++p; p < end && ((*p >= 'A' && *p <= 'Z') ||
		(*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_'); ++p) {}
	nameEnd = p;
	while (p < end && dotenv_space(*p)) ++p;
	if (p == end || *p++ != '=') goto invalid;
	while (p < end && dotenv_space(*p)) ++p;
	name = xrtStrDupN(nameStart, (size_t)(nameEnd - nameStart));
	value = (char*)xrtMalloc((size_t)(end - p) + 1u);
	if (name == NULL || value == NULL) goto memory;
	if (p < end && (*p == '\'' || *p == '"')) {
		char quote = *p++;
		bool closed = false;
		while (p < end) {
			char c = *p++;
			if (c == quote) { closed = true; break; }
			if (quote == '"' && c == '\\') {
				if (p == end) goto invalid;
				c = *p++;
				if (c == 'n') c = '\n';
				else if (c == 'r') c = '\r';
				else if (c == 't') c = '\t';
				else if (c != '"' && c != '\\') goto invalid;
			}
			value[used++] = c;
		}
		if (!closed) goto invalid;
		while (p < end && dotenv_space(*p)) ++p;
		if (p < end && *p != '#') goto invalid;
	} else {
		while (p < end) {
			if (*p == '#' && (p == start || dotenv_space(p[-1]))) break;
			value[used++] = *p++;
		}
		while (used != 0 && dotenv_space(value[used - 1])) --used;
	}
	value[used] = '\0';
	if (!dotenv_put_owned(config, name, value, line)) {
		xrtFree(name);
		xrtFree(value);
		return false;
	}
	return true;
invalid:
	xrtFree(name);
	xrtFree(value);
	dotenv_error(config, XRT_DOTENV_FORMAT, line, "invalid dotenv syntax at line %zu", line);
	return false;
memory:
	xrtFree(name);
	xrtFree(value);
	dotenv_error(config, XRT_DOTENV_MEMORY, line, "cannot allocate dotenv input");
	return false;
}

bool xrtDotenvParse(xrtdotenv* config, const char* text, size_t size, size_t maxBytes)
{
	xrtdotenv parsed = {0};
	size_t offset = 0;
	size_t line = 1;
	xrtClearError();
	if (config == NULL) return false;
	dotenv_clear_error(config);
	if (text == NULL) {
		dotenv_error(config, XRT_DOTENV_ARGUMENT, 0, "dotenv input contains NUL or is null");
		return false;
	}
	if (size > maxBytes) {
		dotenv_error(config, XRT_DOTENV_LIMIT, 0, "dotenv input exceeds byte limit");
		return false;
	}
	if (memchr(text, '\0', size) != NULL) {
		dotenv_error(config, XRT_DOTENV_ARGUMENT, 0, "dotenv input contains NUL or is null");
		return false;
	}
	if (!xrtUtf8Valid(xrtStrViewN(text, size), NULL)) {
		dotenv_error(config, XRT_DOTENV_FORMAT, 0, "dotenv input is not UTF-8");
		return false;
	}
	if (size >= 3 && memcmp(text, "\xef\xbb\xbf", 3) == 0) offset = 3;
	if (!dotenv_init_items(&parsed)) {
		dotenv_error(config, XRT_DOTENV_MEMORY, 0, "cannot initialize dotenv entries");
		return false;
	}
	while (offset < size) {
		size_t end = offset;
		while (end < size && text[end] != '\n') {
			/* CR 只属于 CRLF；孤立回车不能藏进配置值。 */
			if (text[end] == '\r' &&
				(end + 1u == size || text[end + 1u] != '\n')) {
				dotenv_error(config, XRT_DOTENV_FORMAT, line,
					"invalid dotenv line ending at line %zu", line);
				xrtMapUnit(&parsed.Items);
				return false;
			}
			++end;
		}
		if (end > offset && text[end - 1] == '\r') --end;
		if (!dotenv_line(&parsed, text + offset, text + end, line)) {
			config->ErrorCode = parsed.ErrorCode;
			config->ErrorLine = parsed.ErrorLine;
			memcpy(config->ErrorText, parsed.ErrorText, sizeof(config->ErrorText));
			xrtMapUnit(&parsed.Items);
			return false;
		}
		offset = end;
		while (offset < size && text[offset] != '\n') ++offset;
		if (offset < size) ++offset;
		++line;
	}
	xrtMapUnit(&config->Items);
	config->Items = parsed.Items;
	return true;
}

bool xrtDotenvLoad(xrtdotenv* config, const char* path, size_t maxBytes)
{
	bytes input;
	size_t size = 0;
	bool result;
	xrtClearError();
	if (config == NULL) return false;
	dotenv_clear_error(config);
	if (path == NULL || path[0] == '\0') {
		dotenv_error(config, XRT_DOTENV_ARGUMENT, 0, "dotenv path is empty");
		return false;
	}
	input = xrtFileReadAllLimit(path, maxBytes, &size);
	if (input == NULL) {
		const xerror* error = xrtGetError();
		const char* message = error != NULL ? xrtErrorMessage(error) : NULL;
		int code = XRT_DOTENV_IO;
		if (error != NULL && xrtErrorKind(error) == XERR_MEMORY)
			code = XRT_DOTENV_MEMORY;
		else if (error != NULL && xrtErrorKind(error) == XERR_RANGE &&
			xrtErrorCode(error) == XFILE_ERROR_LIMIT)
			code = XRT_DOTENV_LIMIT;
		dotenv_error(config, code, 0, "cannot read dotenv file: %s",
			message != NULL ? message : path);
		return false;
	}
	result = xrtDotenvParse(config, (const char*)input, size, maxBytes);
	xrtFree(input);
	return result;
}

/* ---------- 显式导入进程环境 ---------- */

bool xrtDotenvApply(xrtdotenv* config, bool overwrite)
{
	xmapiter iterator;
	xrt_dotenv_pair* pair;
	xrtClearError();
	if (config == NULL) return false;
	dotenv_clear_error(config);
	if (!xrtMapIterBegin(&config->Items, &iterator)) {
		dotenv_error(config, XRT_DOTENV_ENVIRONMENT, 0,
			"cannot iterate dotenv entries");
		return false;
	}
	while ((pair = (xrt_dotenv_pair*)xrtMapIterNext(&iterator, NULL)) != NULL) {
		str current = NULL;
		if (!overwrite && !xrtEnvLookup(pair->Key, &current)) {
			const xerror* error = xrtGetError();
			int code = error != NULL && xrtErrorKind(error) == XERR_MEMORY ?
				XRT_DOTENV_MEMORY : XRT_DOTENV_ENVIRONMENT;
			xrtMapIterEnd(&iterator);
			dotenv_error(config, code, 0,
				"cannot read process environment");
			return false;
		}
		if (current != NULL) { xrtFree(current); continue; }
		if (!xrtEnvSet(pair->Key, pair->Value)) {
			const xerror* error = xrtGetError();
			int code = error != NULL && xrtErrorKind(error) == XERR_MEMORY ?
				XRT_DOTENV_MEMORY : XRT_DOTENV_ENVIRONMENT;
			xrtMapIterEnd(&iterator);
			dotenv_error(config, code, 0,
				"cannot set process environment: %s", pair->Key);
			return false;
		}
	}
	xrtMapIterEnd(&iterator);
	return true;
}
