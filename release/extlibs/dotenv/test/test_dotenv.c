#include "../include/xrt_dotenv.h"
#include "xrt.h"

#include <stdio.h>
#include <string.h>

static int check(int condition, const char* message)
{
	if (condition) return 1;
	fprintf(stderr, "test_dotenv: %s\n", message);
	return 0;
}

static int test_create_failure(void)
{
	unsigned failure;
	int saw_failure = 0, saw_success = 0;
	for (failure = 0; failure < 16; ++failure) {
		xrtdotenv* config;
		int triggered, ok;
		if (!check(xrtMemDebugFailAfter(failure), "cannot inject create failure"))
			return 0;
		config = xrtDotenvCreate();
		triggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if (triggered) {
			const xerror* error = xrtGetError();
			saw_failure = 1;
			ok = check(config == NULL && error != NULL &&
				xrtErrorKind(error) == XERR_MEMORY,
				"create allocation failure lost its memory error");
		} else {
			saw_success = 1;
			ok = check(config != NULL && xrtGetError() == NULL,
				"create failed without an allocation fault");
		}
		xrtDotenvDestroy(config);
		xrtClearError();
		if (!ok) return 0;
	}
	return check(saw_failure && saw_success,
		"create allocation scan missed failure or success");
}

static int test_allocation_transactions(void)
{
	const char* source =
		"A=1\nB=2\nC=3\nD=4\nE=5\nF=6\nG=7\nH=8\nI=9\n"
		"J=10\nK=11\nL=12\nM=13\nN=14\nO=15\nP=16\nQ=17\n";
	unsigned failure;
	int ok = 1;
	int saw_failure = 0, saw_success = 0;

	/* 覆盖局部配置的首次分配、键值复制和映射桶扩容。 */
	for (failure = 0; failure < 96; ++failure) {
		xrtdotenv* config = xrtDotenvCreate();
		int parsed, triggered;
		const char* old;
		const char* last;
		ok &= check(config != NULL && xrtDotenvSet(config, "ORIGINAL", "keep"),
			"cannot prepare parse allocation test");
		if (!ok) { xrtDotenvDestroy(config); return 0; }
		ok &= check(xrtMemDebugFailAfter(failure), "cannot inject parse failure");
		parsed = xrtDotenvParse(config, source, strlen(source), 4096u);
		triggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		old = xrtDotenvGet(config, "ORIGINAL");
		last = xrtDotenvGet(config, "Q");
		if (triggered) {
			saw_failure = 1;
			int recovered;
			const char* after;
			ok &= check(!parsed && old != NULL && strcmp(old, "keep") == 0 &&
				last == NULL && xrtDotenvCount(config) == 1u &&
				xrtDotenvErrorCode(config) == XRT_DOTENV_MEMORY,
				"parse allocation failure changed the old configuration");
			recovered = xrtDotenvParse(config, "AFTER=ok\n", 9u, 4096u);
			after = xrtDotenvGet(config, "AFTER");
			ok &= check(recovered && xrtDotenvCount(config) == 1u &&
				after != NULL && strcmp(after, "ok") == 0,
				"parse could not recover after allocation failure");
		} else {
			saw_success = 1;
			ok &= check(parsed && old == NULL && last != NULL &&
				xrtDotenvCount(config) == 17u && strcmp(last, "17") == 0,
				"parse did not recover after allocation fault range");
		}
		xrtDotenvDestroy(config);
		if (!ok) return 0;
	}
	ok &= check(saw_failure && saw_success,
		"parse fault scan did not cover both outcomes");

	/* 更新已有键时，任一分配失败都不能提前替换旧值。 */
	for (failure = 0; failure < 4; ++failure) {
		xrtdotenv* config = xrtDotenvCreate();
		int updated, triggered;
		const char* value;
		ok &= check(config != NULL && xrtDotenvSet(config, "KEY", "old"),
			"cannot prepare set allocation test");
		if (!ok) { xrtDotenvDestroy(config); return 0; }
		ok &= check(xrtMemDebugFailAfter(failure), "cannot inject set failure");
		updated = xrtDotenvSet(config, "KEY", "new");
		triggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		value = xrtDotenvGet(config, "KEY");
		ok &= check(value != NULL && strcmp(value, triggered ? "old" : "new") == 0 &&
			xrtDotenvCount(config) == 1u && updated == !triggered &&
			(!triggered || xrtDotenvErrorCode(config) == XRT_DOTENV_MEMORY),
			"set allocation failure changed the old value");
		if (triggered) {
			int recovered = xrtDotenvSet(config, "KEY", "new");
			const char* after = xrtDotenvGet(config, "KEY");
			ok &= check(recovered && after != NULL && strcmp(after, "new") == 0,
				"set could not recover after allocation failure");
		}
		xrtDotenvDestroy(config);
		if (!ok) return 0;
	}
	return ok;
}

static int test_error_recovery(void)
{
	xrtdotenv* config;
	int ok;
	xrtSetErrorInfo(XERR_IO, "xlang.extlib.dotenv.test", 0, "prior error");
	config = xrtDotenvCreate();
	ok = check(config != NULL && xrtGetError() == NULL,
		"successful create kept previous XRT error");
	if (!ok) return 0;
	ok &= check(!xrtDotenvLoad(config, "test/data/missing-dotenv-file.env", 4096u) &&
		xrtGetError() != NULL, "missing file did not set XRT error");
	ok &= check(xrtDotenvErrorCode(config) == XRT_DOTENV_IO &&
		xrtDotenvErrorText(config) != NULL && xrtGetError() != NULL,
		"error inspection erased the original failure");
	ok &= check(xrtDotenvParse(config, "A=1\n", 4u, 4096u) &&
		xrtGetError() == NULL, "successful parse kept previous XRT error");
	ok &= check(!xrtDotenvLoad(config, "test/data/missing-dotenv-file.env", 4096u) &&
		xrtGetError() != NULL, "second missing file did not set XRT error");
	ok &= check(xrtDotenvSet(config, "A", "2") &&
		xrtGetError() == NULL, "successful set kept previous XRT error");
	ok &= check(!xrtDotenvLoad(config, "test/data/missing-dotenv-file.env", 4096u) &&
		xrtGetError() != NULL, "third missing file did not set XRT error");
	ok &= check(xrtDotenvGet(config, "A") != NULL &&
		xrtGetError() == NULL, "successful get kept previous XRT error");
	ok &= check(!xrtDotenvLoad(config, "test/data/missing-dotenv-file.env", 4096u) &&
		xrtGetError() != NULL, "fourth missing file did not set XRT error");
	ok &= check(xrtDotenvHas(config, "A") &&
		xrtGetError() == NULL, "successful has kept previous XRT error");
	ok &= check(!xrtDotenvLoad(config, "test/data/missing-dotenv-file.env", 4096u) &&
		xrtGetError() != NULL, "fifth missing file did not set XRT error");
	ok &= check(xrtDotenvCount(config) == 1u &&
		xrtGetError() == NULL, "successful count kept previous XRT error");
	ok &= check(!xrtDotenvLoad(config, "test/data/missing-dotenv-file.env", 4096u) &&
		xrtGetError() != NULL, "sixth missing file did not set XRT error");
	xrtDotenvClear(config);
	ok &= check(xrtGetError() == NULL && xrtDotenvCount(config) == 0u,
		"clear kept previous XRT error");
	ok &= check(!xrtDotenvLoad(config, "test/data/missing-dotenv-file.env", 4096u) &&
		xrtGetError() != NULL, "seventh missing file did not set XRT error");
	ok &= check(xrtDotenvApply(config, false) &&
		xrtGetError() == NULL, "successful apply kept previous XRT error");
	ok &= check(!xrtDotenvLoad(config, "test/data/missing-dotenv-file.env", 4096u) &&
		xrtGetError() != NULL, "eighth missing file did not set XRT error");
	ok &= check(xrtDotenvLoad(config, "test/data/dotenv_test.env", 4096u) &&
		xrtGetError() == NULL, "successful load kept previous XRT error");
	xrtSetErrorInfo(XERR_IO, "xlang.extlib.dotenv.test", 0, "prior error");
	xrtDotenvDestroy(config);
	ok &= check(xrtGetError() != NULL,
		"destroy erased the caller's error");
	xrtClearError();
	return ok;
}

static int test_parse(void)
{
	const char source[] =
		"\xef\xbb\xbf# comment\r\nexport NAME=first\r\nNAME=second\r\n"
		"EMPTY=\r\nTEXT=\"hello\\nworld\" # note\r\nRAW='a#b'\r\n"
		"UTF8=你好\r\n";
	xrtdotenv* config = xrtDotenvCreate();
	int ok = check(config != NULL, "allocation failed");
	if (!ok) return 0;
	ok &= check(xrtDotenvParse(config, source, sizeof(source) - 1u, 4096u),
		xrtDotenvErrorText(config));
	ok &= check(xrtDotenvCount(config) == 5u, "wrong count");
	ok &= check(strcmp(xrtDotenvGet(config, "NAME"), "second") == 0,
		"duplicate key must take the last value");
	ok &= check(strcmp(xrtDotenvGet(config, "EMPTY"), "") == 0,
		"empty value lost");
	ok &= check(strcmp(xrtDotenvGet(config, "TEXT"), "hello\nworld") == 0,
		"quoted escape failed");
	ok &= check(strcmp(xrtDotenvGet(config, "RAW"), "a#b") == 0,
		"single quote should preserve hash");
	ok &= check(strcmp(xrtDotenvGet(config, "UTF8"), "你好") == 0,
		"UTF-8 value failed");
	ok &= check(!xrtDotenvHas(config, "MISSING") &&
		xrtDotenvGet(config, "MISSING") == NULL, "missing key contract failed");
	{
		const char escaped[] = "A=\"one\\tline\\nnext\\rreturn\\\"quote\\\\slash\"\nRAW='\\q'\n";
		const char invalid[] = "A=\"bad\\q\"\n";
		ok &= check(xrtDotenvParse(config, escaped, sizeof(escaped) - 1u, 4096u) &&
			strcmp(xrtDotenvGet(config, "A"), "one\tline\nnext\rreturn\"quote\\slash") == 0 &&
			strcmp(xrtDotenvGet(config, "RAW"), "\\q") == 0,
			"quoted escape and raw single-quote semantics changed");
		ok &= check(!xrtDotenvParse(config, invalid, sizeof(invalid) - 1u, 4096u) &&
			xrtDotenvErrorCode(config) == XRT_DOTENV_FORMAT &&
			xrtDotenvErrorLine(config) == 1u &&
			strcmp(xrtDotenvGet(config, "A"), "one\tline\nnext\rreturn\"quote\\slash") == 0,
			"invalid quoted escape must preserve previous config");
		ok &= check(xrtDotenvParse(config, source, sizeof(source) - 1u, 4096u),
			"source must remain usable after invalid escape");
	}

	/* 坏输入不能覆盖已有配置。 */
	ok &= check(!xrtDotenvParse(config, "GOOD=1\nbad-key=2\n", 17u, 4096u) &&
		xrtDotenvErrorCode(config) == XRT_DOTENV_FORMAT &&
		xrtDotenvErrorLine(config) == 2u &&
		strcmp(xrtDotenvGet(config, "NAME"), "second") == 0,
		"malformed input must preserve previous config");
	{
		const char bare_cr[] = "A=1\rB=2\n";
		const char eof_cr[] = "A=1\r";
		ok &= check(!xrtDotenvParse(config, bare_cr,
			sizeof(bare_cr) - 1u, 4096u) &&
			xrtDotenvErrorCode(config) == XRT_DOTENV_FORMAT &&
			xrtDotenvErrorLine(config) == 1u &&
			strcmp(xrtDotenvGet(config, "NAME"), "second") == 0,
			"bare CR must not join two dotenv assignments");
		ok &= check(!xrtDotenvParse(config, eof_cr,
			sizeof(eof_cr) - 1u, 4096u) &&
			xrtDotenvErrorCode(config) == XRT_DOTENV_FORMAT &&
			xrtDotenvCount(config) == 5u,
			"bare CR at EOF must not be treated as CRLF");
	}
	ok &= check(!xrtDotenvParse(config, "A=123", 5u, 2u) &&
		xrtDotenvErrorCode(config) == XRT_DOTENV_LIMIT,
		"input limit not enforced");
	ok &= check(!xrtDotenvParse(config, "A=1", 3u, 0u) &&
		xrtDotenvErrorCode(config) == XRT_DOTENV_LIMIT &&
		xrtDotenvCount(config) == 5u,
		"zero byte limit must preserve previous config");
	ok &= check(!xrtDotenvParse(config, "A\0B", 3u, 1u) &&
		xrtDotenvErrorCode(config) == XRT_DOTENV_LIMIT &&
		xrtDotenvCount(config) == 5u,
		"oversized input must be rejected before inspecting its bytes");
	ok &= check(!xrtDotenvParse(config, "A=x\0B=y", 7u, 4096u) &&
		xrtDotenvErrorCode(config) == XRT_DOTENV_ARGUMENT,
		"embedded NUL not rejected");
	{
		const char invalidUtf8[] = "A=\xff\n";
		ok &= check(!xrtDotenvParse(config, invalidUtf8,
			sizeof(invalidUtf8) - 1u, 4096u) &&
			xrtDotenvErrorCode(config) == XRT_DOTENV_FORMAT &&
			xrtDotenvCount(config) == 5u,
			"invalid UTF-8 changed configuration");
	}
	ok &= check(!xrtDotenvParse(config, "A=\"unterminated\n", 16u, 4096u) &&
		xrtDotenvErrorCode(config) == XRT_DOTENV_FORMAT,
		"unclosed quote was accepted");
	ok &= check(xrtDotenvParse(config, "A=hello # note\n", 15u, 4096u) &&
		strcmp(xrtDotenvGet(config, "A"), "hello") == 0,
		"unquoted inline comment failed");
	ok &= check(!xrtDotenvSet(config, "bad-key", "x") &&
		xrtDotenvCount(config) == 1u, "invalid set changed config");
	ok &= check(xrtDotenvSet(config, "NAME", "third") &&
		strcmp(xrtDotenvGet(config, "NAME"), "third") == 0,
		"set after error failed");
	/* 空文件与只有 BOM 的文件都是合法空配置，且应整体替换旧条目。 */
	ok &= check(xrtDotenvParse(config, "", 0u, 0u) &&
		xrtDotenvCount(config) == 0u &&
		xrtDotenvGet(config, "NAME") == NULL &&
		xrtDotenvErrorCode(config) == XRT_DOTENV_OK,
		"empty input did not clear previous config");
	ok &= check(xrtDotenvSet(config, "AFTER", "ok") &&
		xrtDotenvParse(config, "\xef\xbb\xbf", 3u, 3u) &&
		xrtDotenvCount(config) == 0u &&
		xrtDotenvGet(config, "AFTER") == NULL,
		"BOM-only input did not clear previous config");
	xrtDotenvDestroy(config);
	return ok;
}

/* 从合法配置变异，验证失败解析始终保留旧配置。 */
static int test_parse_mutations(void)
{
	static const char source[] =
		"A=\"line\\ntext\" # note\nB='你好#x'\nC=value # tail\n";
	static const unsigned char replacements[] = {
		0, '\r', '\n', '=', '#', '\'', '"', '\\', 0x7f, 0xff
	};
	char changed[sizeof(source) - 1u];
	size_t size, position, replacement;

	for (size = 0; size <= sizeof(source) - 1u; ++size) {
		xrtdotenv* config = xrtDotenvCreate();
		int parsed, ok;
		if (!check(config != NULL && xrtDotenvSet(config, "GUARD", "keep"),
			"cannot prepare truncated dotenv case")) {
			xrtDotenvDestroy(config);
			return 0;
		}
		parsed = xrtDotenvParse(config, source, size, sizeof(source));
		ok = parsed ? xrtDotenvErrorCode(config) == XRT_DOTENV_OK &&
			xrtGetError() == NULL && !xrtDotenvHas(config, "GUARD") :
			xrtDotenvErrorCode(config) != XRT_DOTENV_OK &&
			xrtDotenvCount(config) == 1u &&
			xrtDotenvGet(config, "GUARD") != NULL &&
			strcmp(xrtDotenvGet(config, "GUARD"), "keep") == 0;
		xrtDotenvDestroy(config);
		if (!ok) {
			fprintf(stderr, "dotenv truncation failed at %zu\n", size);
			return 0;
		}
	}

	for (position = 0; position < sizeof(changed); ++position) {
		for (replacement = 0; replacement < sizeof(replacements); ++replacement) {
			xrtdotenv* config = xrtDotenvCreate();
			int parsed, ok;
			if (!check(config != NULL && xrtDotenvSet(config, "GUARD", "keep"),
				"cannot prepare mutated dotenv case")) {
				xrtDotenvDestroy(config);
				return 0;
			}
			memcpy(changed, source, sizeof(changed));
			changed[position] = (char)replacements[replacement];
			parsed = xrtDotenvParse(config, changed, sizeof(changed),
				sizeof(changed));
			ok = parsed ? xrtDotenvErrorCode(config) == XRT_DOTENV_OK &&
				xrtGetError() == NULL && !xrtDotenvHas(config, "GUARD") :
				xrtDotenvErrorCode(config) != XRT_DOTENV_OK &&
				xrtDotenvCount(config) == 1u &&
				xrtDotenvGet(config, "GUARD") != NULL &&
				strcmp(xrtDotenvGet(config, "GUARD"), "keep") == 0;
			xrtDotenvDestroy(config);
			if (!ok) {
				fprintf(stderr, "dotenv mutation failed at %zu/%zu\n",
					position, replacement);
				return 0;
			}
		}
	}
	return 1;
}

static int test_file_and_environment(void)
{
	const char* key = "XLANG_DOTENV_C_TEST_ONLY";
	xrtdotenv* config = xrtDotenvCreate();
	str previous = NULL;
	int ok = check(config != NULL, "allocation failed");
	if (!ok) return 0;
	ok &= check(xrtDotenvLoad(config, "test/data/dotenv_test.env", 4096u) &&
		xrtDotenvCount(config) == 7u &&
		strcmp(xrtDotenvGet(config, "DB_HOST"), "localhost") == 0,
		"file load failed");
	ok &= check(!xrtDotenvLoad(config, "test/data/missing-dotenv-file.env", 4096u) &&
		xrtDotenvErrorCode(config) == XRT_DOTENV_IO &&
		xrtDotenvCount(config) == 7u,
		"failed load must preserve previous config");
	xrtClearError();
	ok &= check(!xrtDotenvLoad(config, "test/data/dotenv_test.env", 4u) &&
		xrtDotenvErrorCode(config) == XRT_DOTENV_LIMIT &&
		xrtDotenvCount(config) == 7u,
		"file byte limit must preserve previous config");
	ok &= check(!xrtDotenvLoad(config, "test/data/dotenv_test.env", 0u) &&
		xrtDotenvErrorCode(config) == XRT_DOTENV_LIMIT &&
		xrtDotenvCount(config) == 7u,
		"zero file byte limit must preserve previous config");
	xrtClearError();
	ok &= check(xrtEnvLookup(key, &previous), "environment lookup failed");
	if (ok) {
		xrtEnvSet(key, "");
		ok &= check(xrtDotenvSet(config, key, "new") &&
			xrtDotenvApply(config, false), "apply without overwrite failed");
		{
			str value = xrtEnvGet(key);
			ok &= check(value != NULL && strcmp(value, "") == 0,
				"apply changed existing empty process value");
			xrtFree(value);
		}
		ok &= check(xrtDotenvApply(config, true), "apply overwrite failed");
		{
			str value = xrtEnvGet(key);
			ok &= check(value != NULL && strcmp(value, "new") == 0,
				"apply did not overwrite process value");
			xrtFree(value);
		}
		ok &= check(xrtDotenvSet(config, key, "你好") &&
			xrtDotenvApply(config, true), "UTF-8 environment apply failed");
		{
			str value = xrtEnvGet(key);
			ok &= check(value != NULL && strcmp(value, "你好") == 0,
				"UTF-8 environment value changed during apply");
			xrtFree(value);
		}
		if (previous != NULL) xrtEnvSet(key, previous);
		else xrtEnvRemove(key);
	}
	xrtFree(previous);
	xrtDotenvDestroy(config);
	return ok;
}

static int test_apply_memory_error(void)
{
	const char* key = "XLANG_DOTENV_APPLY_OOM_TEST";
	xrtdotenv* config = xrtDotenvCreate();
	str previous = NULL;
	str current;
	int ok = check(config != NULL && xrtEnvLookup(key, &previous),
		"cannot prepare environment allocation test");
	if (!ok) { xrtDotenvDestroy(config); return 0; }
	ok &= check(xrtEnvSet(key, "existing") &&
		xrtDotenvSet(config, key, "replacement"),
		"cannot set up environment allocation test");
	if (ok) {
		int applied, triggered;
		ok &= check(xrtMemDebugFailAfter(0), "cannot inject environment lookup failure");
		applied = xrtDotenvApply(config, false);
		triggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		ok &= check(triggered && !applied &&
			xrtDotenvErrorCode(config) == XRT_DOTENV_MEMORY &&
			xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY,
			"environment allocation failure lost its memory error");
		current = xrtEnvGet(key);
		ok &= check(current != NULL && strcmp(current, "existing") == 0,
			"failed apply changed the process environment");
		xrtFree(current);
		ok &= check(xrtDotenvApply(config, true), "apply did not recover after OOM");
		current = xrtEnvGet(key);
		ok &= check(current != NULL && strcmp(current, "replacement") == 0,
			"recovered apply did not set the value");
		xrtFree(current);
	}
	if (previous != NULL) xrtEnvSet(key, previous);
	else xrtEnvRemove(key);
	xrtFree(previous);
	xrtDotenvDestroy(config);
	return ok;
}

int main(void)
{
	int ok = test_create_failure();
	ok &= test_allocation_transactions();
	ok &= test_error_recovery();
	ok &= test_parse();
	ok &= test_parse_mutations();
	ok &= test_file_and_environment();
	ok &= test_apply_memory_error();
	if (ok) puts("test_dotenv: PASS");
	return ok ? 0 : 1;
}
